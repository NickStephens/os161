#include <types.h>
#include <lib.h>
#include <thread.h>
#include <proc.h>

int
sys__exit(int exitcode)
{
	struct process *proc;

	proc = getcurprocess();

	lock_acquire(proctable_lock);
	proc->exitcode = exitcode;
	lock_release(proctable_lock);

	ringparent();

	thread_exit();
}
