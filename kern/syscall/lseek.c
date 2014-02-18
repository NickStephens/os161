#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/stat.h>
#include <file.h>

off_t
sys_lseek(int fd, off_t pos, int whence)
{
	struct sys_filemapping *mpg;	
	struct stat st;
	off_t seekto;
	int result;
	int eof;

	mpg = resolvefd(fd);
	if (mpg==NULL)
		return -EBADF;


	lock_acquire(filetable_lock);
	switch(whence)
	{
		case SEEK_SET:
			if (pos < 0)
			{
				lock_release(filetable_lock);
				return -EINVAL;
			}
			seekto = pos;
			break;
		case SEEK_CUR:
			seekto = mpg->offset + pos;
			break;
		case SEEK_END:
			result = VOP_STAT(mpg->vn, &st);
			if (result)
			{
				lock_release(filetable_lock);
				return -result;
			}
			eof = st.st_size;
			if ((eof + pos) < 0)
			{
				lock_release(filetable_lock);
				return -EINVAL;
			}
			seekto = eof + pos;
			break;
		default:
			lock_release(filetable_lock);
			return -EINVAL;
	}


	result = VOP_TRYSEEK(mpg->vn, seekto);
	if (result)
	{
		lock_release(filetable_lock);
		return -result;
	}

	mpg->offset = seekto;
	lock_release(filetable_lock);

	return seekto;
}
