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
#define ARG_LOAD_CHUNK_SIZE 1024 

/**
 * Static function prototypes
*/
static int copyinstrupto(const_userptr_t usersrc, char *dest, size_t len, size_t *actual);
static int userstrlen(const_userptr_t usersrc, size_t *len);

/**
 * @brief Execute a program
 * 
 * @param progname path to the program
 * @param args arguments for the program
 * @param retval on success, no return value is given and the program starts executing.
 * 
 * Must be done with minimal memory usage, and must not use more than 1KB of memory at a time, 
 * because the arguments to the program may be up to 64KB in size. The approach we take is 
 * to copy the arguments from the old address space to the new address space, on the stack, 
 * while using the kernel stack as a temporary buffer to copy the arguments in 1KB chunks.
*/
int sys_execv(userptr_t progname, userptr_t args, int *retval) 
{
    int result;
    int argc;
    vaddr_t stackptr;
    vaddr_t argvp;
    vaddr_t entrypoint;
    struct addrspace *as1;
    struct addrspace *as2;
    struct vnode *v;
    char kprogname[PATH_MAX];

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

    /**
     * Allocate space on the stack for a null-terminated array of pointers to the arguments.
     * This will be used to pass the arguments to the new program, and will point to the 
     * locations where all the strings are saved on the new address-space user-level stack.
    */
    stackptr -= (argc + 1) * sizeof(char*);
    argvp = stackptr;

    // copyout the null terminator at end of argv array
    vaddr_t nullptr = 0;
    result = copyout(&nullptr, (userptr_t)(stackptr + argc*sizeof(char*)), sizeof(char*));

    /**
     * Next, for each arg in args, we will copy it onto the kernel stack, then onto the 
     * new address space stack, all 1KB at a time, then save the pointer on the stack
     * in the previously allocated portion, pointed to by argvp.
     * 
     * To do this, we need to: 
     * 1. switch to as1 and load in the user-space arg pointer, and get its size
     * 2. allocate space on the as2 stack for the full size of the arg
     * 3. switch to as2 and copy 1KB of the arg onto the kernel stack
     * 4. copy the 1KB of the arg from the kernel stack to the new address space stackptr
     * 5. repeat 3-4 until the arg is fully copied
     * 6. record the location of the arg on the stack, in the argv array on the stack
     * 7. Repeat 1-6 for each arg
    */
    for (int i = 0; i < argc; i++) 
    { 
        userptr_t argp;
        size_t size_copied = 0;
        size_t copied_bytes = 0;
        size_t arg_size = 0;

        // switch to as1 to get arg ptr and size
        proc_setas(as1);
        as_activate();

        // copyin ptr to arg, will be updated as we copy in the arg 1 chunk at a time
        result = copyin(args + i * sizeof(char*), &argp, sizeof(char*));

        // get arg_size
        result = userstrlen(argp, &arg_size);
        if (result) {
            return result;
        }

        // allocate space on the stack for the full size of the arg
        stackptr -= arg_size;

        do {
            char karg[ARG_LOAD_CHUNK_SIZE];
            copied_bytes = 0;

            // switch to as1
            proc_setas(as1);
            as_activate();

            // copyin up to 1KB of the string argument into karg, on the stack so it gets deallocated after each chunk
            result = copyinstrupto(argp, karg, ARG_LOAD_CHUNK_SIZE, &copied_bytes);
            if (result) {
                return result;
            }

            // switch to as2
            proc_setas(as2);
            as_activate();

            /**
             * Copy the chunk of the arg from the kernel stack to the new address space stack.
             * Don't use copyoutstr because it may not have a null terminator at the end of the chunk.
            */
            result = copyout(karg, (userptr_t)stackptr, copied_bytes);
            if (result) {
                return result;
            }

            size_copied += copied_bytes;
            argp += copied_bytes;
            stackptr += copied_bytes;

        } while (size_copied < arg_size);

        /**
         * Now that the full argument is copied, we need to copy the pointer to the new address-space stack,
         * in the space we allocated for the pointer array at the i'th position, above all the strings.
         * 
         * First move stackptr back to the bottom of the arg, then copyout the address to argv[i] on as2 stack. 
        */
        stackptr -= arg_size; 
        result = copyout((userptr_t)&stackptr, (userptr_t)argvp + i * sizeof(char*), sizeof(char*));
        if (result) {
            return result;
        }
    }

    as_destroy(as1);
    enter_new_process(argc, (userptr_t)argvp, NULL, stackptr, entrypoint);

    panic("execv is continuing in old process\n");
}


/**
 * @brief Copy a string from user-space to kernel-space, up to len bytes
 * 
 * @param usersrc user-space pointer to the source string
 * @param dest kernel-space pointer to the destination string
 * @param len maximum number of bytes to copy
 * 
 * Copies up to len bytes of a string from user-space to kernel-space.
 * Will not copy more than len bytes, and will not copy past a null terminator.
 * Stores the actual number of bytes copied in actual.
 * 
 * This function is used as a replacement to copyinstr, when we want to be able to 
 * copy a portion of a string up to len bytes, without knowing how big the string is.
*/
static int copyinstrupto(const_userptr_t usersrc, char *dest, size_t len, size_t *actual)
{
    for (size_t i = 0; i < len; i++) {
        int result = copyin(usersrc + i, dest + i, 1);
        if (result) {
            return result;
        }
        (*actual)++;
        if (dest[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

/**
 * @brief Get the length of a string in user-space
 * 
 * @param usersrc user-space pointer to the source string
 * @param len pointer to store the length of the string
 * 
 * Gets the length of a string in user-space, up to a null terminator,
 * without allocating more than a few bytes on the stack.
*/
static int userstrlen(const_userptr_t usersrc, size_t *len)
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
    } while (c != '\0');
    *len = actual;
    return 0;
}