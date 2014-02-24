#include <types.h>
#include <lib.h>
#include <synch.h>
#include <machine/vm.h>
#include <thread.h>
#include <curthread.h>
#include <pagetable.h>

int pagetable_initialized;

void
pagetable_bootstrap(void)
{
	int i;
	u_int32_t lo, hi;
	u_int32_t total;
	u_int32_t frames;
	u_int32_t pframes;
	u_int32_t pteposs;

	ram_getsize(&lo, &hi);
	total = hi - lo;

	/* calculate the number of frames */
	frames = total / PAGE_SIZE;

	/* how many ptes can we fit in a frame? */
	pteposs = PAGE_SIZE / sizeof(struct pte);

	pframes = 1;
	frames--;
	while((pteposs*pframes) < frames)
	{
		pframes++;
		frames--;
	}

	kprintf("pagetable_bootstrap: pagetable initialing with:\n");
	kprintf("available number of PTEs per frame: %d\n", pteposs*pframes);
	kprintf("number of frames occupied by pagetable: %d\n", pframes);
	kprintf("number of frames available: %d\n", frames);

	/* if this happens our while loop went wild */
	assert(frames!=0);

	/* lock our pagetable into its frame*/
	pagetable = (struct pte *) PADDR_TO_KVADDR(ram_stealmem(pframes));
	kprintf("pagetable initialized at 0x%08x\n", pagetable);

	bframe = lo + PAGE_SIZE * pframes;
	kprintf("bottom frame: 0x%08x\n", bframe);

	pagetable_size = frames;

	/* invalidate all the pagetable entries */
	for(i=0;((u_int32_t) i)<pagetable_size;i++)
		pagetable[i].control = 0;

	pagetable_lock = lock_create("pagetable_lock");
	if (pagetable_lock==NULL)
		panic("pagetable_bootstrap: unable to initialize pagetable_lock\n");

	pagetable_initialized = 1;

}

/* define alloc_kpages (malloc) here for the time being */
/* kernel pages are a special case. since they will never 
 * be asked to be resolve by mips there only real presence
 * is to tell userland that a frame is occupied */
vaddr_t
alloc_kpages(int npages)
{
	u_int32_t i;
	paddr_t free;
	int j;

	if (pagetable_initialized)
	{
		free = 0;
		for(i=0;i<pagetable_size;i++)
		{
			kprintf("page [%d] validity: %d\n", i, PTE_VALID(pagetable[i])); 
			if (!PTE_VALID(pagetable[i]))
			{
				for(j=1;j<npages;j++)
				{
					if (PTE_VALID(pagetable[i+j]))
						break;
				}
				if (j==npages)
				{
					pagetable[i].control = VALID_B | SUPER_B;
					free = FRAME(i);
					break;
				}
			}
		}
	}
	else
	{
		free = ram_stealmem(npages);
	}

	return PADDR_TO_KVADDR(free);
}

/* for now we just invalidate one page. This won't work if more than one
 * page was originally allocated */
void
free_kpages(vaddr_t page)
{
	if (pagetable_initialized)
		invalidatepage(page);
}

void
addpage(vaddr_t page, int read, int write, int execute)
{
	int index; 
	struct pte *pre, *cur;

	index = hash(page);

	lock_acquire(pagetable_lock);
	pre = NULL;
	cur = &pagetable[index];

	/* XXX possible this loops forever */
	while (cur->control & VALID_B)
	{
		kprintf("addpage: encountered valid page index %d\n", index);
		pre = cur;
		if (cur->next==-1)
			index = (index*index) % pagetable_size;
		else
			index = cur->next;
		cur = &pagetable[index];
	}

	if (pre!=NULL)
		pre->next = index;

	cur->page    = page;
	cur->owner   = curthread->t_pid;
	cur->control = 0;
	
	if (read)	
		cur->control |= R_B;
	if (write)
		cur->control |= W_B;
	if (execute)
		cur->control |= X_B;

	cur->control |= VALID_B;
	cur->next = -1;

	lock_release(pagetable_lock);
}

/* invalidates the page pointed to by page. Resolves the index to invalidate 
 * by hashing both the page and pid */
void
invalidatepage(vaddr_t page)
{
	lock_acquire(pagetable_lock);
	pagetable[hash(page)].control &= ~VALID_B;
	lock_release(pagetable_lock);
}

struct pte *
getpte(vaddr_t page)
{
	int index;
	struct pte *cur;

	index = hash(page);
	cur = &pagetable[index];

	while((cur->owner!=curthread->t_pid)||(cur->page!=page))
	{
		if (cur->next!=-1)
			cur = &pagetable[cur->next];
		else
		{
			cur = NULL;
			break;
		}
	}
	
	return cur;
}

int 
getindex(vaddr_t page)
{
	int index;
	struct pte *cur;

	index = hash(page);
	cur = &pagetable[index];
	while((cur->owner!=curthread->t_pid)||(cur->page!=page))
	{
		if (cur->next!=-1)
		{
			cur = &pagetable[cur->next];
			index = cur->next;
		}
		else
		{
			index = -1;
			break;
		}
	}

	return index;
}
	
	

/* a stupid hash function for the inverted page table */
/* for the most part a lot of these values are somewhat arbitrary until
 * I take the time to research what makes a good hash function. Generally
 * speaking we want to give more weight to the pid because these
 * are guaranteed to be unique */
int
hash(vaddr_t page)
{
	return ((page/10) + ((curthread->t_pid * 7) * 0x01000000)) % pagetable_size;
}

void
pagetable_dump(void)
{
	u_int32_t i;

	kprintf("PAGETABLE DUMP\n");
	for (i=0;i<pagetable_size;i++)
	{
		kprintf("| %02d | %08x | %02d | %c | %c%c%c | %02d |\n",
				i,
			 	pagetable[i].page,
				pagetable[i].owner,
				pagetable[i].control & VALID_B ? 'v' : '-',
				pagetable[i].control & R_B ? 'r' : '-',
				pagetable[i].control & W_B ? 'w' : '-',
				pagetable[i].control & X_B ? 'x' : '-',
				pagetable[i].next);
	}
}
