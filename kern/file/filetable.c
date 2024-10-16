#include <types.h>
#include <proc.h>
#include <synch.h>
#include <kern/errno.h>
#include <filetable.h>

// Make sure to destroy
void 
ft_bootstrap() 
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

	char *ft_loc = (char *) kmalloc(32 * sizeof(char));
	for (unsigned int i = 0; i < FILETABLE_INIT_SIZE; i++)
	{
		snprintf(ft_loc, 32, "kfile_table lock %d", i);
		kfile_table->files_lk[i] = lock_create(ft_loc);

		if (kfile_table->files_lk[i] == NULL)
		{
			panic("Creating locks for kproc failed\n");
		}
	}
    

    kfile_table->curr_size = FILETABLE_INIT_SIZE;
    kfile_table->files_counter = 0;
    // TODO: set all things to NULL
}

void 
ft_destroy(struct filetable* ft)
{
    (void)ft;
}


void 
ft_adjust_size(void) 
{

}

int 
ft_add_file(struct abstractfile* file, int* location) 
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
            *location = kfile_table->curr_size;     // Exit the loop
        }
        else if(i == (kfile_table->curr_size -1))
        {
            /* Either have this or make a table that dynamiclly adjusts size */
            return ENFILE;
        }
    }
    
    kfile_table->files_counter++;
    
    return 0;

}

void 
ft_remove_file(unsigned int fd) 
{
    KASSERT(kfile_table != NULL); 
    KASSERT(kfile_table->files != NULL);
    KASSERT(fd < kfile_table->curr_size);

    kfile_table->files[fd] = NULL;
}