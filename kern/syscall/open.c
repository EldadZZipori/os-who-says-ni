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

/** 
 * @brief a system level code for opening a file or device
 *
 * @param path path to the file device to open
 * @param flags flags indicating the way to open the file
 * @param retval the return value the user should get when the syscall ends
 * 
 * @return compliance with errno, 0 on success
 */
int
sys_open(userptr_t path, int flags, int* retval)
{

    int result;
    int file_location;
    int fd;

    char kpath[__PATH_MAX];
    size_t actual;
    struct abstractfile* af = NULL;
    (void)actual;

    lock_acquire(curproc->fdtable_lk);

    /*
     * Main flags for file opening, others are addons.
     * O_RDONLY   00     
     * O_WRONLY   01      
     * O_RDWR     10    
     */
    int access_mode = flags & O_ACCMODE;
    if (access_mode == 3) 
    {
        lock_release(curproc->fdtable_lk);
        return EINVAL;
    }

    /* Copy memory block from user argument to kernel space */
    result = copyinstr(path, kpath, sizeof(kpath),&actual);
    //result = copyin(path, kpath, sizeof(kpath));
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    result = __open(kpath, flags, &af);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    result = pt_find_free_fd(curproc, &fd);
    if (result)
    {
        vfs_close(af->vn);
        af_destroy(&af);
        lock_release(curproc->fdtable_lk);
        return result;
    }


    result = ft_add_file(&af, &file_location);
    if(result)
    {
        vfs_close(af->vn);
        af_destroy(&af);
        lock_release(curproc->fdtable_lk);
        return result;
    }

    curproc->fdtable[fd] = file_location;

    /*
     * both vfs_open and vfs_lookup that open up a vnode file use VOP_DECREF  
     * Increase the reference count of the v-ndode
     * Increase the amount of open file descriptors for the process table 
     */
    curproc->fdtable_num_entries ++;
    
    *retval = fd;

    lock_release(curproc->fdtable_lk);

    return 0;
}
