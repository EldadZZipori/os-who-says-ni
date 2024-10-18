#include <types.h>
#include <current.h>
#include <proc.h>
#include <spinlock.h>
#include <abstractfile.h>
#include <vfs.h>
#include <uio.h>
#include <copyinout.h>
#include <vnode.h>
#include <synch.h>
#include <syscall.h>

int
sys_chdir(userptr_t pathname)
{
    lock_acquire(curproc->fdtable_lk);

    int result;
    size_t pathlen = sizeof(pathname);

    char kbuf[pathlen];

    // Only need to copy out
    result = copyinstr(pathname, kbuf, pathlen, NULL);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    // use copyin copyout
    result = vfs_chdir(kbuf);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    lock_release(curproc->fdtable_lk);
    return 0;
}