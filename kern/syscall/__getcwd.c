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
sys___getcwd(userptr_t buf, size_t buflen, int * retval)
{
    int result;
	struct iovec iov;  
	struct uio ku;     

    char kbuf[buflen];
    
    lock_acquire(curproc->fdtable_lk);

    copyin(buf, kbuf, sizeof(kbuf));
	uio_kinit(&iov, &ku, kbuf, sizeof(kbuf), 0, UIO_READ);

    result = vfs_getcwd(&ku);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    *retval = buflen - ku.uio_resid;

    result = copyout(kbuf, buf, sizeof(ku));
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }
    lock_release(curproc->fdtable_lk);

    return 0;
}