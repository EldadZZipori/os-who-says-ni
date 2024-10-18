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
chdir(userptr_t pathname)
{
    lock_acquire(curproc->fdtable_lk);

    int result;
    size_t pathlen = sizeof(pathname);
	struct iovec iov;   // Used for read/write I/O calls
	struct uio ku;      // Memory block for kernel/user space

    char kbuf[pathlen];

    // Only need to copy out
    result = copyinstr(pathname, kbuf, pathlen, NULL);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    // use copyin copyout
	uio_kinit(&iov, &ku, kbuf, sizeof(kbuf), 0, UIO_READ);

    result = vfs_chdir(&ku);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    lock_release(curproc->fdtable_lk);
    return 0;
}