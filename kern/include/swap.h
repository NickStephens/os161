#ifndef SWAP_H_
#define SWAP_H_

#include <types.h>
#include <lib.h>
#include <vnode.h>
#include <synch.h>

/* swap file API */

extern int swapsize;
extern struct vnode *swap;
extern off_t swap_offset;
extern struct swapentry *swapped;
extern struct lock *swapped_lock;

/* swapentry - an in memory representation of a page on disk 
 * addr - the virtual address of the page
 * owner - the process who owns the page
 * index - an index into the swap file
 */

struct swapentry
{
	int		valid; /* gonna word align anyway */
	vaddr_t 	addr;
	pid_t		owner;
	u_int32_t 	perms;
};

/* to grab a page out of the swap file requires finding the page in
 * swapped array, the index of this element is the same as the index
 * of the page into the swapfile.
 * multiplying the index by PAGE_SIZE will land the file offset at 
 * the desired page */

/* bootstrap */
void
swap_bootstrap();
 
/* write the content out to the swap under the page and */
int 
swapout(vaddr_t page, pid_t pid, const void *content, int read, int write, int execute);

/* swaps the requested page out of the 'swap' and places into the 
 * page table at index */
int 
swapin(int index, vaddr_t page, pid_t pid);

/* gets the swap entry corresponding the page and pid return -1 if the
 * entry cannot be found */
int
getswap(vaddr_t page, pid_t pid);

/* invalidates all swapentries belonging to the given process id */
void
invalidateswapentries(pid_t pid);

static
int
findfreeentry();

/* implements the clock policy to choose a page to best swapout */
unsigned int
pickreplacement();

/* debug */
void
swapped_dump(void);
#endif
