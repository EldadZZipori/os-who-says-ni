#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <synch.h>
#include <proc.h>
#include <current.h>
#include <abstractfile.h>
#include <filetable.h>
#include <syscall.h>
#include <vnode.h>
#include <vfs.h>
#include <copyinout.h>
#include <uio.h>

ssize_t sys_read(int filehandle, userptr_t buf, size_t size, int *retval)
{ 
    int result;
    int ft_idx;
    //char kbuf[size]; 
    
    struct uio uio;
    struct iovec iov;
    struct abstractfile *af;

    // acquire lock for process' fd table - first layer of file structure
    lock_acquire(curproc->fdtable_lk); // acquire lock for process' fd table.

    // if (buf == NULL)
    // {
    //     lock_release(curproc->fdtable_lk);
    //     return 0;
    // }
    // check if filehandle is valid
    result = __check_fd(filehandle);
    if (result)
    {
        lock_release(curproc->fdtable_lk);
        return result;
    }

    ft_idx = curproc->fdtable[filehandle]; 

    if (ft_idx == FDTABLE_EMPTY || ft_idx > (int)kfile_table->curr_size) 
    {
        lock_release(curproc->fdtable_lk); 
        return EBADF;
    }

    // acquire lock for abstract file - second layer of file structure
    //lock_acquire(kfile_table->files_lk[ft_idx]); // acquire absfile lock
    lock_acquire(kfile_table->location_lk);
    
    

    // get ptr to abstract file
    af = kfile_table->files[ft_idx];

    off_t offset = af->offset;
    int status = af->status;
    struct vnode *vn = af->vn;

    // If the file does not have a read flag, return an error
    // O_RDONLY is 0 !!! that is evil
    int access_mode = status & O_ACCMODE;
    if (access_mode != O_RDONLY && access_mode!= O_RDWR) 
    {
        //lock_release(kfile_table->files_lk[ft_idx]);
        lock_release(kfile_table->location_lk);
        lock_release(curproc->fdtable_lk);
        return EBADF;
    }

    // create a uio struct to read from the file
    //uio_kinit(&iov, &uio, buf, size, offset, UIO_READ);
    //uio.uio_segflg = UIO_USERSPACE;
    struct addrspace *as = proc_getas();
    iov.iov_kbase = buf;
    iov.iov_len = size;
    uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_resid = size;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_space = as;

    // read from the file
    result = VOP_READ(vn, &uio);
    if (result) 
    {
        lock_release(kfile_table->location_lk);
        lock_release(curproc->fdtable_lk);
        return result; // will return EIO if VOP_READ fails
    }

    // copy the contents of kbuf into buf
    // result = copyout(kbuf, buf, size);
    // if (result) 
    // {
    //     lock_release(kfile_table->files_lk[ft_idx]);
    //     lock_release(curproc->fdtable_lk);
    //     return EFAULT;
    // }

    // update the offset in the abstract file
    af->offset = uio.uio_offset;

    // release locks
    lock_release(kfile_table->location_lk);
    lock_release(curproc->fdtable_lk);

    // return number of bytes read
    *retval = size - uio.uio_resid;
    return 0;
}