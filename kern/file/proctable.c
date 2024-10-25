/*
 * Include order matters. 
 * Make sure to add this file in conf.kern
 */

#include <types.h>
#include <proc.h>
#include <synch.h>
#include <kern/errno.h>
#include <filetable.h>
#include <proctable.h>


// Make sure to destroy

void 
pt_bootstrap(void)
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
    kproc_table->pid_lk = lock_create("proctable pid lock");
    if(kproc_table->pid_lk == NULL)
    {
        panic("Could not create proccess table lock for pids\n");
    }

    kproc_table->processes[0] = kproc;
    kproc_table->curr_size = BASE_PROC_AMOUNT;
    kproc_table->process_counter = 1;               // The kernel processor must exists 
}

void 
pt_destroy(void)
{
    // Implement in assignment 
}

void 
pt_adjust_size(void)
{
    /* This should really only be called if the process table is full */
    KASSERT(kproc_table != NULL);
    KASSERT(kproc_table->process_counter ==  kproc_table->curr_size);

    if (kproc_table->curr_size == __PID_MAX)
    {
        return;
    }
}

void 
pt_add_proc(struct proc* pr)
{
    (void) pr;

    // Implement in assignment 5
}

void 
pt_remove_proc(struct proc* pr)
{
    (void) pr;
     // Implement in assignment 5

}

int 
pt_find_free_fd(struct proc* pr, int* fd)
{
    if (pr->fdtable_num_entries == __OPEN_MAX)
    {
        return EMFILE;
    }

    for (int i = 0; i < __OPEN_MAX; i++)
    {
        if (pr->fdtable[i] == FDTABLE_EMPTY)
        {
            *fd = i;
            return 0;
        }
    }

    return EMFILE;
}

int
pt_find_avail_pid(void)
{
    int pid = -1;

    KASSERT(kproc_table != NULL);
    if (kproc_table->process_counter == __PID_MAX)
    {
        return MAX_PID_REACHED;
    }

    lock_acquire(kproc_table->pid_lk);

    for (int i = 0; i < kproc_table->curr_size; i++)
    {
        if (kproc_table->processes[i] == NULL)
        {
            pid = i;    // TODO: might need to be +1 as __PID_MIN is 2
        }
    }

    /* If we cannot find any available pid, check if we can allocate more space for programs */
    if (pid == -1)
    {
        pt_adjust_size();
        pid = kproc_table->curr_size;  // the new size will be the index of the newest pid
    }

    lock_release(kproc_table->pid_lk);

    return pid;
}