#ifndef _FILE_H_
#define _FILE_H_
#include "types.h"

/*
 * @brief represents an open file
 */
struct file
{  
    unsigned int status;    // Indicates how the file is open (i.e. read/write/etc)
    unsigned int offset;    // Indicates location of the curser in the file (what line to read/write next)
    struct vnode* node;     // Pointer to the file in the virtual file system
    struct lock* vf_lk;     // Lock to ensure atomic operations relatively to to the same file
};


int open(const char *filename, int flags);
int open(const char *filename, int flags, mode_t mode);

#endif