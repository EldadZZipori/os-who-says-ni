#include <types.h>
#include <current.h>
#include <proc.h>
#include <spinlock.h>
#include <abstractfile.h>
#include <vfs.h>
#include <uio.h>
#include <vnode.h>

int 
open(const char *filename, int flags, mode_t mode)
{
    // This operation must be atomic 
    spinlock_acquire(&curproc->p_lock);

    /*
        Check if the an identical vnode already exists
        if not call open from vfs

    */

    spinlock_release(&curproc->p_lock);
}

int
__getcwd(char *buf, size_t buflen)
{
    int result;
	struct iovec iov;   // Used for read/write I/O calls
	struct uio ku;      // Memory block for kernel/user space


	uio_kinit(&iov, &ku, buf, buflen, 0, UIO_READ);

    result = vfs_getcwd(&ku);

    return result;
}

int
chdir(const char *pathname)
{

}