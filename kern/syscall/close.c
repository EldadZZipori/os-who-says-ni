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
    lock_acquire(curproc->fdtable_lk);

    result = __close(fd);
    if (result)
    {
        return result;
    }

    lock_release(curproc->fdtable_lk);

    return 0;
}
