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
    int result;
    char kpath[__PATH_MAX];
    
    lock_acquire(curproc->fdtable_lk);

    result = copyinstr(pathname, kpath, __PATH_MAX, NULL);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    result = vfs_chdir(kpath);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    lock_release(curproc->fdtable_lk);
    return 0;
}