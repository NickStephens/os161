#include <types.h>
#include <lib.h>
#include <kern/unistd.h>
#include <thread.h>
#include <curthread.h>
#include <vnode.h>
#include <vfs.h>
#include <vm.h>
#include <uio.h>
#include <pagetable.h>
#include <swap.h>

int swapsize;
struct vnode *swap;
off_t swap_offset;
struct swapentry *swapped;
struct lock *swapped_lock;

void
swap_bootstrap()
{
	int i;
	int result;

	swapsize = 73 * 2;
	swap_offset = 0;
	result = vfs_open("./swap", O_RDWR | O_CREAT, &swap);
	if (result)
		panic("[swap_bootstrap]: can't open swapfile %d\n", result);

	swapped = kmalloc(swapsize * sizeof(struct swapentry));
	if (swapped==NULL)
		panic("[swap_bootstrap]: can't allocate memory for swapped array\n");

	swapped_lock = lock_create("swapped_lock");
	if (swapped_lock==NULL)
		panic("[swap_bootstrap]: can't allocate memory for swapped_lock\n");

	for (i=0;i<swapsize;i++)
		swapped[i].valid = 0;

	kprintf("swapspace initialized\n");
}

static
int
findfreeswapped()
{
	int i;

	for(i=0;i<swapsize;i++)
		if (!swapped[i].valid)
			break;

	return i;
}

int
getswap(vaddr_t page, pid_t pid)
{
	int i;

	for (i=0;i<swapsize;i++)
		if (swapped[i].valid)
			if ((swapped[i].addr==page)&&(swapped[i].owner==pid))
				return i;

	return -1;
}

int
swapout(vaddr_t page, pid_t pid, const void *content, int read, int write, int execute)
{
	struct uio ku;
	int result;
	int swap_index;

	swap_index = findfreeswapped();
	//kprintf("[swapout] pid %d, free swap %d/%d\n", curthread->t_pid, swap_index, swapsize);
	if (swap_index==swapsize)
		panic("[swapout]: swapped array full. system out of memory\n");


	if (content!=NULL)
	{
		mk_kuio(&ku, content, PAGE_SIZE, swap_index * PAGE_SIZE, UIO_WRITE);		
		result = VOP_WRITE(swap, &ku);

		if (result)
			return -result;
	} 
	else
	{
		/* write zeros out to the page */
	}

	swapped[swap_index].valid = 1;
	swapped[swap_index].addr  = page;
	swapped[swap_index].owner = pid;
	if (read)
		swapped[swap_index].perms |= R_B;
	if (write)
		swapped[swap_index].perms |= W_B;
	if (execute)
		swapped[swap_index].perms |= X_B;

	return 0;
}

int
swapin(int index, vaddr_t page, pid_t pid)
{
	struct pte *rpte;
	struct swapentry *swap_page;
	struct uio ku;
	int swap_index;
	int result;

	//kprintf("[swapin] pid %d page (%08x)\n", curthread->t_pid, page);
	/* should never fail */
	swap_index = getswap(page, pid);
	if (swap_index==-1)
		panic("[swapin]: invoked with a bad page (%08x) and pid (%d)\n", page, pid);

	swap_page = &swapped[swap_index];
	rpte = &pagetable[index];

	/* setup new pte */

	rpte->control &= ~(R_B | W_B | X_B);
	if (swap_page->perms & R_B)
		rpte->control |= R_B;
	if (swap_page->perms & W_B)
		rpte->control |= W_B;
	if (swap_page->perms & X_B)
		rpte->control |= X_B;

	rpte->page     = swap_page->addr;
	rpte->owner    = swap_page->owner;
	rpte->control |= VALID_B; 

	if (rpte->prev != -1)
		pagetable[rpte->prev].next = rpte->next;
	if (rpte->next != -1)
		pagetable[rpte->next].prev = rpte->prev;
	//rpte->next     = -1;


	/* turn off swapped entry */
	swap_page->valid = 0;

	/* transfer the page */
	mk_kuio(&ku, PADDR_TO_KVADDR(FRAME(index)), PAGE_SIZE, 
			swap_index * PAGE_SIZE, UIO_READ);

	result = VOP_READ(swap, &ku);

	if (result)
	{
		return -result;
	}

	return 0;
}