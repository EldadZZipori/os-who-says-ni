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

/**
 * @brief Execute a program
 * 
 * 
*/
int sys_execv(userptr_t progname, userptr_t args, int *retval) 
{
    int result;
    int argc;
    size_t offset;
    char kprogname[PATH_MAX];
    // char argv_kern[ARG_MAX];
    argv_kern[32 * 1024];
    vaddr_t stackptr;
    // vaddr_t argvptr;
    vaddr_t entrypoint;
    struct addrspace *as;
    struct vnode *v;

    // copyin kprogname
    copyinstr(progname, kprogname, PATH_MAX, NULL);

    // copyin the argv array to kernel stack
    char** argv = (char**)args;
    argc = 0;
    while (argv[argc] != '\0') {
        argc++;
    }

    char* argv_kern_ptrs[argc];
    size_t argv_kern_sizes[argc];

    // copyin args into argv_kern_ptrs 
    copyin(args, argv_kern_ptrs, argc * sizeof(char*));

    // for each string in argv, copyin the string into argv_kern
    // and update offset, and store size of string in argv_kern_sizes
    offset = 0;
    for (int i = 0; i < argc; i++) { 
        result = copyinstr((userptr_t)argv_kern_ptrs[i], argv_kern + offset, ARG_MAX - offset, NULL);
        if (result) {
            return result;
        }
        argv_kern_sizes[i] = strlen(argv_kern + offset) + 1;
        offset += argv_kern_sizes[i];
    }

    // open file progname
    result = vfs_open(kprogname, O_RDONLY, 0, &v);
    if (result) 
    { 
        return result;
    }

    // create new address space and activate
    as = as_create();
    if (as == NULL) 
    {
        return ENOMEM;
    }
    
    /* Switch to new addrspace and activate it */
    proc_setas(as);
    as_activate();


    // load elf 
    result = load_elf(v, &entrypoint);
    if (result) 
    {
        vfs_close(v);
        return result;
    }

    // close file progname
    vfs_close(v);

    // set up new address space and define stack
    result = as_define_stack(as, &stackptr);
    if (result) 
    {
        return result;
    }

    /** 
     * We are going to allocate space for argv on user stack, 
     * then we will copy all the strings onto the user stack, 
     * and we will save these pointers in the argv_kern array, 
     * then we will copyout the argv array to user stack.
    */

    *retval = 0;
    return 0; 
}