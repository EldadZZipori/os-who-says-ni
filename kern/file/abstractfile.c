#include <types.h>
#include <current.h>
#include <proc.h>
#include <spinlock.h>
#include <abstractfile.h>
#include <vfs.h>
#include <uio.h>
#include <copyinout.h>
#include <vnode.h>


struct abstractfile*
create_abstractfile(unsigned int status ,struct vnode* node)
{
    struct abstractfile* af = kmalloc(sizeof(struct abstractfile));
    af->offset = 0;
    af->node = node;
    af->ref_count = 1;
    af->status = status;

    return af;
}