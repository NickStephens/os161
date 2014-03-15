#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <proc.h>
#include <file.h>

int
sys_close(int fd)
{
	struct sys_filemapping *mpg;
	struct process *proc;
	proc_filemapping *pmpg;
	int sys_index;

	mpg = resolvefd(fd);
	if (mpg==NULL)
		return -EBADF;

	lock_acquire(filetable_lock);
	mpg->refcnt--;

	proc = getcurprocess();

	/* should never fail seeing how we just resolved it */
	pmpg = (proc_filemapping *) array_getguy(proc->filetable, fd);

	sys_index = *pmpg;

	kfree(pmpg);
	array_setguy(proc->filetable, fd, NULL);

	if(mpg->refcnt>0)
	{
		lock_release(filetable_lock);
		return 0;
	}

	/* no more references to mapping */

	vfs_close(mpg->vn);
	
	kfree(mpg);

	if (sys_index < array_getnum(proc->filetable))
		array_setguy(proc->filetable, sys_index, NULL);

	lock_release(filetable_lock);
	return 0;
}

