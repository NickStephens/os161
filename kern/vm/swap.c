#include <types.h>
#include <lib.h>
#include <kern/unistd.h>
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

	swapsize = 48;
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
swapout(vaddr_t page, pid_t pid, const void *content, int read, int write, int execute)
{
	struct uio ku;
	int result;
	int swap_index;

	swap_index = findfreeswapped();
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
