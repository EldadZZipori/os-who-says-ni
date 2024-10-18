#include <uio.h>
#include <stat.h>
#include <vnode.h>
#include <kern/errno.h>
#include <fcntl.h>
#include <synch.h>
#include <proc.h>
#include <current.h>
#include <abstractfile.h>
#include <filetable.h>
#include <stat.h>
#include <vfs.h>

int
lseek(int fd, off_t pos, int whence, int *retval_64)
{
    lock_acquire(curproc->fdtable_lk);

    int actual_index = curproc->fdtable[fd];
    
    if (actual_index== FDTABLE_EMPTY)
    {
        lock_release(curproc->fdtable_lk);
        return EBADF;
    }

    struct abstractfile* actual_file = kfile_table->files[actual_index];

    if (!VOP_ISSEEKABLE(actual_file->vn))
    {
        lock_release(curproc->fdtable_lk);
        return ESPIPE;
    }
    
    int actual_pos = 0;
    switch (whence)
    {
    case SEEK_SET:
        actual_pos = pos;
        break;
    case SEEK_CUR:
        actual_pos = actual_file->offset + pos;
        break;
    case SEEK_END:
        struct stat file_stat;
        if (VOP_STAT(actual_file->vn, &file_stat)) 
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

    if (actual_file < 0)
    {
        lock_release(curproc->fdtable_lk);
        return EINVAL;
    }

    actual_file->offset = actual_file;
    *retval_64 = actual_file;

    lock_release(curproc->fdtable_lk);

    return 0;
}