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

/**
 * @brief Creates the kfile_table that the system uses. 
 */
void 
pt_bootstrap(void);

/**
 * @brief When system shutsdown or an error occures, cleans everythingup. 
 */
void 
pt_destroy(void);

/**
 * @brief When the process table is full, re-ajust the size. 
 */
void 
pt_adjust_size(void);

/**
 * @brief addes a new process to the table. Should be used in Fork.
 * 
 * @param pr the new process
 */
void 
pt_add_proc(struct proc* pr);

/**
 * @brief when a process exits it should be removed from the table
 * 
 * @param pr the process to destroy
 */
void 
pt_remove_proc(struct proc* pr);

/**
 * @brief helper function to find a free location in a process's file descriptor table
 * 
 * @param pr the process
 * @param fd a free index, used as a return value
 * 
 * @return compliance with errno, 0 on success
 */
int 
pt_find_free_fd(struct proc* pr, int* fd);
#endif