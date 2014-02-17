#include <types.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <proc.h>
#include <file.h>

int
sys_open(char *path, int flags)
{
	struct vnode *v;
	int fd;
	int result;

	result = vfs_open(path, flags, &v);
	if (result)
		return -result;

	kprintf("open: newfilemapping\n");
	result = newfilemapping(v, flags);  
	if (result < 0)
		return result;

	kprintf("getting current process\n");
	kprintf("adding a process filemapping\n");
	fd = addprocfilemapping(result, curthread->t_pid);

	return fd;
}
