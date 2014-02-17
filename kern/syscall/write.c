#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <uio.h>
#include <proc.h>
#include <file.h>

int
sys_write(int fd, const void *buf, size_t nbytes)
{
	struct sys_filemapping *mpg;	
	struct uio ku;
	char *kbuf;
	int result;

	mpg = resolvefd(fd);
	if (mpg==NULL)
		return -EBADF;

	/* check flags */
	if (!((mpg->flags & O_WRONLY) || (mpg->flags & O_RDWR)))
		return -1;


	kbuf = (char *) kmalloc(nbytes);
	if (kbuf==NULL)
		return -ENOMEM;


	result = copyin(buf, kbuf, nbytes);
	if (result)
	{
		kfree(kbuf);
		return -result;
	}

	mk_kuio(&ku, kbuf, nbytes, mpg->offset, UIO_WRITE);
	result = VOP_WRITE(mpg->vn, &ku);

	if (result)
		return -result;

	mpg->offset += nbytes;

	return result;
}
