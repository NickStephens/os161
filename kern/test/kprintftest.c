#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <thread.h>

int kprintf_child(void *nargs, unsigned long nargc)
{
	(void)nargs;
	(void)nargc;

	kprintf("I'm a child thread\n");

	return 0;
}

int kprintftest(void)
{
	struct thread *child;

	child = (struct thread *) kmalloc(sizeof(struct thread));
	if (child==NULL)
	{
		return ENOMEM;
	}
	thread_fork("test-thread", NULL, 0, kprintf_child, &child);
	kprintf("I'm a parent\n");

	return 0;
}
