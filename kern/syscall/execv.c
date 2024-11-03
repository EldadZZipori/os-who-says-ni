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
    vaddr_t entrypoint;
    (void)progname;
    (void)retval;

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
        copyin((const_userptr_t)argv[i], argv_kern[i], argv_sizes[i]);
    }

    // create new address space and activate
    struct addrspace *as = as_create();
    proc_setas(as);
    as_activate();

    // open file progname
    struct vnode *v;
    result = vfs_open((char*)progname, O_RDONLY, 0, &v);
    if (result) { 
        // clean up all malloc'd memory
        for (int i = 0; i < argc; i++) {
            kfree(argv_kern[i]);
        }
        kfree(argv_kern);
        return result;
    }

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

    // define stack
    as_define_stack(as, &stackptr);

    /** 
     * We are going to allocate space for argv on user stack, 
     * then we will copy all the strings onto the user stack, 
     * and we will save these pointers in the argv_kern array, 
     * then we will copyout the argv array to user stack.
    */

    // allocate space for argv on user stack
    stackptr -= (argc + 1) * sizeof(char*); // create empty space "below" (more positive) stackptr
    vaddr_t argvptr = stackptr;

    // copyout each element of argv_kern into user stack, updating the pointers in the argv array
    for (int i = 0; i < argc; i++) {
        stackptr -= argv_sizes[i];
        copyout(argv_kern[i], (userptr_t)stackptr, argv_sizes[i]);
        argv_kern[i] = (char*)stackptr;
    }

    // copyout the argv pointer array, which now holdes user-space pointers
    copyout(argv_kern, (userptr_t)argvptr, (argc + 1) * sizeof(char*));

    // warp to usermode (enter_new_process)
    enter_new_process(argc, (userptr_t)argvptr, NULL, stackptr, entrypoint);

    return 0; 
}