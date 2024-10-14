# CPEN 331 - Operating Systems

This repository will be used write, learn, and submit assignments about/on OS161.
OS161 is an educational OS. The full documentation for it can be found here - [OS161 Documentation](http://www.os161.org/).

## Assignment 4: Implementing file-related syscalls
Status: In Progress

#### Files

- `kern/include/file.h` machine independent declerations of file functions and struct
- 

#### `int open(const char *filename, int flags);`
#### `int open(const char *filename, int flags, mode_t mode)`
Description: Opens a file, device, or other kernel object returning a file descriptor suitable for passing to `read()`, `write()`, or `close()`. Must be atomic (I think just with respect to other threads accessing the same file).

Note: File handles 0 (STDIN_FILENO), 1 (STDOUT_FILENO), and 2 (STDERR_FILENO) are used in special ways and are typically assumed by user-level code to always be open. 

Inputs:
- `const char* filepath`: Path to the object to open
- `int flags`: Flags that specify how to open the object. Examples are `O_RDONLY`, `O_WRONLY`, `O_APPEND`, and `O_CREAT`.

Outputs: a non-negative file descriptor on success, or -1 on error. On error, errno must be set according to the error that occured.

#### `read(int fd, void *buf, size_t buflen)`
Description: Reads a given number of bytes from a given file, at the current seek position (offset) of the file, and stores in `buf`. The file must be open for reading. Must be atomic relative to other I/O to the same file. Note: the kernel is not obliged to (and generally cannot) make the read atomic with respect to other threads in the same process accessing the I/O buffer during the read. 

Inputs:
- `int fd`: File descriptor of an open file for reading from
- `void *buf`: Memory address to copy file contents to
- `size_t buflen`: Number of bytes to read

Outputs:
- Non-negative number of bytes read, or -1 for error. Must set `errno` on error. Number of bytes read does not necessarily have to be equal to `buflen`, and does not signify end of file if it isn't.

#### `ssize_t write(int fd, const void *buf, size_t nbytes)`
Description: Writes to a file given its file descriptor (should get from open() function). While buf is a void pointer
we should decide on what it has to be as we will need to build a uio struct to write to the vnode in reality.
**must be atomic relative to other IO operations**. Note: write will be done at offset.

Inputs:
- `int fd`: File descriptor of the open file
- `void *buf`: Memory address to copy from to the file
- `size_t nbytes`: Indicates the size of the memory to be written to the file

Outputs:
- Returns the cout of bytes written to the file. Will return -1 on error. 0 means no bytes could be written to file.

#### `off_t lseek(int fd, off_t pos, int whence);`
Description: Moves the offset location in a file to a new position. The way to move the curser depends on the whence flag. 

Inputs::
-`int fd`: File descriptor of the open file
-`off_t pos`: signed quntity to move the cursor by.
-`int whence`: Determines how to move the cursor 
- `SEK_SET`, the new position is pos.
- `SEEK_CUR`, the new position is the current position plus pos.
- `SEEK_END`, the new position is the position of end-of-file plus pos.
- anything else, lseek fails.

Outputs:
- Returns the new position on success
- Return -1 on error and sets errno according to the error

#### `int close(int fd)`
Description: Closes a file given its file descriptor. Other files are not affected even if they are attached to the closed file.

Inputs:
-`int fd`: File decriptor of the open file.

Outputs:
- Returns 0 on success and -1 on error. errno is set accordingly 
-`EBADF` fd is not a valid file handle.
-`EIO`	A hard I/O error occurred.

#### `dup2()`
Description:

Inputs:

Outputs:

#### `chdir()`
Description:

Inputs:

Outputs:

#### `__getcwd()`
Description:

Inputs:

Outputs:

## Resources

[OS161 Source Code Layout - os161.org](http://www.os161.org/resources/layout.html)

[SFS Definition - harvard.edu](https://www.eecs.harvard.edu/~cs161/assignments/a4.html)