#include <types.h>
#include <filetable.h>

// Make sure to destroy
void ft_bootstrap() 
{
    kfile_table = (struct filetable*)kmalloc(sizeof(struct filetable));

    if (kfile_table == NULL) 
    { 
        panic("Could not create file table\n");
    }

    kfile_table->files = (struct abstractfile**)kmalloc(FILETABLE_INIT_SIZE*sizeof(struct abstractfile*));
    if (kfile_table->files == NULL) 
    {
        panic("Could not create proccess table\n");
    }

    kfile_table->files_lk = (struct lock**)kmalloc(FILETABLE_INIT_SIZE*sizeof(struct lock*));
    if (kfile_table->files_lk == NULL) 
    {
        panic("Could not create proccess table\n");
    }

    kfile_table->curr_size = FILETABLE_INIT_SIZE;
    kfile_table->files_counter = 0;
    // TODO: set all things to NULL
}

void ft_adjust_size(void) 
{

}

unsigned int ft_add_file(struct abstractfile* file) 
{
    KASSERT(kfile_table != NULL); 
    KASSERT(kfile_table->files != NULL);
    
    // TODO: add check for a valid file
    
    if ((kfile_table->files_counter % FILETABLE_INIT_SIZE )== 0)
    {
        ft_adjust_size();
    }

    /* Find an empty spot in the table */
    for(unsigned int i = 0; i < kfile_table->curr_size; i++)
    {
        if(kfile_table->files[i] == NULL)
        {
            kfile_table->files[i] = file;
            i = kfile_table->curr_size;     // Exit the loop
        }
    }
    
    kfile_table->files_counter++;
    
    return kfile_table->files_counter;

}

void ft_remove_file(unsigned int fd) 
{
    KASSERT(kfile_table != NULL); 
    KASSERT(kfile_table->files != NULL);
    KASSERT(fd < kfile_table->curr_size);

    kfile_table->files[fd] = NULL;
}