#include <types.h>
#include <stat.h>
#include <vnode.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <proc.h>
#include <synch.h>
#include <current.h>
#include <abstractfile.h>
#include <filetable.h>
#include <vfs.h>
#include <syscall.h>
#include <copyinout.h>
#include <uio.h>

/**
 * @brief Duplicates a file descriptor.
 * 
 * @param oldfd: The file descriptor to duplicate.
 * @param newfd: The new file descriptor to assign to the duplicated file descriptor.
 * 
 * @return newfd on success, or -1 if an error code, and sets errno to the following:
 * 
 * EBADF 	    oldfd is not a valid file handle, or newfd is a value that cannot be a valid file handle.
 * EMFILE		The process's file table was full, or a process-specific limit on open files was reached.
 * ENFILE		The system's file table was full, if such a thing is possible, or a global limit on open files was reached.
*/
int 
sys_dup2(int oldfd, int newfd, int* retval)  
{ 
    // check file descriptors are in range
    if (oldfd < 0 || oldfd > OPEN_MAX || newfd < 0 || newfd > OPEN_MAX) return EBADF;

    // acquire lock for process' fd table - first layer of file structure
    lock_acquire(curproc->fdtable_lk); // acquire lock for process' fd table.

    // check oldfd is a valid file descriptor
    if (curproc->fdtable[oldfd] == FDTABLE_EMPTY) 
    {
        lock_release(curproc->fdtable_lk);
        return EBADF;
    }

    // check we have at least one space in process fdtable 
    if (curproc->fdtable_num_entries >= OPEN_MAX) 
    {
        lock_release(curproc->fdtable_lk);
        return EMFILE;
    }

    // TODO: Is there a global limit on open files? if so, check it here and return EMFILE

    // if newfd is an already opened file, close it
    if (curproc->fdtable[newfd] != FDTABLE_EMPTY) 
    {
        // THIS IS MAYBE JANK
        sys_close(newfd);
    }

    // copy oldfd to newfd
    curproc->fdtable[newfd] = curproc->fdtable[oldfd];

    *retval = newfd; 
    return 0;


}