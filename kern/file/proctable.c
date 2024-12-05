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

    for (int i=0; i < BASE_PROC_AMOUNT; i++) kproc_table->processes[i] = NULL;

    kproc_table->pid_lk = lock_create("proctable pid lock");
    if(kproc_table->pid_lk == NULL)
    {
        panic("Could not create proccess table lock for pids\n");
    }

    kproc_table->processes[0] = kproc;
    kproc->p_pid = 0;                              // kernel process should always be 0
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
        return; // Should never really get here as calling function should just return an error if max size reached
    }
    int new_size = kproc_table->curr_size + BASE_PROC_AMOUNT;
    struct proc** new_proc_list = (struct proc**)kmalloc((new_size)*sizeof(struct proc*));
    if (new_proc_list == NULL)
    {
        panic("Could not adjust the size of the process table");
    }

    // give all the pointers to the process to the new processes list
    for(unsigned int i=0; i < kproc_table->curr_size; i++)
    {
        new_proc_list[i] = kproc_table->processes[i];
        kproc_table->processes[i] = NULL;
    }

    for (int i=kproc_table->curr_size; i < new_size; i++) kproc_table->processes[i] = NULL;

    // release the memory allocated for the original list
    kfree(kproc_table->processes);


    // assign the new list to the process table pointer
    kproc_table->processes = new_proc_list;
    kproc_table->curr_size = new_size;

}

int 
pt_add_proc(struct proc* pr, int pid)
{

    // Implement in assignment 5
    // check pid is valid
     if (pid == MAX_PID_REACHED || pid >= __PID_MAX || pid < 0)
     {
        return EINVAL;
     }

    // Check pid is actually free
    if (kproc_table->processes[pid] != NULL)
    {
        return EINVAL; // TODO return something more meaningfull
    }


    kproc_table->processes[pid] = pr;
    kproc_table->process_counter++;

    // pid should only really be given my the process table
    pr->p_pid = pid;

    return 0;

}

void 
pt_remove_proc(int pid)
{
     // Implement in assignment 5
     if (pid == MAX_PID_REACHED || pid >= __PID_MAX || pid < 0)
     {
        return;
     }

    kproc_table->processes[pid] = NULL;
    kproc_table->process_counter--;

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

    for (unsigned int i = 0; i < kproc_table->curr_size; i++)
    {
        if (kproc_table->processes[i] == NULL)
        {
            pid = i;    // TODO: might need to be +1 as __PID_MIN is 2
            break;
        }
    }

    /* If we cannot find any available pid, check if we can allocate more space for programs */
    if (pid == -1)
    {
        pt_adjust_size();
        pid = kproc_table->curr_size;  // the new size will be the index of the newest pid
    }

    return pid;
}

int
pt_check_pid(int pid)
{
    if (pid <= 0 || pid >=  __PID_MAX)
    {
        return ESRCH;
    }
    
    if (kproc_table->processes[pid] != NULL)
    {
        return 0;
    }

    // if the process at that pid is null it does not exist
    return ESRCH;
}