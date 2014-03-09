#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <array.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <pagetable.h>

/*
 * Machine dependent memory stuff. Mainly vm_fault.
 */


int
vm_fault(int faulttype, vaddr_t faultaddress)
{

	struct pte *p;
	u_int32_t ehi, elo;
	int index;
	int spl;
	int i;
	paddr_t paddr;

	spl = splhigh();

	faultaddress &= PAGE_FRAME;
	p = getpte(faultaddress);
	if (p==NULL)
	{
		splx(spl);
		return EFAULT;
	}
	
	index = getindex(faultaddress);

	for(i=0;i<NUM_TLB;i++)
	{
		TLB_Read(&ehi, &elo, i);
		if (elo & TLBLO_VALID)
			continue;

		elo = paddr | TLBLO_VALID;

		paddr = FRAME(index);

		elo = paddr | TLBLO_VALID | TLBLO_DIRTY;
		/*
		if (p->control & W_B)
			elo |= TLBLO_DIRTY;
			*/

		ehi = faultaddress;	

		// kprintf("curthread->pid %08x\n\
			 curthread->pid << 6 %08x\n", curthread->t_pid,
		//		 curthread->t_pid << 6);
		ehi |= curthread->t_pid << 6;
		//kprintf("[vm_fault]: ehi: %08x pid: %d\n", ehi, curthread->t_pid);
		TLB_Write(ehi, elo, i);
		splx(spl);
		return 0;
	}
	/* reset TLB_HI to the correct process id 
	 * TLB_Read and TLB_Write clobbers the entry_hi
	 * value */
	TLB_SetProc(curthread->t_pid);
	
	splx(spl);
	return EFAULT;
}

/* clears the machine-dependent TLB */
void
md_cacheflush()
{
	int i;

	for(i=0;i<NUM_TLB;i++)
	{
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

}

void
md_loadprocid(void)
{
	pid_t curpid;
	int ehi;
	int ehi1, elo1;
	int spl;

	spl = splhigh();
	curpid = curthread->t_pid;

	//kprintf("[md_loadprocid]: setting procid to %d\n", curpid);
	TLB_SetProc(curpid << 6, &ehi);
	//kprintf("[md_loadprocid]: gethi returned %08x\n", ehi);
	TLB_GetEhi(&ehi); 

	splx(spl);

}
