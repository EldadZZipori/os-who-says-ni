#ifndef _PROC_TABLE_H_
#define _PROC_TABLE_H_

//#include "current.h"

/* 
 * For now this is the amount of processes we are allowing in our system. 
 * would be better to dynamiclly change the size of the table.
 * 
 * NOTE: (IMPORTANT!!!)
 *  All functionality of the proctable assumes that the calling function aquires the correct lock BEFORE calling the functions
 *  of the proctable!!!
 */
#define BASE_PROC_AMOUNT    17
#define MAX_PID_REACHED     -1


struct proctable* kproc_table;

/*
 * @brief holds the current running processes and their open file descriptor tables.
 * Lives in kernel space, and only has one instance in the kernel. Should never be deallocated (unless shutting down?).
 */
struct proctable
{  
    struct proc** processes;            // Pointer to array of all running proccesses

    // will probably need a lock her for assignment 5

    struct lock* pid_lk;

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
 * 
 */
void 
pt_adjust_size(void);

/**
 * @brief addes a new process to the table. Should be used in Fork.
 * 
 * @param pr the new process
 * @param pid the new pid for this process
 * 
 * @warning pt_find_free_fd() should be called before to allow the parent function to check if its even relavent to call this function.
 * this function will still return an error if the pid is invalid
 * @return returns the pid of the added process
 */
int 
pt_add_proc(struct proc* pr, int pid);

/**
 * @brief when a process exits it should be removed from the table
 * 
 * @param pid pid of process to remove from the table
 */
void 
pt_remove_proc(int pid);

/**
 * @brief helper function to find a free location in a process's file descriptor table
 * 
 * @param fd a free index, used as a return value
 * 
 * @return compliance with errno, 0 on success
 */
int 
pt_find_free_fd(struct proc* pr, int* fd);

/**
 * @brief finds available pid
 * 
 * @return an available pid, if non is available returns MAX_PID_REACHED
 * @warning this should really only be called by other operations in the file
 */
int
pt_find_avail_pid(void);

/**
 * @brief checks if pid exists
 * 
 * @return true or false
 */
int
pt_check_pid(int pid);
#endif