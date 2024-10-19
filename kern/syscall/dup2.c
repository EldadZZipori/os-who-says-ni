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

int 
sys_dup2(int oldfd, int newfd, int* retval)  
{ 
    int result;


    // acquire lock for process' fd table - first layer of file structure
    lock_acquire(curproc->fdtable_lk); // acquire lock for process' fd table.

    result = __check_fd(oldfd);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }
    result = __check_fd(newfd);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }
    

    int acttual_index = curproc->fdtable[oldfd];

    // check oldfd is a valid file descriptor
    if (acttual_index == FDTABLE_EMPTY) 
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
        // THIS IS TRASH - ELDAD
        //sys_close(newfd); - this was trash
        __close(newfd);
    }

    // copy oldfd to newfd
    curproc->fdtable[newfd] = acttual_index;

    /* 
     * Don't forget to indicate that another thing is now using this file 
     * One of few cases we want to do this manually and not in open
     */
    curproc->fdtable_num_entries++;
    kfile_table->files[acttual_index]->ref_count++;
    VOP_INCREF(kfile_table->files[acttual_index]->vn); 
    

    *retval = newfd; 
    lock_release(curproc->fdtable_lk);
    return 0;


}