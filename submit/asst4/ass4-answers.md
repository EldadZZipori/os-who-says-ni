# Assigment 4 - Octorber 21th

## Code Reading Exercise

### Question 1
Q: What are the ELF magic numbers?

A: They are set values that are checked at the start of the load_elf function, in order to check that the ELF file is actually an ELF file. 
By convention, they are stored in the first four bytes of an ELF file.
load_elf is a function that loads a user program, so it is important to check that the program is indeed the correct type. These numbers are defined by the linker that links the object files into the executable file.

They are -
- ELFMAG0 - 0x7f
- ELFMAG1 - 'E'/0x45
- ELFMAG2 - 'L'/0x4C
- ELFMAG3 - 'F'/0x46

### Question 2 
Q: What is the difference between UIO_USERISPACE and UIO_USERSPACE? When should one use UIO_SYSSPACE instead?

A: UIO_USERISPACE, UIO_USERSPACE, and UIO_SYSSPACE are enumerations of type uio_seg, which are the different options for the source or destination of a uiomove() call. 
If the uio_seg is USERISPACE, then depending on if it is a read or write, it either is copied from kernel buffer to user-space, or user space to kernel buffer, respectively.
When a memory block is executeable (i.e. a program/process) we need to use UIO_USERISPACE, which should be done by the kernel only. 
When we are doing reading or writing that can be done in user context (i.e. file reading or just memory data) we use UIO_USERSPACE.
UIO_SYSSPACE is used by the kernel only, it is used when the kernel need to do I/O operations.

### Question 3
Q: Why can the struct uio that is used to read in a segment be allocated on the stack in load_segment() (i.e., where does the memory read actually go)?

A: The struct uio can be allocated on the stack because the segment is loaded at the virtual address passed in as vaddr, not directly into the uio struct itself.

### Question 4 
Q: In runprogram(), why is it important to call vfs_close() before going to usermode?

A: It is important to call vfs_close() before going going back to user context, because by opening the executable file to read it and load it into memory, 
we increment the reference count to signal that we are using the vnode that represents this file. 
We need to decrement the refcount, which can be done by calling vfs_close(). If we don't do this, the reference count will be inaccurate, 
which means it may not get deallocated even when nothing is pointing to it (it is unreachable and will not be used again).

### Question 5
Q: What function forces the processor to switch into usermode? Is this function machine dependent?

A: Hard to say at which level we are actually forced into user mode. 
Since this is ambiguous, I will just outline my understanding and hope that one of these things is what the question is looking for. 
runprogram calls enter_new_process which calls mips_usermode which calls asm_usermode, which is a function inside exception-mips1.S, which just copies a0 onto the stack jumps into it.
 Arguably, the function that actually puts us into usermode is asm_usermode (the comment says 'This actually does it. See exception-*.S.'), defined in exception-*.S. 
This tells us that it is machine-dependant, as it depends on which type of machine it is (mips, arm, x86, etc.) 

### Question 6
Q: In what file are copyin and copyout defined? memmove? Why can't copyin and copyout be implemented as simply as memmove?

A: 
- Copyin/copyout is defined in kern/include/copyinout.h.
- memmove is defined in common/libc/stirng/memmove.c
memmove is used both by kernel and user context. We don't want to allow a user to move memeory around directly. So copyin and copyout copies memory from the kernel->user and user->kernel in a structured way that does not allow direct control for the user.

We can't simply use memove as if a copy fail we would like to trach what exactly faild and alerrt the kernel correctly

### Question 7
Q: What (briefly) is the purpose of userptr_t?

A: It used to pointer to data that is in userspace. Usually when we are taking a syscall we will copyin data that is userspace data to use
in the kernel context. 
We will use the userptr_t to know the data is from user space.

### Question 8 
Q: What is the numerical value of the exception code for a MIPS system call?

A: The number is - 8
```c
 #define EX_SYS    8    /* Syscall */
```

### Question 9 
Q: How many bytes is an instruction in MIPS? (Answer this by reading syscall() carefully, not by looking somewhere else.)

A:  We can see that a mips instruction is 4 bytes by the fact that it increments the program counter tf->tf_epc by 4 after calling the syscall.

### Question 10
Q: Why do you "probably want to change" the implementation of kill_curthread()?

A: Right now kill_curthread() calls panic no matter what. We probably will not want to collapse the whole system in the future
when a user program does something bad. 
We will want to handle these exceptions more gracefully without completely killing the machine, in the case that a faulty program is ran.

### Question 11
Q: What would be required to implement a system call that took more than 4 arguments?

A: As specified in build/userland/lib/libc/syscalls.S, the first four arguments are stored in a0-3, and the rest are stored on the stack. 
So you would simply need to place the extra arguments on the stack. The MIPS calling convention organizes the stack frame such that the extra arguments are at sp+16, sp+24, etc.

### Question 12
Q: What is the purpose of the SYSCALL macro?

A: The purpose of the SYSCALL macro is to place the syscall number into the register that the kernel expects to find it in (v0), 
and jump to the shared syscall code, __syscall. These are both defined in build/userland/lib/libc/mips/syscalls-mips.S.

### Question 13
Q: What is the MIPS instruction that actually triggers a system call? (Answer this by reading the source in userland/lib/libc/arch/mips, not looking somewhere else.) 

A: The MIPS instruction that triggers a system call is just the `syscall` instruction. 
This is called in the shared syscall MIPS code, __syscall. 

### Question 14
Q: After reading syscalls-mips.S and syscall.c, you should be prepared to answer the following question: OS/161 supports 64-bit values; lseek() takes and returns a 64-bit offset value. Thus, lseek() takes a 32-bit file handle (arg0), 
a 64-bit offset (arg1), a 32-bit whence (arg2), and needs to return a 64-bit offset value. In void syscall(struct trapframe *tf) where will you find each of the three arguments (in which registers) and how will you return the 64-bit offset?

A:
- 32bit arg0 file handleis going to be in regiser a0 (note a1 will be empty as they are an aligned pair)
- 64bit arg1 offset will be in register a2 and register a3 (note that the lsb will be in a3 and the msb in a2, whis is not documented anywhere)
- 32bit arg2 whence will be found on the stack in address (sp+16)
- The 64bit return value will be stored in register v0 and v1 (note that v1 hold the lsd and v0 the lsb)

### Question 15
Q: As you were reading the code in runprogram.c and loadelf.c, you probably noticed how the kernel manipulates the files. Which kernel function is called to open a file? Which macro is called to read the file? 
What about to write a file? Which data structure is used in the kernel to represent an open file? 

A: The kernel function vfs_open is used to open a file and vfs. The macro VOP_READ is used to read a file, 
and VOP_WRITE is used to write a file. These operations are all supported using the vnode struct, 
which is the data structure tah is used in the kernel to represent an *open* file. 


### Question 16
Q: What is the purpose of VOP_INCREF and VOP_DECREF?

A: The purpose of VOP_INCREF and VOP_DECREF are to increment and decrement the reference count of the vnode. 
This is for garbage collection purposes; once the reference count to a vnode is zero, nothing points to it, 
and nothing should ever access it again (on purpose), so it should be deallocated.