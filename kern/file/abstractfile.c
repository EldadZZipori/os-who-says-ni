#include <types.h>
#include <current.h>
#include <proc.h>
#include <spinlock.h>
#include <abstractfile.h>
#include <vfs.h>
#include <vnode.h>

int open(const char *filename, int flags, mode_t mode)
{
    // This operation must be atomic 
    spinlock_acquire(&curproc->p_lock);

    /*
        Check if the an identical vnode already exists
        if not call open from vfs
        
    */

    spinlock_release(&curproc->p_lock);
}