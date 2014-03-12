#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <vm.h>
#include <mmap.h>
#include <pagetable.h>

int
sys_mprotect(unsigned long addr, size_t len, int protections)
{
	/* change permissions on each perprocess page in span */
	int pages;		
	int result;
	int i;

	pages = (len / PAGE_SIZE);

	for (i=0;i<pages;i++)	
	{
		result = changeperms(addr + (i*PAGE_SIZE), protections);
		if (result)
			return EFAULT;
	}

	/* clear the tlb to allow these changes to take affect */
	md_cacheflush();

	return 0;
}
