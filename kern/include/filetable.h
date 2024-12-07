#ifndef _FILE_TABLE_H_
#define _FILE_TABLE_H_

/* 
 * For now this is the amount of  total files we are allowing in our system. 
 * would be better to dynamiclly change the size of the table.
 */
#define FILETABLE_INIT_SIZE 20

struct filetable* kfile_table;

/*
 * @brief represents the open files table
 */
struct 
filetable
{  
    struct abstractfile** files;      // All files open on the file table

    unsigned int curr_size;

    unsigned int files_counter;

    struct lock** files_lk;

    struct lock* location_lk; // This will only be used by open 
};

/**
 * @brief Initializes the open file table for the kernel use
 */
void 
ft_bootstrap(void);

void 
ft_destroy(struct filetable* ft);

/**
 * @brief Dynamiclly changes the size of kfile_talbe to allow for more open files
 */
void 
ft_adjust_size(void);

/**
 * @brief Adds a file to the open file table
 * 
 * @param file : A non-NULL file that has just been open
 * 
 * @param location: location of the file in the filetable (populated by the function)
 * 
 * @return compliance with errno, 0 on success
 */
int 
ft_add_file(struct abstractfile** file, int* location);

/**
 * @brief Removes file at specified index from open file table. 
 *        Requires that fd is non-negative and less than the file 
 *        table size. 
 * 
 * @param index: A non-negative file descriptor, returned by some call 
              to open(), dup2(), etc. 
              Question: What if another file closes? Could this be 
              an invalid fd? 
 */
void 
ft_remove_file(unsigned int index);


/** 
 * @brief this is a helper function to open file internally in the kernel. the systemcall for open
 * will reuse this code so make sure it is safe to use in the kernel before calling it.
 * 
 * Whoever calls this function should have the lock on the current process's file table
 * 
 * @param kpath path to file
 * @param flags how to open the file
 * @param af the returned high level file will be returned through this variable.
 * 
 * @return returns 0 on success, on error comply with errno
 */
int 
__open(char* kpath, int flags, struct abstractfile** af);

/** 
 * @brief this is a helper function to close file internally in the kernel. the systemcall for open
 * will reuse this code so make sure it is safe to use in the kernel before calling it.
 * 
 * Whoever calls this function should have the lock on the current process's file table, or is doing it
 * should already have the lock for the process's file table.
 * 
 * @param fs the filde descriptor of the file. Will be selected from the current running process
 * 
 * @return returns 0 on success, on error comply with errno
 */
int
__close(struct proc* cur_proc, int fd);
#endif