#include <uio.h>
#include <stat.h>
#include <vnode.h>
#include <errno.h>
#include <synch.h>
#include <proc.h>
#include <current.h>
#include <abstractfile.h>
#include <filetable.h>



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

    result = copyin(buf, kbuf, size); // copy size bytes from but to kbuf
    if (result)
    { 
        return EFAULT;
    }

    lock_acquire(curproc->fdtable_lks[filehandle]); // acquire lock for local fd.
    int ft_idx = curproc->fdtable[filehandle]; 

    // check that file table index is valid
    if (ft_idx == FDTABLE_EMPTY || ft_idx > kfile_table->curr_size) 
    {
        lock_release(curproc->fdtable[filehandle]); 
        return EBADF;
    }

    lock_acquire(kfile_table->files_lk[ft_idx]); // acquire absfile lock

    // get ptr to abstract file
    struct abstractfile *af = kfile_table->files[ft_idx];

    int offset = af->offset;
    int status = af->status;
    struct vn* = af->vn;



    






    if (filehandle < 0 || filehandle) 
    { 
        return EBADF;
    }

    VOP_WRIT()
    
    return 0;

}