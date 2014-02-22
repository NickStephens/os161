#include <types.h>
#include <lib.h>
#include <synch.h>
#include <machine/vm.h>
#include <pagetable.h>

void
pagetable_bootstrap(void)
{
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
	kprintf("number of frames available: %d\n", pframes);

	/* if this happens our while loop went wild */
	assert(frames!=0);

	/* lock our pagetable into its frame*/
	pagetable = (struct pte *) PADDR_TO_KVADDR(ram_stealmem(pframes));
	kprintf("pagetable initialized at 0x%08x\n", pagetable);

	pagetable_lock = lock_create("pagetable_lock");
	if (pagetable_lock==NULL)
		panic("pagetable_bootstrap: unable to initialize pagetable_lock\n");

}
