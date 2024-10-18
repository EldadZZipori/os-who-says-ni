#include <types.h>
#include <current.h>
#include <proc.h>
#include <spinlock.h>
#include <abstractfile.h>
#include <vfs.h>
#include <uio.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <vnode.h>


int
af_create(unsigned int status ,struct vnode* vn, struct abstractfile** af)
{
    *af = kmalloc(sizeof(struct abstractfile));
    if (af == NULL)
    {
        return ENOMEM;
    }
    (*af)->offset = 0;
    (*af)->vn = vn;
    (*af)->ref_count = 1;
    (*af)->status = status;

    return 0;
}

int 
af_destroy(struct abstractfile** af)
{
    struct abstractfile* local_af = (*af);
    
    // Should not destroy a file that still has references to it
    if(local_af->ref_count != 0)
    {
        return EINVAL;
    }
    
    // No need to decrease ref on the vnode as vfs_close does that!!!
    local_af->vn = NULL;
    kfree(local_af);
    
    return 0;
}