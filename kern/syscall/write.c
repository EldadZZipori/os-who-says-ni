#include <types.h>
#include <stat.h>
#include <vnode.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <synch.h>
#include <proc.h>
#include <current.h>
#include <abstractfile.h>
#include <filetable.h>
#include <vfs.h>
#include <syscall.h>
#include <copyinout.h>
#include <uio.h>


ssize_t sys_write(int filehandle, userptr_t buf, size_t size, int *retval)
{
    int ft_idx;
    int result; 
    //void* kbuf = kmalloc(size);
    struct uio uio;
    struct iovec iov;
    struct stat file_stat;


    // acquire lock for process' fd table - first layer of file structure
    lock_acquire(curproc->fdtable_lk); // acquire lock for process' fd table.
    // if (buf == NULL)
    // {
    //     lock_release(curproc->fdtable_lk);
    //     return 0;
    // }

    result = __check_fd(filehandle);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    // copy user argument buf (contents to write) to kernel space
    // this should be done anytime a kernel function is called with a 
    // user-space pointer
    // result = copyin(buf, kbuf, size); // copy size bytes from but to kbuf
    // if (result)
    // { 
    //     lock_release(curproc->fdtable_lk);
    //     return result; // will return EFAULT if copyin fails
    // }
    
    ft_idx = curproc->fdtable[filehandle]; 

    if (ft_idx == FDTABLE_EMPTY || ft_idx > (int)kfile_table->curr_size) 
    {
        lock_release(curproc->fdtable_lk); 
        return EBADF;
    }

    // acquire lock for abstract file - second layer of file structure
    lock_acquire(kfile_table->files_lk[ft_idx]); // acquire absfile lock

    // get ptr to abstract file
    struct abstractfile *af = kfile_table->files[ft_idx];

    off_t offset = af->offset;
    int status = af->status;
    struct vnode *vn = af->vn;


    // check that file is open for writing
    if ((status & (O_WRONLY | O_RDWR | O_APPEND)) == 0)
    {
        lock_release(kfile_table->files_lk[ft_idx]);
        lock_release(curproc->fdtable_lk);
        return EBADF;
    }

    // create a uio struct to write to the file
    //uio_kinit(&iov, &uio, buf, size, offset, UIO_WRITE);
    //uio.uio_segflg = UIO_USERSPACE;
    struct addrspace *as = proc_getas();
    iov.iov_kbase = buf;
    iov.iov_len = size;
    uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_resid = size;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_space = as;

    // get file info including size for append mode
    result = VOP_STAT(vn, &file_stat);
    if (result) 
    {
        lock_release(kfile_table->files_lk[ft_idx]);
        lock_release(curproc->fdtable_lk);
        return EIO;
    }

    // update the offset in the uio struct depending on the file status
    if (status == O_APPEND) 
    {
        uio.uio_offset = file_stat.st_size;
    }

    // write to the file
    result = VOP_WRITE(vn, &uio);
    if (result) 
    {
        lock_release(kfile_table->files_lk[ft_idx]);
        lock_release(curproc->fdtable_lk);
        return result; // will return EIO if VOP_WRITE fails or ENOSPC if there is no space left on the device
                       // ENOSPC is hard to find. It is returned inside sfs_write. 
                       // trace: sys_write -> VOP_WRITE -> sfs_write -> sfs_io -> sfs_blockio -> sfs_bmap 
                       // -> sfs_balloc -> bitmap_alloc -> ENOSPC
    }

    // update the offset in the abstract file
    af->offset = uio.uio_offset;

    // release locks
    lock_release(kfile_table->files_lk[ft_idx]);
    lock_release(curproc->fdtable_lk);

    *retval = size - uio.uio_resid;
    return 0;

}
