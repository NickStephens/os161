#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <uio.h>
#include <proc.h>
#include <file.h>

int
sys_read(int fd, void *buf, size_t buflen)
{
	struct sys_filemapping *mpg;
	struct uio ku;
	void *kbuf;
	int result;

	mpg = resolvefd(fd);	
	if (mpg==NULL)
		return -EBADF;

	/* check flags */
	if (!((mpg->flags & O_RDONLY) || (mpg->flags & O_RDWR)))
		return -1;

	kbuf = kmalloc(buflen);
	if (kbuf==NULL)
		return -ENOMEM;

	mk_kuio(&ku, kbuf, buflen, mpg->offset, UIO_READ);
	result = VOP_READ(mpg->vn, &ku);

	if (result)
	{
		kfree(kbuf);
		return -result;
	}

	mpg->offset += buflen;

	result = copyout(kbuf, buf, buflen);
	if (result)
	{
		kfree(kbuf);
		return -result;
	}

	return buflen;
}
