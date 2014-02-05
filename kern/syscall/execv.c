#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <vnode.h>
#include <vfs.h>
#include <addrspace.h>

void kfree_all(char *argv[])
{
	int i;

	for (i=0; argv[i]; i++)
		kfree(argv[i]);

}

int sys_execv(const char *path, char *argv[])
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	char **savedargv;
	char **newargv;
	int result;
	int i, j;
	unsigned int cpaddr;
	int length, tail;	

	/* copyin arguments */
	for (i=0; argv[i]; i++);

	savedargv = (char **) kmalloc(sizeof(char **) * i);
	if (savedargv==NULL)
	{
		return -ENOMEM;
	}

	for (j=0; j<i ; j++)
	{
		savedargv[j] = kmalloc(strlen(argv[j])+1);
		savedargv[j+1] = NULL;
		if (savedargv[j]==NULL)
		{
			kfree_all(savedargv);
			kfree(savedargv);
			return -ENOMEM;
		}
		result = copyin(argv[j], savedargv[j], strlen(argv[j])+1);
		if (result)
		{
			kfree_all(savedargv);
			kfree(savedargv);
			return -result;
		}
	}

	/* old addrspace is no longer needed */
	as_destroy(curthread->t_vmspace);

	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL)
	{
		return -ENOMEM;
	}

	result = vfs_open(path, O_RDONLY, &v);
	if (result) {
		as_destroy(curthread->t_vmspace);
		kfree_all(savedargv);
		kfree(savedargv);
		return -result;
	}

	as_activate(curthread->t_vmspace);

	result = load_elf(v, &entrypoint);
	if (result)
	{
		as_destroy(curthread->t_vmspace);
		kfree_all(savedargv);
		kfree(savedargv);
		vfs_close(v);
		return -result;
	}

	vfs_close(v);

	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		return -result;
	}

	/* set up stack with arguments here */
	newargv = (char *) kmalloc(sizeof(char *) * (i+1));
	if (newargv==NULL)
	{
		as_destroy(curthread->t_vmspace);
		kfree_all(savedargv);
		kfree(savedargv);
		return -ENOMEM;
	}

	cpaddr = stackptr;
	for (j=0; j<i; j++)
	{
		length = strlen(savedargv[j])+1;
		cpaddr -= length;
		tail = 0;
		if (cpaddr & 0x3)
		{
			tail = cpaddr & 0x3;
			cpaddr -= tail;
		}
		copyout(savedargv[j], cpaddr, length);
		//bzero(cpaddr+length, tail);
		newargv[j] = cpaddr;
	}
	newargv[j] = NULL;
	cpaddr -= sizeof(char *) * (j+1);
	copyout(newargv, cpaddr, sizeof(char *) * (j+1));

	/* debug */
	/*
	for(j=0; j<i; j++)
	{
		kprintf("[0x%08x]: %s\n", newargv[j], newargv[j]);
	}
	*/

	md_usermode(i, NULL, stackptr, entrypoint);

}
