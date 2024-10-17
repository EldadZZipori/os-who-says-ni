#include <uio.h>
#include <stat.h>
#include <vnode.h>
#include <errno.h>
#include <fcntl.h>
#include <synch.h>
#include <proc.h>
#include <current.h>
#include <abstractfile.h>
#include <filetable.h>
#include <vfs.h>



/**
 * @brief Writes to a file represented by a file descriptor.
 *
 * @param filehandle: Process-local file descriptor as returned from open()
 * @param buf: Void I/O buffer. May be used by other threads.
 * @param size: Number of bytes to write 
 * 
 * @return The number of bytes written, or -1 if an error code, and sets errno to the following:
 * 
 * EBADF 	fd is not a valid file descriptor, or was not opened for writing.
 * EFAULT 	Part or all of the address space pointed to by buf is invalid.
 * ENOSPC 	There is no free space remaining on the filesystem containing the file.
 * EIO 	    A hardware I/O error occurred writing the data.
 * 
 * Note: Returns zero if nothing could be written but no error occured, which only occurs at the end of fixed-size objects.
 */

ssize_t sys_write(int filehandle, const void *buf, size_t size)
{
    int result; 
    int ft_idx;
    int kfiletable_idx;
    char kbuf[size];


    if (filehandle < 0 || filehandle > OPEN_MAX) return EBADF;

    // copy user argument buf (contents to write) to kernel space
    // this should be done anytime a kernel function is called with a 
    // user-space pointer
    result = copyin(buf, kbuf, size); // copy size bytes from but to kbuf
    if (result)
    { 
        return result; // will return EFAULT if copyin fails
    }

    // acquire lock for process' fd table - first layer of file structure
    lock_acquire(curproc->fdtable_lk); // acquire lock for process' fd table.
    int ft_idx = curproc->fdtable[filehandle]; 

    if (ft_idx == FDTABLE_EMPTY || ft_idx > kfile_table->curr_size) 
    {
        lock_release(curproc->fdtable_lk); 
        return EBADF;
    }

    // acquire lock for abstract file - second layer of file structure
    lock_acquire(kfile_table->files_lk[ft_idx]); // acquire absfile lock

    // get ptr to abstract file
    struct abstractfile *af = kfile_table->files[ft_idx];

    int offset = af->offset;
    int status = af->status;
    struct vnode *vn = af->vn;

    // check that file is open for writing
    if (status != O_WRONLY && status != O_RDWR && status != O_APPEND) 
    {
        lock_release(kfile_table->files_lk[ft_idx]);
        lock_release(curproc->fdtable_lk);
        return EBADF;
    }

    // create a uio struct to write to the file
    struct uio uio;
    struct iovec iov;
    uio_kinit(&iov, &uio, kbuf, size, offset, UIO_WRITE);

    // write to the file
    result = VOP_WRITE(vn, &uio);
    if (result) 
    {
        lock_release(kfile_table->files_lk[ft_idx]);
        lock_release(curproc->fdtable_lk);
        return result;
    }

    // update the offset in the abstract file
    af->offset = uio.uio_offset;

    // release locks
    lock_release(kfile_table->files_lk[ft_idx]);
    lock_release(curproc->fdtable_lk);

    return size - uio.uio_resid;

}
