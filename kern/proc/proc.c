#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <proc.h>

void
proc_bootstrap(void)
{
	proctable = array_create();	
	proctable_lock = lock_create("proctable_lock");
	if (proctable_lock==NULL)
		panic("proc_bootstrap: failed to initialize proctable_lock\n");
}

struct process *
getprocess(pid_t pid)
{
	struct process *proc;

	lock_acquire(proctable_lock);
	if (pid > array_getnum(proctable))
		proc = NULL;
	else 
		proc = (struct process *) array_getguy(proctable, (int) pid);

	lock_release(proctable_lock);

	return proc;
}

struct process *
getcurprocess()
{
	pid_t curpid;
	struct process *proc;

	curpid = curthread->t_pid;

	lock_acquire(proctable_lock);
	assert(curpid < array_getnum(proctable));

	proc = (struct process *) array_getguy(proctable, (int) curpid);	

	lock_release(proctable_lock);
	
	return proc;
}

/* If being called by an exiting child, this should only be called
 * once done fiddling with the process table entry */
void
ringparent()
{
	struct process *proc;
	struct process *parentproc;

	proc = getcurprocess();
	
	parentproc = getprocess(proc->parentpid);

	/* If the parent has left and we've been orphaned, just return */
	/* it's actually more likely that we'll crash-and-burn from a 
	 * NULL pointer dereference if the parent has left */
	if (parentproc==NULL)
		return;

	lock_acquire(proctable_lock);
	cv_broadcast(parentproc->childexit, proctable_lock);
}
