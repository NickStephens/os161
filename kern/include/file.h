#ifndef _FILE_H_
#define _FILE_H_

#include <types.h>
#include <array.h>
#include <synch.h>
#include <vnode.h>
#include <vfs.h>

/* FILE API */

/* global file table */
struct array *filetable;
struct lock  *filetable_lock;

/* system file table entry:
 *  vn 	   - vnode representing the open file.
 *  offset - the offset into the file.
 *  flags  - the oflags the filemapping was openned with
 *  refcnt - reference count of processes pointing to this vnode 
 */ 

struct sys_filemapping {
	struct vnode *vn;
	off_t offset;
	int flags;
	u_int32_t refcnt; 
};

/* a per-process file mapping is just an array of ints */
typedef int proc_filemapping;

/* bootstrap */
void
file_bootstrap(void);

/* resolves a per-process filetable mapping to a sys_filemapping */
struct sys_filemapping *
resolvefd(int fd);

/* places a new sys_filemapping into the filetable
 * returns a negative on error */
int
newfilemapping(struct vnode *v, int flags); 

struct array *
copyfiletable(struct array *);

/* finds the first free file descriptor in the filetable, if there
 * are no free slots, return -1 */
/* since this is inefficient as a loop, it may be better to just define
 * an array to contain free descriptors */
int
findfreedescriptor();

#endif
