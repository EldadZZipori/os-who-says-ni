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
    int whence_val;
    int result;

    lock_acquire(curproc->fdtable_lk);

    result = __check_fd(fd);
    if (result) 
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    result = copyin((userptr_t)(sp + 16), &whence_val, sizeof(int32_t));
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }


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
    
    off_t actual_pos = 0;
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
        lock_release(curproc->fdtable_lk);
        return EINVAL;
        break;
    }

    if (actual_pos < 0)
    {
        lock_release(curproc->fdtable_lk);
        return EINVAL;
    }

    // Protect!!!!
    lock_acquire(kfile_table->files_lk[actual_index]);
    kfile_table->files[actual_index]->offset = actual_pos;
    lock_release(kfile_table->files_lk[actual_index]);
    *retval_64 = actual_pos;

    lock_release(curproc->fdtable_lk);

    return 0;
}