#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <array.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <mmap.h>
#include <pagetable.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */


/* an addrspace will contain
 * an array of vaddrs and the number of pages
 * associated with each vaddr */

/* A typical memory use case: (from execv)
 * - a program starts up and calls as_create
 * - as_create allocates an array for vaddrs
 * - a program calls as_activate, this puts mappings
 *   in the pagetable 
 *   (when we get to vm we'll also have to make decisions
 *   about how we swap out programs, in fact we might not
 *   even put anything thing in the pagetable right away
 *   we might just immediately go to disk)
 * - a program calls as_define_stack, we assign a 
 *   a random stack address and put the addr in our
 *   address space array
 */

/* A typical memory use case: (from fork)
 * - a program starts up and calls as_create
 * - a program calls as_copy
 *   this has to immediately read off all the 
 *   vaddrs from the target address space, resolve 
 *   those and do a memcpy, we should probably
 *   write a nice pagetable function to handle all of this */

struct addrspace *
as_create(void)
{

	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */

	as->pages = array_create();
	if (as->pages==NULL)
	{
		kfree(as);
		return NULL;
	}

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret, pid_t pid)
{
	struct addrspace *newas;
	struct page *newpage, *page;
	int i;
	int nindex, oindex;
	paddr_t nf, of;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	for(i=0;i<array_getnum(old->pages);i++)
	{
		newpage = (struct page *) kmalloc(sizeof(struct page));
		if (newpage==NULL)
			return ENOMEM;
		page = (struct page *) array_getguy(old->pages, i);
		memcpy(newpage, page, sizeof(struct page));
		array_add(newas->pages, newpage);

		oindex = getindex(newpage->vaddr);
		of = FRAME(oindex);

		nindex = addpage(newpage->vaddr,
				pid, 
				newpage->perms & P_R_B,
				newpage->perms & P_W_B,
				newpage->perms & P_X_B,
				PADDR_TO_KVADDR(of));


	}

	*ret = newas;
	return 0;
}
 
/* frees pages */
/* hashes all pages belonging to process */
/* calls md_freetld */
void
as_destroy(struct addrspace *as)
{
	struct page *p;
	int i;

	for(i=0;i<array_getnum(as->pages);i++)
	{
		p = (struct page *) array_getguy(as->pages, i);
		invalidatepage(p->vaddr);
		kfree(p);
	}

	invalidateswapentries(curthread->t_pid);

	array_destroy(as->pages);	
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	/* write out to tlb */
	/* make sure tlblo contains the current pid 
	 * within the asid field */

	md_cacheflush();

	//md_loadprocid();

	(void)as;  // suppress warning until code gets written
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	struct page *p;
	size_t curpage;
	size_t npages;

	/* make entries for each page */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	for (curpage=0;curpage<npages;curpage++)
	{
		p = (struct page *) kmalloc(sizeof(struct page));
		if (p==NULL)
			return ENOMEM;
		p->vaddr = vaddr + curpage * PAGE_SIZE;
		p->perms = 0;
		if (readable)
			p->perms |= P_R_B;
		if (writeable)
			p->perms |= P_W_B;
		if (executable)
			p->perms |= P_X_B;
		array_add(as->pages, p);
	}

	return 0;
}

/* place all entries into the pagetable */
int
as_prepare_load(struct addrspace *as)
{
	struct page *p;
	int i;
	int num = array_getnum(as->pages);

	for (i=0;i<num;i++)
	{
		p = (struct page *) array_getguy(as->pages, i);

		addpage(p->vaddr, curthread->t_pid, 1, 1, 1, NULL); // enable all permission for writing page in
	}


	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	int i;
	struct page *p;
	int num = array_getnum(as->pages);

	/* update permissions on all pages */
	for(i=0;i<num;i++)
	{
		p = (struct page *) array_getguy(as->pages, i);
		sys_mprotect(p->vaddr, PAGE_SIZE,
				(p->perms & P_R_B ? PROT_READ : 0)
			      | (p->perms & P_W_B ? PROT_WRITE : 0)
			      | (p->perms & P_X_B ? PROT_EXEC : 0));
	}

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	struct page *p;
	size_t npages;
	size_t curpage;
	vaddr_t stacktop;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	npages = (size_t) STACKSIZE;
	stacktop = *stackptr - PAGE_SIZE * npages;

	for(curpage=0;curpage<npages;curpage++)
	{
		p = (struct page *) kmalloc(sizeof(struct page));
		if (p==NULL)
			return ENOMEM;

		p->vaddr = stacktop + curpage * PAGE_SIZE;
		p->perms = P_R_B | P_W_B;
		array_add(as->pages, p);


		addpage(p->vaddr, curthread->t_pid, p->perms & P_R_B, 
			p->perms & P_W_B, p->perms & P_X_B, NULL);
	}

	return 0;
}

void
addrspace_dump(struct addrspace *as)
{
	struct page *p;
	int num;
	int i;

	num = array_getnum(as->pages);

	kprintf("+-ADDRSPACE------+\n");
	for(i=0;i<num;i++)
	{
		p = (struct page *) array_getguy(as->pages, i);
		kprintf("| %08x | %c%c%c |\n", p->vaddr,
			p->perms & P_R_B ? 'r' : '-', 
			p->perms & P_W_B ? 'w' : '-',
			p->perms & P_X_B ? 'x' : '-');
	}
}
