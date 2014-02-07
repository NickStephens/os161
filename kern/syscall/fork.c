#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <machine/trapframe.h>

void
addrspace_dump(struct addrspace *as)
{
	kprintf("as_vbase1: 0x%08x\n", as->as_vbase1);
	kprintf("as_pbase1: 0x%08x\n", as->as_pbase1);
	kprintf("as_vbase2: 0x%08x\n", as->as_vbase2);
	kprintf("as_pbase2: 0x%08x\n", as->as_pbase2);

}

int
sys_fork(struct trapframe *tf)
{
	struct thread *retthread;
	struct addrspace *retaddrspace;
	struct trapframe *childtrapframe;
	int result;

	retaddrspace = as_create();
	if (retaddrspace==NULL)
		return -ENOMEM;

	result = as_copy(curthread->t_vmspace, &retaddrspace);
	if (result)
	{
		kprintf("got result %d\n", result);
		return -result;
	}

	childtrapframe = (struct trapframe *) kmalloc(sizeof(struct trapframe));
	if (childtrapframe==NULL)
	{
		return -ENOMEM;
	}

	memcpy(childtrapframe, tf, sizeof(struct trapframe));
	childtrapframe->tf_vaddr = retaddrspace;
	result = thread_fork("fork", childtrapframe, 0, md_forkentry, &retthread);
	//retthread->t_vmspace = retaddrspace;
	if (result)
	{
		return -result;
	}

	return 1;
}
