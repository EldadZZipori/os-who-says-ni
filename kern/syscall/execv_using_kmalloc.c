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
    vaddr_t stackptr;
    vaddr_t argvptr;
    vaddr_t entrypoint;
    char kprogname[PATH_MAX];
    struct addrspace *as;
    struct vnode *v;

    // copyin kprogname
    copyinstr(progname, kprogname, PATH_MAX, NULL);

    // copyin the argv array to kernel stack
    char **argv = (char**)args;
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }

    // create array to keep track of string sizes
    int argv_sizes[argc];

    // create array for storing string pointers
    char **argv_kern = kmalloc((argc + 1) * sizeof(char*)); // +1 for NULL
    if (argv_kern == NULL) {
        return ENOMEM;
    }

    copyin(args, argv_kern, (argc + 1) * sizeof(char*)); // include NULL terminator

    // copyin the strings pointed to by the argv array on kernel heap
    for (int i = 0; i < argc; i++) {
        argv_sizes[i] = strlen(argv_kern[i]) + 1; // including NULL terminator
        argv_kern[i] = kmalloc(argv_sizes[i]);
        // check we have enough MEM
        if (argv_kern[i] == NULL) {
            for (int j = 0; j < i; j++) {
                kfree(argv_kern[j]);
            }
            kfree(argv_kern);
            return ENOMEM;
        }
        // copy string into kernel 
        copyinstr((const_userptr_t)argv[i], argv_kern[i], argv_sizes[i], NULL);
    }

    // open file progname
    result = vfs_open(kprogname, O_RDONLY, 0, &v);
    if (result) { 
        // clean up all malloc'd memory
        for (int i = 0; i < argc; i++) {
            kfree(argv_kern[i]);
        }
        kfree(argv_kern);
        return result;
    }

    // create new address space and activate
    as = as_create();
    if (as == NULL) 
    {
        for (int i = 0; i < argc; i++) {
            kfree(argv_kern[i]);
        }
        kfree(argv_kern);
        return ENOMEM;
    }
    
    /* Switch to new addrspace and activate it */
    proc_setas(as);
    as_activate();


    // load elf 
    result = load_elf(v, &entrypoint);
    if (result) {
        for (int i = 0; i < argc; i++) {
            kfree(argv_kern[i]);
        }
        kfree(argv_kern);
        vfs_close(v);
        return result;
    }

    // close file progname
    vfs_close(v);

    // set up new address space and define stack
    result = as_define_stack(as, &stackptr);
    if (result) {
        for (int i = 0; i < argc; i++) {
            kfree(argv_kern[i]);
        }
        kfree(argv_kern);
        return result;
    }

    /** 
     * We are going to allocate space for argv on user stack, 
     * then we will copy all the strings onto the user stack, 
     * and we will save these pointers in the argv_kern array, 
     * then we will copyout the argv array to user stack.
    */

    // allocate space for argc on user stack
    stackptr -= sizeof(int);
    copyout(&argc, (userptr_t)stackptr, sizeof(int));

    // allocate space for argv on user stack
    stackptr -= (argc + 1) * sizeof(char*); // create empty space "below" (more positive) stackptr
    argvptr = stackptr;

    // copyout each element of argv_kern into user stack, updating the pointers in the argv array
    for (int i = 0; i < argc; i++) {
        stackptr -= argv_sizes[i]; // create space below sp
        copyoutstr(argv_kern[i], (userptr_t)stackptr, argv_sizes[i], NULL);
        argv_kern[i] = (char*)stackptr; // store location of i'th argument in argv_kern
    }

    // copyout the argv pointer array, which now holdes user-space pointers
    copyout(argv_kern, (userptr_t)argvptr, (argc + 1) * sizeof(char*));

    // warp to usermode (enter_new_process)
    enter_new_process(argc, (userptr_t)argvptr, NULL, stackptr, entrypoint);

    *retval = 0;
    return 0; 
}