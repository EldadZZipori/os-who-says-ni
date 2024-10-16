#ifndef _FILE_TABLE_H_
#define _FILE_TABLE_H_

#define FILETABLE_INIT_SIZE 10   // The base size of the file table. Dynamiclly changes when full

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
 */
unsigned int 
ft_add_file(struct abstractfile* file);

/**
 * @brief Removes file at specified index from open file table. 
 *        Requires that fd is non-negative and less than the file 
 *        table size. 
 * 
 * @param fd: A non-negative file descriptor, returned by some call 
              to open(), dup2(), etc. 
              Question: What if another file closes? Could this be 
              an invalid fd? 
 */
void 
ft_remove_file(unsigned int fd);
#endif