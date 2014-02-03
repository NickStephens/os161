#include <types.h>
#include <lib.h>
#include <thread.h>

int
sys__exit(int exitcode)
{
	thread_exit();

	return exitcode;
}
