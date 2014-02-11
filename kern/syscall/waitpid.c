#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <thread.h>
#include <curthread.h>
#include <machine/vm.h>
#include <proc.h>

/*
 * TODO add support for the -1 parameter.
 * when this is passed in any of the parent's
 * children are waited on. This will require
 * a moditication to the process struct.
 * The process struct will now have to the contain
 * the pid of the last exitted child 
 */
pid_t
sys_waitpid(pid_t pid, int *status, int options) 
{
	struct process *proc;
	struct process *curproc;

	/* error checking */

	/* status must point to userland */
	if (status > USERTOP)
	{
		return (pid_t) -EFAULT;	
	}

	/* currently, no options should be selected */
	if (options==0)
	{
		return (pid_t) -EINVAL;
	}

	proc = getprocess(pid);
	if (proc==NULL)
	{
		return (pid_t) -EINVAL;
	}

	if (proc->exited)
	{
	 	*status = proc->exitcode;
		/* TODO clean up child here 
		 * won't be able to do this 
		 * until a hashtable is implemented.
		 */
		return pid;
	}

	curproc = getcurprocess();

	/* do the wait */
	lock_acquire(proctable_lock);
	cv_wait(curproc->childexit, proctable_lock);
	/* TODO clean up child here */

	status = proc->exitcode;
	return pid;
}
