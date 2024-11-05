#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <proc.h>
#include <syscall.h>
#include <copyinout.h>
#include <addrspace.h>
#include <vnode.h>
#include <current.h>

/**
 * @brief Execute a program
 * 
 * 
*/
int sys_execv(userptr_t progname, userptr_t args, int *retval) 
{
    int result;
    int argc;
    char kprogname[PATH_MAX];
    // char argv_kern[ARG_MAX];
    // size_t offset;
    // vaddr_t stackptr;
    // vaddr_t argvptr;
    vaddr_t entrypoint;
    struct addrspace *as1;
    struct addrspace *as2;
    struct vnode *v;

    // save as1 
    as1 = curproc->p_addrspace;

    // copyin kprogname
    copyinstr(progname, kprogname, PATH_MAX, NULL);

    // create a new address space
    as2 = as_create();
    if (as2 == NULL) {
        return ENOMEM;
    }

    // open executable 
    result = vfs_open(kprogname, O_RDONLY, 0, &v);
    if (result) {
        return result;
    }

    // load the executable
    result = load_elf(v, &entrypoint);
    if (result) {
        vfs_close(v);
        return result;
    }

    // close the executable, don't need it anymore
    vfs_close(v);

    // count args
    argc = 0; 
    while (1) { 
        char* arg; 
        result = copyin(args + argc * sizeof(char*), &arg, sizeof(char*));
        if (result) {
            return result;
        }
        if (arg == NULL) {
            break;
        }
        argc++;
    }

    // allocate pointer array and sizes array on kernel heap (should be fine?)
    char *argv_kern_ptrs = kmalloc(argc * sizeof(char*));
    size_t *argv_kern_sizes = kmalloc(argc * sizeof(size_t));

    /**
     * Next, for each arg in args, we will copy it onto the kernel stack 1KB at a time, 
     * and then copy it into the new address space stack.  
    */

    (void)argv_kern_ptrs;
    (void)argv_kern_sizes;
    (void)as1;
    (void)as2;



    *retval = 0;
    return 0;
}