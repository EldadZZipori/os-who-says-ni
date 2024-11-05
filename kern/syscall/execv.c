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
 * Constants
*/
#define ARG_LOAD_CHUNK_SIZE 128 

/**
 * Function Prototypes
*/
static int copyinstrcustom(const_userptr_t usersrc, char *dest, size_t len, size_t *actual);
static int copyinstrlen(const_userptr_t usersrc, size_t *len);

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
    vaddr_t stackptr;
    vaddr_t argvp;
    vaddr_t entrypoint;
    struct addrspace *as1;
    struct addrspace *as2;
    struct vnode *v;

    // save as1 
    as1 = curproc->p_addrspace;

    // copyin kprogname
    copyinstr(progname, kprogname, PATH_MAX, NULL);

    // count args
    argc = 0; 
    while (1) { 
        char *arg; 
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
    char *argv_kern_ptrs[argc + 1];

    // copyin the arg pointers
    result = copyin(args, argv_kern_ptrs, (argc + 1) * sizeof(char*));
    if (result) {
        return result;
    }

    // create a new address space
    as2 = as_create();
    if (as2 == NULL) {
        return ENOMEM;
    }

    // switch to new addrspace
    proc_setas(as2);
    as_activate();

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

    // set up stack 
    result = as_define_stack(as2, &stackptr);
    if (result) {
        return result;
    }

    // close the executable, don't need it anymore
    vfs_close(v);

    // allocate space on as2 stack for argc 
    stackptr -= sizeof(int);
    result = copyout(&argc, (userptr_t)stackptr, sizeof(int));
    if (result) {
        return result;
    }

    // allocate space on as2 stack for the arg pointers (argv will point here, so save its address)
    stackptr -= (argc + 1) * sizeof(char*);
    argvp = stackptr;


    /**
     * Next, for each arg in args, we will copy it onto the kernel stack 1KB at a time, 
     * and then copy it into the new address space stack.  
     * 
     * To do this, we need to: 
     * 1. switch to as1
     * 2. copyinstr 1KB of the arg onto the kernel stack
     * 3. switch to as2
     * 4. advance the stack pointer by 1KB, or the size of the arg that was copied
     * 5. copyout the arg from the kernel stack to the new address space stackptr
     * 6. repeat 4-5 until the arg is fully copied
     * 7. record the pointer to the arg in argv_kern_ptrs using stackptr 
     * 8. record the size of the arg in argv_kern_sizes by tracking the size of the arg copied
    */
    for (int i = 0; i < argc; i++) 
    { 
        // ptr to arg, will be updated as we copy in the arg 1 chunk at a time
        userptr_t argp = (userptr_t)argv_kern_ptrs[i];
        size_t size_copied = 0;
        size_t copyin_size = 0;
        size_t arg_size = 0;
        
        // switch to as1 to get arg size
        proc_setas(as1);
        as_activate();

        // get argsize
        result = copyinstrlen(argp, &arg_size);
        if (result) {
            return result;
        }

        // allocate space on the stack for the full size of the arg
        stackptr -= arg_size;

        do {
            // switch to as1
            proc_setas(as1);
            as_activate();

            copyin_size = 0;  

            // copyin the arg, allocating char[] on the stack so it gets deallocated after each chunk
            char karg[ARG_LOAD_CHUNK_SIZE];
            result = copyinstrcustom(argp, karg, ARG_LOAD_CHUNK_SIZE, &copyin_size);
            if (result) {
                return result;
            }

            // switch to as2
            proc_setas(as2);
            as_activate();

            // copyout the arg to the new address space stack
            // (don't use copyoutstr because it may not have null terminator)
            result = copyout(karg, (userptr_t)stackptr, copyin_size);
            if (result) {
                return result;
            }

            size_copied += copyin_size; // update size of arg copied
            argp += copyin_size; // point to next chunk in user-space arg
            stackptr += copyin_size; // advance user-level stack pointer by size of chunk

        } while (size_copied < arg_size);

        // now full arg is copied, record the pointer and size
        // stackptr should be pointing to the end of the arg, so we need to subtract the size of the arg to get the start
        stackptr -= arg_size;

        // point to start of arg and record ptr and size
        argv_kern_ptrs[i] = (char*)stackptr;

        // write ptr to arg in argv_kern_ptrs
        result = copyout(&stackptr, (userptr_t)argvp + i * sizeof(char*), sizeof(char*));
    }

    // now copy the argv array, with the updated pointers, to as2 stack 
    result = copyout(argv_kern_ptrs, (userptr_t)argvp, (argc + 1) * sizeof(char*));
    if (result) {
        return result;
    }

    // warp to user mode 
    enter_new_process(argc, (userptr_t)argvp, NULL, stackptr, entrypoint);

    *retval = 0;
    return 0;
}

static int copyinstrcustom(const_userptr_t usersrc, char *dest, size_t len, size_t *actual)
{
    for (size_t j = 0; j < len; j++) {
        int result = copyin(usersrc + j, dest + j, 1);
        if (result) {
            return result;
        }
        (*actual)++;
        if (dest[j] == '\0') {
            return 0;
        }
    }
    
    // reached end of len, but no null terminator. actual == len. 
    return 0;
}

static int copyinstrlen(const_userptr_t usersrc, size_t *len)
{
    char c;
    int result;
    size_t actual = 0;
    do {
        result = copyin(usersrc + actual, &c, 1);
        if (result) {
            return result;
        }
        actual++;
        (*len)++;
    } while (c != '\0');
    return 0;
}