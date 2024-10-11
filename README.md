# CPEN 331 - Operating Systems

This repository will be used write, learn, and submit assignments about/on OS161.
OS161 is an educational OS. The full documentation for it can be found here - [OS161 Documentation](http://www.os161.org/).

## Assignment 4: Implementing file-related syscalls
Status: In Progress

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

#### `write()`
Description:

Inputs:

Outputs:

#### `lseek()`
Description:

Inputs:

Outputs:

#### `close()`
Description:

Inputs:

Outputs:

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