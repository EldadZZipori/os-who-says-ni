# Assigment 4 - Octorber 21th

## Code Reading Exercise

### Question 1
Q: What are the ELF magic numbers?

A: 

### Question 2 
Q: What is the difference between UIO_USERISPACE and UIO_USERSPACE? When should one use UIO_SYSSPACE instead?

A:

### Question 3
Q: Why can the struct uio that is used to read in a segment be allocated on the stack in load_segment() (i.e., where does the memory read actually go)?

A:

### Question 4 
Q: In runprogram(), why is it important to call vfs_close() before going to usermode?

A: 

### Question 5
Q: What function forces the processor to switch into usermode? Is this function machine dependent?

A: 

### Question 6
Q: In what file are copyin and copyout defined? memmove? Why can't copyin and copyout be implemented as simply as memmove?

A: 
### Question 7
Q: What (briefly) is the purpose of userptr_t?

A: 

### Question 8 
Q: What is the numerical value of the exception code for a MIPS system call?

A:

### Question 9 
Q: How many bytes is an instruction in MIPS? (Answer this by reading syscall() carefully, not by looking somewhere else.)

A: 

### Question 10
Q: Why do you "probably want to change" the implementation of kill_curthread()?

A: 

### Question 11
Q: What would be required to implement a system call that took more than 4 arguments?

A:

### Question 12
Q: What is the purpose of the SYSCALL macro?

A:

### Question 13
Q: What is the MIPS instruction that actually triggers a system call? (Answer this by reading the source in userland/lib/libc/arch/mips, not looking somewhere else.) 

A: 

### Question 14
Q: After reading syscalls-mips.S and syscall.c, you should be prepared to answer the following question: OS/161 supports 64-bit values; lseek() takes and returns a 64-bit offset value. Thus, lseek() takes a 32-bit file handle (arg0), 
a 64-bit offset (arg1), a 32-bit whence (arg2), and needs to return a 64-bit offset value. In void syscall(struct trapframe *tf) where will you find each of the three arguments (in which registers) and how will you return the 64-bit offset?

A:


### Question 15
Q: As you were reading the code in runprogram.c and loadelf.c, you probably noticed how the kernel manipulates the files. Which kernel function is called to open a file? Which macro is called to read the file? 
What about to write a file? Which data structure is used in the kernel to represent an open file? 

A: 


### Question 16
Q: What is the purpose of VOP_INCREF and VOP_DECREF?

A: