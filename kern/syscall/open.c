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

    *retval = -1;

    char kpath[__PATH_MAX];
    /*
     *  O_RDONLY   00     
     *  O_WRONLY   01      
     *  O_RDWR     10    
     */
    int access_mode = flags & O_ACCMODE;
    if (access_mode == 3) 
    {
        return EINVAL;
    }

    /* Copy memory block from user argument to kernel space */
    result = copyin(path, kpath, sizeof(kpath));
    if (result)
    {
        return result;
    }

    struct vnode* vn = NULL;
    /* 
     * As per man page ignoring mode for OS161
     */
    result = vfs_open(kpath, sizeof(kpath), 0, &vn);
    if (result)
    {
        return result;
    }
    
    struct abstractfile* af = NULL;

    result = af_create(flags, vn, af);
    if (result)
    {
        vfs_close(vn);
        return result;
    }

    /* When the file is in append mode set the offset to the end of the file */
    if (flags & O_APPEND)
    {
        struct stat st;   

        //  vop_stat        - Return info about a file. The pointer is a
        //                    pointer to struct stat; see kern/stat.h.
        //  See vnode.h
        result = VOP_STAT(vn, &st);
        if (result)
        {
            vfs_close(vn);
            af_destroy(af);
            return result;
        }

        af->offset = st.st_size;
    }
    
    lock_acquire(kproc->fdtable_lk);

    int fd;
    result = pt_find_free_fd(kproc, &fd);
    if (result)
    {
        vfs_close(vn);
        af_destroy(af);
        return result;
    }

    int file_location;

    result = ft_add_file(af, &file_location);
    if(result)
    {
        vfs_close(vn);
        af_destroy(af);
        lock_release(kproc->fdtable_lk);
        return result;
    }

    kproc->fdtable[fd] = file_location;

    /*
     * both vfs_open and vfs_lookup that open up a vnode file use VOP_DECREF   
     */
    VOP_INCREF(af->vn);
    
    *retval = fd;

    lock_release(kproc->fdtable_lk);


    return 0;
}