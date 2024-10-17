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
    (void)af;

    return 0;
}