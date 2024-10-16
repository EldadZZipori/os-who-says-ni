#ifndef _PROC_TABLE_H_
#define _PROC_TABLE_H_

//#include "current.h"

/* 
 * Constants
 */
#define BASE_PROC_AMOUNT 32

struct proctable* kproc_table;

/*
 * @brief holds the current running processes and their open file descriptor tables.
 * Lives in kernel space, and only has one instance in the kernel. Should never be deallocated (unless shutting down?).
 */
struct proctable
{  
    struct proc** processes;            // Pointer to array of all running proccesses

    unsigned int curr_size;         // Current size of the table, dynamically allocated

    unsigned int process_counter;   //  Amount of processes currently running
};

void pt_bootstrap(void);
void pt_destroy(void);
void pt_adjust_size(void);
void pt_add_proc(struct proc* file);
void pt_remove_proc(struct proc* file);
#endif