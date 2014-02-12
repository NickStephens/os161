#ifndef _PROC_H_
#define _PROC_H_

#include <types.h>
#include <array.h>
#include <thread.h>
#include <synch.h>

/* PROCESS API */

/* global process table */
struct array *proctable;
struct lock  *proctable_lock;

/* Each kernel thread should have a corresponding process structure 
 * A thread can find its own process structure by calling getprocess()
 * on curthread->t_pid, this is also simplified with the getcurprocess 
 * function provided in this API */

/* the process structure:
 *  parentpid - pid of the parent
 *  childexit - when a child exits, it will ring this CV to assist waitpid()
 *  exitted   - flag marking whether the process has exitted or not
 *  exitcode  - self explanatory, undefined if exitted is 0
 */
struct process
{
	/* struct filetable *ftable; */
	pid_t parentpid;
	struct cv *childexit;
	int8_t exited;
	u_int8_t exitcode;
};

/* bootstrap */
void
proc_bootstrap(void);

/* gets an arbitrary process entry */
struct process * 
getprocess(pid_t pid);

/* gets the curthread's process entry */
struct process *
getcurprocess();

/* makes a new process in the process table, returns the pid_t of this new
 * process to be used in assigning to threads. Returns a negative pid_t on
 * error */
pid_t 
newprocess(pid_t parent);

/* notifies the parent kernel thread by broadcasting on it's childexit cv */
void
ringparent();

#endif 
