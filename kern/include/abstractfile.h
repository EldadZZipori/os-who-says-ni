#ifndef _ABSTRACT_FILE_H_
#define _ABSTRACT_FILE_H_
#include <types.h>
#include <vnode.h>

/*
 * @brief represents an open file
 */
struct abstractfile
{  
    unsigned int ref_count;
    unsigned int status;    // Indicates how the file is open (i.e. read/write/etc)
    off_t offset;    // Indicates location of the curser in the file (what line to read/write next)
    struct vnode* vn;     // Pointer to the file in the virtual file system
};


/**
 * @brief Allocates space and abstract file. This function already sets the reference count to 1.
 * 
 * @param status flags used to open this file
 * @param vnode the virtual file this higher level file points to.
 * @param af the created file will be passed through this pointer
 * 
 * @return returns 0 on success, on error comply with errno
 */
int
af_create(unsigned int status ,struct vnode* node, struct abstractfile** af);

/**
 * @brief destroys a file, this should be called by the open file table when the reference count to the file is zero
 * @param af the file to destroy
 * 
 * @return returns 0 on success, on error comply with errno
 */
int 
af_destroy(struct abstractfile** af);

#endif