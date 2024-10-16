#include <types.h>
#include <current.h>
#include <proc.h>
#include <spinlock.h>
#include <abstractfile.h>
#include <vfs.h>
#include <uio.h>
#include <copyinout.h>
#include <vnode.h>


struct abstractfile*
create_abstractfile(unsigned int status ,struct vnode* node)
{
    struct abstractfile* af = kmalloc(sizeof(struct abstractfile));
    af->offset = 0;
    af->node = node;
    af->ref_count = 1;
    af->status = status;

    return af;
}

int 
open(const char *filename, int flags, mode_t mode)
{
    (void)filename;
    (void)flags;
    (void)mode;
    // This operation must be atomic 
    spinlock_acquire(&curproc->p_lock);

    /*
        Check if the an identical vnode already exists
        if not call open from vfs

    */

    spinlock_release(&curproc->p_lock);

    return 0;
}

int
__getcwd(char *buf, size_t buflen)
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

    copyout(&ku,(userptr_t) buf, sizeof(ku));

    return result;
}

int
chdir(const char *pathname)
{
    (void)pathname;
    return 0;//vfs_chdir(pathname);
}