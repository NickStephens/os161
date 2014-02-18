#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/stat.h>
#include <file.h>
#include <vm.h>

int
sys_fstat(int fd, struct stat *statbuf)
{
	struct sys_filemapping *mpg;
	int result;

	if (statbuf >= (USERTOP-(sizeof (struct stat))))
		return EFAULT;

	mpg = resolvefd(fd);
	if (mpg==NULL)
		return EBADF;

	lock_acquire(filetable_lock);
	result = VOP_STAT(mpg->vn, statbuf);
	lock_release(filetable_lock);

	if (result)
		return result;

	return 0;
}
