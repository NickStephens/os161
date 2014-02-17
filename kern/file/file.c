#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <array.h>
#include <synch.h>
#include <vnode.h>
#include <vfs.h>
#include <thread.h>
#include <curthread.h>
#include <file.h>
#include <proc.h>

void
file_bootstrap()
{
	struct array *ft;
	struct process *proc;
	int result;

	filetable = array_create();
	if (filetable==NULL)
		panic("file_bootstrap: could not allocate filetable\n");

	filetable_lock = lock_create("filetable_lock");
	if (filetable_lock==NULL)
		panic("file_bootstrap: could not allocate filetable_lock\n");

	/* TODO add filedescriptors for console to table */
	
	/* set up filetable for origin thread */
	proc = getcurprocess();
	ft = array_create();
	if (ft==NULL)
		panic("file_bootstrap: could not allocate origin thread's filetable\n");

	setfiletable(curthread->t_pid, ft);

	result = sys_open("con:", O_RDWR); // stdin
	addprocfilemapping(result, curthread->t_pid); // stdout
	addprocfilemapping(result, curthread->t_pid); // stderr
}

void
filetable_dump(void)
{
	struct process *proc;
	struct array *ft;
	int i, num;
	proc_filemapping *mpg;

	proc = getcurprocess();

	lock_acquire(proctable_lock);
	ft = proc->filetable;

	num = array_getnum(ft);
	for (i=0; i<num; i++)
	{
		mpg = (proc_filemapping *) array_getguy(ft, i);
		kprintf("%d -> %d\n", i, *mpg);
	}
	lock_release(proctable_lock);
}

struct sys_filemapping *
resolvefd(int fd)
{
	struct process *proc;
	struct sys_filemapping *fm;
	int sys_index;

	proc = getcurprocess();

	lock_acquire(proctable_lock);
	if (fd >= array_getnum(proc->filetable))
	{
		lock_release(proctable_lock);
		return NULL;
	}

	sys_index = *((int *) array_getguy(proc->filetable, fd));
	lock_release(proctable_lock);

	lock_acquire(filetable_lock);
	if (sys_index >= array_getnum(filetable))
		panic("resolvefd: process contains stale or invalid sys_fd\n");

	fm = (struct sys_filemapping *) array_getguy(filetable, sys_index);
	lock_release(filetable_lock);

	return fm;
}

int
newfilemapping(struct vnode *v, int flags)
{
	int fd;
	struct sys_filemapping *fm;

	fm = (struct sys_filemapping *) kmalloc(sizeof(struct sys_filemapping));
	if (fm==NULL)
		return -ENOMEM;

	fm->vn = v;
	fm->offset = 0;
	fm->flags = flags;
	fm->refcnt = 1;

	if (flags & O_APPEND)
	{
		/* calculate file length with stat */
		/* fm->offset = eof; */
	}

	fd = findfreedescriptor();
	lock_acquire(filetable_lock);
	if (fd == array_getnum(filetable))
	{
		array_add(filetable, fm);
	} else {
		array_setguy(filetable, fd, fm);
	}
	lock_release(filetable_lock);

	return fd;
}

int
addprocfilemapping(int fd, pid_t pid)
{
	int retfd;
	struct process *proc;
	proc_filemapping *newind;
	
	proc = getprocess(pid);
	lock_acquire(proctable_lock);
	if (proc==NULL)
	{
		lock_release(proctable_lock);
		return -1;
	}
	
	newind = (proc_filemapping *) kmalloc(sizeof(proc_filemapping));
	if (newind==NULL)
	{
		lock_release(proctable_lock);
		return -ENOMEM;
	}
	*newind = fd;

	retfd = array_getnum(proc->filetable);
	array_add(proc->filetable, newind);
	lock_release(proctable_lock);

	return retfd;
}

int
setfiletable(pid_t pid, struct array *ft)
{
	struct process *proc;

	proc = getprocess(pid);
	if (proc==NULL)
		return -1;

	lock_acquire(proctable_lock);
	proc->filetable = ft;
	lock_release(proctable_lock);

	return 0;
}

struct array *
copyfiletable(pid_t pid)
{
	struct process *proc;
	struct array *ft;
	struct array *newft;
	proc_filemapping *ind;
	proc_filemapping *newind;
	int i, num;

	proc = getprocess(pid);
	if (proc==NULL)
		return NULL;
	
	lock_acquire(proctable_lock);
	ft = proc->filetable;

	if (ft==NULL)
	{
		lock_release(proctable_lock);
		return NULL;
	}

	newft = array_create();	
	if (newft==NULL)
	{
		lock_release(proctable_lock);
		return NULL;
	}

	num = array_getnum(ft);
	for (i=0; i<num; i++)
	{
		ind = (proc_filemapping *) array_getguy(ft, i);
		if (ind==NULL)
		{
			newind = NULL;
		}
		else
		{
			newind = (proc_filemapping *) kmalloc(sizeof(proc_filemapping));
			*newind = *ind;
		}
		array_add(newft, newind);
	}
	lock_release(proctable_lock);

	incrementrefs(newft);
	return newft;
}

void
incrementrefs(struct array *ft)
{
	int i, num;
	proc_filemapping *pmpg;
	struct sys_filemapping *smpg;

	lock_acquire(filetable_lock);

	num = array_getnum(ft);
	for(i=0; i<num; i++)
	{
		pmpg = (proc_filemapping *) array_getguy(ft, i);
		smpg = (struct sys_filemapping *) array_getguy(filetable, *pmpg);
		assert(smpg!=NULL); /* no processes should ever hold a stale ref */
		smpg->refcnt++;
	}

	lock_release(filetable_lock);

}

int
findfreedescriptor()
{
	int i, count;
	
	lock_acquire(filetable_lock);
	count = array_getnum(filetable);
	for (i=0; i < count; i++)
	{
		if (array_getguy(filetable, i)==NULL)
			break;
	}

	lock_release(filetable_lock);
	return i;
}
