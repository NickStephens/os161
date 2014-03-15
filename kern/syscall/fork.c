#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <machine/trapframe.h>
#include <proc.h>

int
sys_fork(struct trapframe *tf)
{
	struct thread *retthread;
	struct addrspace *retaddrspace;
	struct trapframe *childtrapframe;
	struct array *newft;
	unsigned long *entryargs;	
	pid_t childpid;
	int result;

	childtrapframe = (struct trapframe *) kmalloc(sizeof(struct trapframe));
	if (childtrapframe==NULL)
	{
		return -ENOMEM;
	}

	entryargs = (unsigned long *) kmalloc(sizeof(unsigned long)*3);
	if (entryargs==NULL)
	{
		kfree(childtrapframe);
		return -ENOMEM;
	}

	memcpy(childtrapframe, tf, sizeof(struct trapframe));

	childpid = newprocess(curthread->t_pid);
	if (childpid < 0)
	{
		kfree(childtrapframe);
		kfree(entryargs);
		return (int) childpid;
	}

	/* copy parent's filetable */
	newft = copyfiletable(curthread->t_pid);
	if (setfiletable(childpid, newft)<0)
	{
		kfree(childtrapframe);
		kfree(entryargs);
		array_destroy(newft);
		return -1;	
	}

	result = as_copy(curthread->t_vmspace, &retaddrspace, childpid);
	if (result)
	{
		return -result;
	}

	entryargs[0] = childtrapframe; entryargs[1] = retaddrspace; entryargs[2] = childpid;
	result = thread_fork(curthread->t_name, entryargs, 0, md_forkentry, &retthread);
	if (result)
	{
		return -result;
	}

	return childpid;
}
