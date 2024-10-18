#include <types.h>
#include <stat.h>
#include <vnode.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <synch.h>
#include <proc.h>
#include <current.h>
#include <abstractfile.h>
#include <filetable.h>
#include <vfs.h>
#include <syscall.h>
#include <copyinout.h>
#include <kern/seek.h>
#include <uio.h>

int
sys_lseek(int fd, off_t pos, int sp, int64_t *retval_64)
{
    lock_acquire(curproc->fdtable_lk);
    int whence_val;
    copyin((userptr_t)(sp + 16), &whence_val, sizeof(int32_t));


    int actual_index = curproc->fdtable[fd];
    
    if (actual_index== FDTABLE_EMPTY)
    {
        lock_release(curproc->fdtable_lk);
        return EBADF;
    }

    ;

    if (!VOP_ISSEEKABLE(kfile_table->files[actual_index]->vn))
    {
        lock_release(curproc->fdtable_lk);
        return ESPIPE;
    }
    
    int actual_pos = 0;
    struct stat file_stat;
    switch (whence_val)
    {
    case SEEK_SET:
        actual_pos = pos;
        break;
    case SEEK_CUR:
        actual_pos = kfile_table->files[actual_index]->offset + pos;
        break;
    case SEEK_END:
        if (VOP_STAT(kfile_table->files[actual_index]->vn, &file_stat)) 
        {
            lock_release(curproc->fdtable_lk);
            return EIO;
        }
        actual_pos = file_stat.st_size + pos;
        break; 
    default:
        return EINVAL;
        break;
    }

    if (actual_pos < 0)
    {
        lock_release(curproc->fdtable_lk);
        return EINVAL;
    }

    kfile_table->files[actual_index]->offset = actual_pos;
    *retval_64 = actual_pos;

    lock_release(curproc->fdtable_lk);

    return 0;
}