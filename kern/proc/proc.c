#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <proc.h>

void
proc_bootstrap(void)
{
	pid_t initpid;

	proctable = array_create();	
	proctable_lock = lock_create("proctable_lock");
	if (proctable_lock==NULL)
		panic("proc_bootstrap: failed to initialize proctable_lock\n");
	
	/* should always be zero */
	initpid = newprocess(0);
	assert(initpid==0);

	curthread->t_pid = initpid;

	kprintf("proctable initialized\n");
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

pid_t
newprocess(pid_t parent)
{
	struct process *newproc;
	pid_t newpid;
	struct cv *newcv;

	newproc = (struct process *) kmalloc(sizeof(struct process));
	if (newproc==NULL)
		return (pid_t) -ENOMEM;

	newcv = cv_create("childexit");
	if (newcv==NULL)
	{
		kfree(newproc);
		return (pid_t) -ENOMEM;
	}

	newproc->parentpid = parent;
	newproc->childexit = newcv;
	newproc->exited = 0;

	lock_acquire(proctable_lock);

	newpid = array_getnum(proctable);	
	array_add(proctable, newproc);

	lock_release(proctable_lock);

	return newpid;
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
