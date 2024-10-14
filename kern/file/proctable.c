#include <proctable.h>
#include <proc.h>
#include <types.h>

void pt_bootstrap(void)
{
    KASSERT(kproc != NULL);

    kproc_table = (struct proctable*)kmalloc(sizeof(struct proctable));

    if (kproc_table == NULL) 
    {
        panic("Could not create proccess table\n");
    }

    kproc_table->table = (struct proc**)kmalloc(BASE_PROC_AMOUNT*sizeof(struct proc*));
    if (kproc_table->table == NULL) 
    {
        panic("Could not create proccess table\n");
    }

    kproc_table->table[0] = kproc;
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