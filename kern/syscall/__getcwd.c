#include <types.h>
#include <current.h>
#include <proc.h>
#include <spinlock.h>
#include <abstractfile.h>
#include <vfs.h>
#include <uio.h>
#include <copyinout.h>
#include <vnode.h>
#include <syscall.h>

int
sys___getcwd(userptr_t buf, size_t buflen, int * retval)
{
    int result;
	struct iovec iov;   // Used for read/write I/O calls
	struct uio ku;      // Memory block for kernel/user space

    char kbuf[buflen];

    // Only need to copy out
    //copyinstr((userptr_t)buf, kbuf, buflen, NULL);

    // use copyin copyout
	uio_kinit(&iov, &ku, kbuf, sizeof(kbuf), 0, UIO_READ);

    result = vfs_getcwd(&ku);
    if (result)
        return result;

    *retval = buflen - ku.uio_resid;

    copyout(&ku, buf, sizeof(ku));

    return 0;
}