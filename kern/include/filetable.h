#ifndef _FILE_TABLE_H_
#define _FILE_TABLE_H_

#define BASE_SIZE 10   // The base size of the file table. Dynamiclly changes when full

const struct filetable* kern_filetable;

/*
 * @brief represents the open files table
 */
struct filetable
{  
    struct file* table[BASE_SIZE];      // All files open on the file table

    unsigned int curr_size;

    unsigned int files_counter;
};

void ft_bootstrap();
void ft_adjust_size();
void ft_add_file(struct file* file);
void ft_remove_file(struct file* file);
#endif