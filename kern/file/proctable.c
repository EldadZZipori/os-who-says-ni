/*
 * Include order matters. 
 * Make sure to add this file in conf.kern
 */

#include <types.h>
#include <proc.h>
#include <proctable.h>


// Make sure to destroy

void pt_bootstrap(void)
{
    KASSERT(kproc != NULL);

    kproc_table = (struct proctable*)kmalloc(sizeof(struct proctable));

    if (kproc_table == NULL) 
    {
        panic("Could not create proccess table\n");
    }

    kproc_table->processes = (struct proc**)kmalloc(BASE_PROC_AMOUNT*sizeof(struct proc*));
    if (kproc_table->processes == NULL) 
    {
        panic("Could not create proccess table\n");
    }

    kproc_table->processes[0] = kproc;
    kproc_table->curr_size = BASE_PROC_AMOUNT;
    kproc_table->process_counter = 1;               // The kernel processor must exists 
}
void pt_adjust_size(void)
{
}
void pt_add_proc(struct proc* file)
{
    (void) file;
}
void pt_remove_proc(struct proc* file)
{
    (void) file;
}