#include <types.h>
#include <lib.h>
#include <synch.h>
#include <machine/vm.h>
#include <thread.h>
#include <curthread.h>
#include <mmap.h>
#include <swap.h>
#include <pagetable.h>

int wat=0;
int pagetable_initialized;
unsigned int occupation_cnt;

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
	{
		pagetable[i].control = 0;
		pagetable[i].next = -1;
		pagetable[i].prev = -1;
	}

	pagetable_lock = lock_create("pagetable_lock");
	if (pagetable_lock==NULL)
		panic("pagetable_bootstrap: unable to initialize pagetable_lock\n");

	pagetable_initialized = 1;
	occupation_cnt = 0;

}

/* define alloc_kpages (malloc) here for the time being */
/* kernel pages are a special case. since they will never 
 * be asked to be resolve by mips there only real presence
 * is to tell userland that a frame is occupied */
vaddr_t
alloc_kpages(int npages)
{
	u_int32_t i;
	struct pte *oldpte;
	paddr_t free;
	int index;
	int j;

	if (pagetable_initialized)
	{
		lock_acquire(pagetable_lock);
		free = 0;
		for(i=0;i<pagetable_size;i++)
		{
			if (curthread->t_pid==26)
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
					occupation_cnt += npages;
					free = FRAME(i);
					pagetable[i].control = VALID_B | REF_B | SUPER_B;
					break;
				}
			}
		}
		lock_release(pagetable_lock);
	}
	else
	{
		free = ram_stealmem(npages);
	}
	
	if (free==0)
	{
		lock_acquire(pagetable_lock);

		index = getoldest();
		/* if the page we're replacing wasn't valid
		 * increase occupancy count */
		if (!(pagetable[index].control & VALID_B))
			occupation_cnt++;

		oldpte = (struct pte *) &pagetable[index];

		swapout(oldpte->page,
			oldpte->owner,
			PADDR_TO_KVADDR(FRAME(index)),	
			oldpte->control & R_B,
			oldpte->control & W_B,
			oldpte->control & X_B);

		/* only guaranteed to be one page */ 
		free = FRAME(index);

		oldpte->page = 0;
		oldpte->owner = 0;
		oldpte->control |= VALID_B | REF_B | SUPER_B;
		lock_release(pagetable_lock);
	}
	return PADDR_TO_KVADDR(free);
}

/* for now we just invalidate one page. This won't work if more than one
 * page was originally allocated */
void
free_kpages(vaddr_t page)
{
	if (pagetable_initialized)
	{
		//kprintf("[free_kpages] %d (%08x) %d\n", INDEX(KVADDR_TO_PADDR(page)), page, occupation_cnt);
		pagetable[INDEX(KVADDR_TO_PADDR(page))].control &= ~VALID_B;
		occupation_cnt--;
	}
}

int
addpage(vaddr_t page, pid_t pid, int read, int write, int execute, const void *content)
{
	int index; 
	int pre;
	struct pte *cur;

	index = hash(page, pid);

	lock_acquire(pagetable_lock);
	pre = -1;
	cur = &pagetable[index];


	//kprintf("[addpage] (%08x) occupation_cnt %d index (%d)\n", page, occupation_cnt, index);
	if (occupation_cnt==pagetable_size)
	{
		/* stash in virtual memory */
		swapout(page, pid, content, read, write, execute);
		lock_release(pagetable_lock);
		return -1;
	}

	while (cur->control & VALID_B)
	{
		pre = index;
		if (cur->next==-1) /* valid but end of chain */
		{
			index = findnextinvalid(++index);
			//kprintf("[addpage] findnextinvalid() returning %d\n", index);
			cur->next = index;
		}
		else /* valid and chain continues */
			index = cur->next;
		cur = &pagetable[index];
	}

	if (pre!=-1)
	{
		pagetable[pre].next = index;
		pagetable[index].prev = pre;
	}

	cur->page    = page;
	cur->owner   = pid;
	cur->control = 0;
	
	if (read)	
		cur->control |= R_B;
	if (write)
		cur->control |= W_B;
	if (execute)
		cur->control |= X_B;

	cur->control |= VALID_B;
	cur->next = -1; 

	/* transfer from content */
	/* content is assumed to be a kernel vaddr */
	if (content!=NULL)
	{
		memmove((void *)PADDR_TO_KVADDR(FRAME(index)), 
			(const void *)content, 
			PAGE_SIZE);
	}

	occupation_cnt++;

	if (curthread->t_pid==24)
	{
		kprintf("[addpage] (%08x, %02d)\n", page, pid);
		pagetable_dump_one(2);	
	}

	lock_release(pagetable_lock);
	return index;
}

/* invalidates the page pointed to by page. Resolves the index to invalidate 
 * by hashing both the page and pid */
void
invalidatepage(vaddr_t page)
{
	int index;
	

	index = getindex(page);
	//kprintf("[invalidatepage] %d (%08x) %d %d %d\n", index, page, occupation_cnt, hash(page, curthread->t_pid), curthread->t_pid);

	if (index == -1)
	{
		return;	
	}
	occupation_cnt--;
	lock_acquire(pagetable_lock);
	pagetable[index].control &= ~VALID_B;
	lock_release(pagetable_lock);
}

struct pte *
getpte(vaddr_t page)
{
	struct pte *oldpte;
	int rindex;
	int index;

	index = getindex(page);
	if (index==-1)
	{
		/* is it in swap? */
		if (getswap(page, curthread->t_pid)==-1)
			return NULL;

		/* swap in page if found */
		lock_acquire(pagetable_lock);

		rindex = getoldest(); // REPLACEMENT POLICY GOES HERE

		/* if the page we're replacing wasn't valid */
		if (!(pagetable[rindex].control & VALID_B))
			occupation_cnt++;

		oldpte = (struct pte *) &pagetable[rindex];	
		/* append the replacement page to the proper hash chain */
		appendtochain(rindex, hash(page, curthread->t_pid));

		swapout(oldpte->page, 
			oldpte->owner,
			PADDR_TO_KVADDR(FRAME(rindex)),
			oldpte->control & R_B,
			oldpte->control & W_B,
			oldpte->control & X_B);

		/* handles all memory transfer and sets up new pte */
		swapin(rindex, page, curthread->t_pid);

		lock_release(pagetable_lock);
		return &pagetable[rindex];
	}
	return &pagetable[index];
}

int 
getindex(vaddr_t page)
{
	int index;
	int origindex;
	struct pte *cur;

	lock_acquire(pagetable_lock);
	origindex = index = hash(page, curthread->t_pid);
	/*
	if ((page==0x00400000) && (curthread->t_pid==31))
	{
		wat++;
		if (wat==4)
			pagetable_dump();
	}
	*/
	cur = &pagetable[index];
	while((cur->owner!=curthread->t_pid)||(cur->page!=page))
	{
		if (cur->next!=-1)
		{
			if (cur->next==origindex)
			{
				//panic("[getindex] loop detected\n");
				index = -1;
				break;
			}
			index = cur->next;
			cur = &pagetable[cur->next];
		}
		else
		{
			index = -1;
			break;
		}
	}
	//if ((page==0x7fff9000)&&(curthread->t_pid==21))
	//kprintf("[getindex] returning %d\n", index);

	lock_release(pagetable_lock);
	return index;
}
	
int
changeperms(vaddr_t page, int protections)
{
	struct pte *ppte;

	kprintf("[changeperms] (%08x, %c%c%c)\n", page, 
			protections & PROT_READ ? 'r' : '-',
			protections & PROT_WRITE ? 'w' : '-',
			protections & PROT_EXEC ? 'x' : '-');
	if (curthread->t_pid==24)
		pagetable_dump_one(2);	
	ppte = getpte(page);
	if (ppte==NULL)
		return -1;
	ppte->control &= ~(R_B | W_B | X_B);
	if (protections & PROT_READ)
		ppte->control |= R_B;
	if (protections & PROT_WRITE)
		ppte->control |= W_B;
	if (protections & PROT_EXEC)
		ppte->control |= X_B;

	return 0;
}

/* a stupid hash function for the inverted page table */
/* for the most part a lot of these values are somewhat arbitrary until
 * I take the time to research what makes a good hash function. Generally
 * speaking we want to give more weight to the pid because these
 * are guaranteed to be unique */
int
hash(vaddr_t page, pid_t pid)
{
	return ((page/10) + ((pid * 7) * 0x01000000)) % pagetable_size;
}

void pagetable_dump_one(int i)
{
	i %= pagetable_size;
	kprintf("| %02d | %08x | %02d | %c | %c%c%c | %02d | %02d |\n",
			i,
			pagetable[i].page,
			pagetable[i].owner,
			pagetable[i].control & VALID_B ? 'v' : '-',
			pagetable[i].control & R_B ? 'r' : '-',
			pagetable[i].control & W_B ? 'w' : '-',
			pagetable[i].control & X_B ? 'x' : '-',
			pagetable[i].next,
			pagetable[i].prev);
}

void
pagetable_dump(void)
{
	u_int32_t i;

	if (pagetable_initialized)
	{

		kprintf("PAGETABLE DUMP: OCCUPIED FRAMES %d\n", occupation_cnt);
		for (i=0;i<pagetable_size;i++)
		{
			kprintf("| %02d | %08x | %02d | %c | %c%c%c | %02d | %02d |\n",
					i,
					pagetable[i].page,
					pagetable[i].owner,
					pagetable[i].control & VALID_B ? 'v' : '-',
					pagetable[i].control & R_B ? 'r' : '-',
					pagetable[i].control & W_B ? 'w' : '-',
					pagetable[i].control & X_B ? 'x' : '-',
					pagetable[i].next,
					pagetable[i].prev);
		}
	}
}

void
appendtochain(int index, int chainstart)
{
	struct pte *cur;

	kprintf("[appendtochain] (%d, %d)\n", index, chainstart);

	cur = &pagetable[chainstart];
	while (cur->next!=-1)
		cur = &pagetable[cur->next];			


	cur->next = index;
}

int
getoldest()
{
	struct pte *cur;
	int i;

	kprintf("[getoldest] ()\n");
	 
	i = 0;
	cur = &pagetable[i];

	while ((cur->control & VALID_B) && (cur->control & REF_B))
	{
		if (!(cur->control & SUPER_B))
			cur->control &= ~REF_B; /* turn off reference */
		cur = &pagetable[++i % pagetable_size];
	}

	return i;
}

int
findnextinvalid(int from)
{
	struct pte *cur;

	kprintf("[findnextinvalid] (%d)\n", from);

	if (curthread->t_pid==24)
	{
		kprintf("[findnextinvalid] before\n");
		pagetable_dump_one(2);	
	}

	from = from % pagetable_size;
	cur = &pagetable[from];

	while (cur->control & VALID_B)
	{
		from = (from + 1) % pagetable_size;
		cur  = &pagetable[++from % pagetable_size];
	}

	if (curthread->t_pid==24)
	{
		kprintf("[findnextinvalid] after\n");
		pagetable_dump_one(2);	
	}

	return from;
}
