#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <array.h>
#include <synch.h>
#include <vnode.h>
#include <vfs.h>
#include <file.h>
#include <proc.h>

void
file_bootstrap()
{
	filetable = array_create();
	if (filetable==NULL)
		panic("file_bootstrap: could not allocate filetable\n");

	filetable_lock = lock_create("filetable_lock");
	if (filetable_lock==NULL)
		panic("file_bootstrap: could not allocate filetable_lock\n");

	/* TODO add filedescriptors for console to table */
}

struct sys_filemapping *
resolvefd(int fd)
{
	struct process *proc;
	struct sys_filemapping *fm;
	int sys_index;

	proc = getcurprocess();

	sys_index = *((int *) array_getguy(proc->filetable, fd));

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
	if (fd < 0)
	{
		fd = array_getnum(filetable);
		array_add(filetable, fm);
	} else {
		array_setguy(filetable, fd, fm);
	}
	lock_release(filetable_lock);

	return fd;
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

	if (i == count)
		return -1;
	return i;
}
