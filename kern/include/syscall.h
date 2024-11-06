/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYSCALL_H_
#define _SYSCALL_H_


#include <cdefs.h> /* for __DEAD */
struct trapframe; /* from <machine/trapframe.h> */

/*
 * The system call dispatcher.
 */

void syscall(struct trapframe *tf);

/*
 * Support functions.
 */

/* Helper for fork(). You write this. */
void enter_forked_process(struct trapframe *tf);

/* Enter user mode. Does not return. */
__DEAD void enter_new_process(int argc, userptr_t argv, userptr_t env,
		       vaddr_t stackptr, vaddr_t entrypoint);


/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);
int sys___time(userptr_t user_seconds, userptr_t user_nanoseconds);


/* 
 * Assigment 4 
 * File Systemcalls
 */
/**
 * @brief get the current working directory
 * 
 * @param buf: buffer to store the path of the current working directory to.
 * @param buflen: size of the buffer. see __PATH_MAX
 * @param retval: returns the length of the data returned.
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * (syscall.c takes care of setting errno and returning the correct return value to the user)
 * 
 * ENOENT	A component of the pathname no longer exists.
 * EIO	A hard I/O error occurred.
 * EFAULT	buf points to an invalid address.
*/
int	
sys___getcwd(userptr_t buf, size_t buflen, int * retval);

/**
 * @brief Opens a file for a process's file descriptor table. In reality also opens it in the global open file descriptor table
 * 
 * @param filename: file name, can include a path.
 * @param flags: the way to open the file.
 * @param retval: the file handle (descriptor) in the current process's open file table
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * (syscall.c takes care of setting errno and returning the correct return value to the user)
 * 
 * ENODEV	The device prefix of filename did not exist.
 * ENOTDIR	A non-final component of filename was not a directory.
 * ENOENT	A non-final component of filename did not exist.
 * ENOENT	The named file does not exist, and O_CREAT was not specified.
 * EEXIST	The named file exists, and O_EXCL was specified.
 * EISDIR	The named object is a directory, and it was to be opened for writing.
 * EMFILE	The process's file table was full, or a process-specific limit on open files was reached.
 * ENFILE	The system file table is full, if such a thing exists, or a system-wide limit on open files was reached.
 * ENXIO	The named object is a block device with no filesystem mounted on it.
 * ENOSPC	The file was to be created, and the filesystem involved is full.
 * EINVAL	flags contained invalid values.
 * EIO	A hard I/O error occurred.
 * EFAULT	filename was an invalid pointer.
*/
int
sys_open(userptr_t path, int flags, int* retval);

/** 
 * @brief Reads from a file represented by a file descriptor.
 * 
 * @param filehandle: Process-local file descriptor as returned from open()
 * @param buf: Void I/O buffer. May be used by other threads.
 * @param size: Number of bytes to read
 * 
 * @param retval The number of bytes read, or -1 if an error code.
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * Note: Must advance the seek position by the number of bytes read.
 * 
 * Must be atomic relative to other I/O to the same file. (I.e., no two reads should return the same data.)
 * 
 * Returns the following error codes: 
 * 
 * EBADF 	fd is not a valid file descriptor, or was not opened for reading.
 * EFAULT 	Part or all of the address space pointed to by buf is invalid.
 * EIO 	    A hardware I/O error occurred reading the data.
*/
ssize_t 
sys_read(int filehandle, userptr_t buf, size_t size, int *retval);

/**
 * @brief Writes to a file represented by a file descriptor.
 *
 * @param filehandle: Process-local file descriptor as returned from open()
 * @param buf: Void I/O buffer. May be used by other threads.
 * @param size: Number of bytes to write 
 * @param retval: The number of bytes written, or -1 if an error code
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * (syscall.c takes care of setting errno and returning the correct return value to the user)
 * EBADF 	fd is not a valid file descriptor, or was not opened for writing.
 * EFAULT 	Part or all of the address space pointed to by buf is invalid.
 * ENOSPC 	There is no free space remaining on the filesystem containing the file.
 * EIO 	    A hardware I/O error occurred writing the data.
 * 
 * Note: Returns zero if nothing could be written but no error occured, which only occurs at the end of fixed-size objects.
 */
ssize_t 
sys_write(int filehandle, userptr_t buf, size_t size, int *retval);

/**
 * @brief Closes a file in a processes file descriptor table. Will also take care of removing any reference counters.
 * 
 * @param fd: The file descriptor to close.
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * (syscall.c takes care of setting errno and returning the correct return value to the user)
 * 
 * EBADF	fd is not a valid file handle.
 * EIO	A hard I/O error occurred.
*/
int
sys_close(int fd);

/**
 * @brief Changes directory
 * 
 * @param pathname: path to new directory, can be relative or absolute
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * (syscall.c takes care of setting errno and returning the correct return value to the user)
 * 
 * ENODEV	The device prefix of pathname did not exist.
 * ENOTDIR	A non-final component of pathname was not a directory.
 * ENOTDIR	pathname did not refer to a directory.
 * ENOENT	pathname did not exist.
 * EIO	A hard I/O error occurred.
 * EFAULT	pathname was an invalid pointer.
*/
int
sys_chdir(userptr_t pathname);

/**
 * @brief Changes the offset position in a file
 * 
 * @param fd: The file descriptor to duplicate.
 * @param pos: The new file descriptor to assign to the duplicated file descriptor.
 * @param sp: Stackpointer, in reality this holds whence, which represents how to set the new pos. This function
 * will take care of copying in the right register from user memeory.
 * @param retval64: a 64 bit return value will be set to the new position
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * (syscall.c takes care of setting errno and returning the correct return value to the user)
 * 
 * EBADF	fd is not a valid file handle.
 * ESPIPE	fd refers to an object which does not support seeking.
 * EINVAL	whence is invalid.
 * EINVAL	The resulting seek position would be negative.
*/
int
sys_lseek(int fd, off_t pos, int sp, int64_t *retval64);

/**
 * @brief Duplicates a file descriptor.
 * 
 * @param oldfd: The file descriptor to duplicate.
 * @param newfd: The new file descriptor to assign to the duplicated file descriptor.
 * 
 * @param retval on success, or -1 if an error code.
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * (syscall.c takes care of setting errno and returning the correct return value to the user)
 * 
 * EBADF 	    oldfd is not a valid file handle, or newfd is a value that cannot be a valid file handle.
 * EMFILE		The process's file table was full, or a process-specific limit on open files was reached.
 * ENFILE		The system's file table was full, if such a thing is possible, or a global limit on open files was reached.
*/
int 
sys_dup2(int oldfd, int newfd, int *retval);

/* Assignment 5 - Processes */
/**
 * @brief duplicates the current running process
 * 
 * @param tf the trapframe of the parent, contatining the architectural state
 * 
 * @param retval in the parent process returns the pid of the child, in the child returns 0
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * on error only the parent returns -
 * 
 *	EMPROC	The current user already has too many processes.
 *	ENPROC	There are already too many processes on the system.
 *	ENOMEM	Sufficient virtual memory for the new process was not available.
 */
int
sys_fork(struct trapframe* tf, int *retval);

/**
 * @brief get the pid of the current running process
 * 
 * @param retval pid of curernt running process will be stored here
 * 
 * @return 0, this function never fails.
 */
int 
sys_getpid(int* reval);

/**
 * @brief system call for existing a process, this indicates expected exit
 * 
 * @param exitcode exit code (status) of program, success etc
 * 
 * @warning this function does not return. See the diagrams to understand whether 
 * the process will become zombie or will actually destroy 
 */
void
sys__exit(int exitcode);

/**
 * @brief exit a function to be used by the kernel, allows exiting processes on faults
 * 
 * @param exitcode exit code (status) of program, success etc
 * 
 * @warning this function does not return. See the diagrams to understand whether 
 * the process will become zombie or will actually destroy 
 */
void
__exit(int exitcode);

/**
 * @brief replaces the currently executing program with a newly loaded program image
 * 
 * @param progname path to the program
 * @param args arguments for the program
 * @param retval on success no return value is given and the program start executing, on error -1
 * 
 * @return does not return on success, otherwise one of the following errors - 
 * 
 * ENODEV	The device prefix of program did not exist.
 * ENOTDIR	A non-final component of program was not a directory.
 * ENOENT	program did not exist.
 * EISDIR	program is a directory.
 * ENOEXEC	program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields.
 * ENOMEM	Insufficient virtual memory is available.
 * E2BIG	The total size of the argument strings exceeeds ARG_MAX.
 * EIO		A hard I/O error occurred.
 * EFAULT	One of the arguments is an invalid pointer.
 */
int 
sys_execv(userptr_t progname, userptr_t args, int *retval);

/**
 * @brief system call allowing a parent process to wait for a child process to exit
 * 
 * @param pid pid of the child process
 * @param status a pointer through which the return status of the child will be passed to its parent
 * @param options no option is currently supported, must be 0
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * 
 * EINVAL	The options argument requested invalid or unsupported options.
 * ECHILD	The pid argument named a process that was not a child of the current process.
 * ESRCH	The pid argument named a nonexistent process.
 * EFAULT	The status argument was an invalid pointer.	
 */
int
sys_waitpid(int pid, userptr_t status, int options, int* retval);

/**
 * @brief internal kernal call allowing a parent process to wait for a child process to exit
 * mostly used to make the kernel wait for a spawned process to finish
 * 
 * @param pid pid of the child process
 * @param status a pointer through which the return status of the child will be passed to its parent
 * @param options no option is currently supported, must be 0
 * @param retval return the pid of the process that exited, on error -1
 * 
 * @return 0 on success, otherwise one of the following errors - 
 * 
 * EINVAL	The options argument requested invalid or unsupported options.
 * ECHILD	The pid argument named a process that was not a child of the current process.
 * ESRCH	The pid argument named a nonexistent process.
 * EFAULT	The status argument was an invalid pointer.	
 */
int
__waitpid(int pid, int* status, int options);
#endif /* _SYSCALL_H_ */

