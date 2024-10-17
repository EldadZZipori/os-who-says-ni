#include <uio.h>
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
 * @brief Reads from a file represented by a file descriptor.
 * 
 * @param filehandle: Process-local file descriptor as returned from open()
 * @param buf: Void I/O buffer. May be used by other threads.
 * @param size: Number of bytes to read
 * 
 * @return The number of bytes read, or -1 if an error code, and sets errno to the following:
 * 
 * Note: Must advance the seek position by the number of bytes read.
 * 
 * Must be atomic relative to other I/O to the same file. (I.e., no two reads should return the same data.)
 * 
 * Returns the following error codes: 
 * 
 * EBADF 	fd is not a valid file descriptor, or was not opened for reading.
 * EFAULT 	Part or all of the address space pointed to by buf is invalid.
 * EIO 	    A hardware I/O error occurred reading the data.
*/
ssize_t read(int filehandle, void *buf, size_t size)
{ 
    // declare variables
    int result;
    char kbuf[size]; // this will later be transferred to buf using copyout
    struct uio uio;
    struct iovec iov;

    // check if filehandle is valid
    if (filehandle < 0 || filehandle > OPEN_MAX) return EBADF;

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

    // check that file is open for reading
    if (status != O_RDONLY && status != O_RDWR) 
    {
        lock_release(kfile_table->files_lk[ft_idx]);
        lock_release(curproc->fdtable_lk);
        return EBADF;
    }

    // create a uio struct to read from the file
    uio_kinit(&iov, &uio, kbuf, size, offset, UIO_READ);

    // read from the file
    result = VOP_READ(vn, &uio);
    if (result) 
    {
        lock_release(kfile_table->files_lk[ft_idx]);
        lock_release(curproc->fdtable_lk);
        return result; // will return EIO if VOP_READ fails
    }

    // copy the contents of kbuf into buf
    result = copyout(kbuf, buf, size);
    if (result) 
    {
        lock_release(kfile_table->files_lk[ft_idx]);
        lock_release(curproc->fdtable_lk);
        return EFAULT;
    }

    // update the offset in the abstract file
    af->offset = uio.uio_offset;

    // release locks
    lock_release(kfile_table->files_lk[ft_idx]);
    lock_release(curproc->fdtable_lk);

    // return number of bytes read
    return size - uio.uio_resid;
}