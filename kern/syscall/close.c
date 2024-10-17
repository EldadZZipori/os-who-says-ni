#include <types.h>
#include <current.h>
#include <proc.h>
#include <spinlock.h>
#include <abstractfile.h>
#include <vfs.h>
#include <uio.h>
#include <copyinout.h>
#include <vnode.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <stat.h>
#include <filetable.h>
#include <proctable.h>
#include <limits.h>
#include <synch.h>
#include <syscall.h>


int
sys_close(int fd)
{
    int result;
    int index_in_fd = curproc->fdtable[fd];

    lock_acquire(curproc->fdtable_lk);
    
    if (index_in_fd == FDTABLE_EMPTY)
    {
        return EBADF;
    }

    /*
     * Removes the file descriptor for this process only
     * Makes it available to reuse.
     */ 
    curproc->fdtable[fd] = FDTABLE_EMPTY; 
    kfile_table->files[index_in_fd]->ref_count --;

    /* 
     *  We are assuming here that the vfs sructre knows to remove the v-node 
     *  once it has no references.
     */
    vfs_close(kfile_table->files[index_in_fd]->vn);

    if (kfile_table->files[index_in_fd]->ref_count == 0)
    {
        ft_remove_file(index_in_fd);
    }

    lock_release(curproc->fdtable_lk);


    return 0;
}