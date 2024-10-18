#include <types.h>
#include <proc.h>
#include <synch.h>
#include <abstractfile.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <stat.h>
#include <vfs.h>
#include <filetable.h>

#define STD_DEVICE "con:"

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

    /* STD input/out/error should be opened */
    kfile_table->files_counter = 3; 

    struct abstractfile* stdin = NULL;
    struct abstractfile* stdout = NULL;
    struct abstractfile* stderr = NULL;

    //char *device = kmalloc(4*sizeof(char));
    //strcpy(device, "con:");

    // FIX!!! BAD FIX HERE
    
    char device1[] = "con:";
    char device2[] = "con:";
    char device3[] = "con:";

    if( __open(device1, O_RDONLY, &stdin))
    {
        panic("Could not open std");
    }

    if(__open(device2, O_RDONLY, &stderr))
    {
        panic("Could not open std");
    }
    if(__open(device3, O_WRONLY, &stdout))
    {
        panic("Could not open std");
    }

    kfile_table->files[0] = stdin;
    kfile_table->files[1] = stdout;
    kfile_table->files[2] = stderr;
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
            *location = i;     // Exit the loop
            break;
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
ft_remove_file(unsigned int index) 
{
    KASSERT(kfile_table != NULL); 
    KASSERT(kfile_table->files != NULL);
    KASSERT(index < kfile_table->curr_size);

    kfile_table->files[index] = NULL;

    kfile_table->files_counter--;
}

int 
__open(char kpath[__PATH_MAX], int flags, struct abstractfile** af)
{
    int result = 0;
    struct vnode* vn;
    /* 
     * As per man page ignoring mode for OS161
     */
    result = vfs_open(kpath, flags, 0, &vn);
    if (result)
    {
        return result;
    }

    result = af_create(flags, vn, af);
    if (result)
    {
        vfs_close(vn);
        return result;
    }

    /* When the file is in append mode set the offset to the end of the file */
    if (flags & O_APPEND)
    {
        struct stat st;   

        //  vop_stat        - Return info about a file. The pointer is a
        //                    pointer to struct stat; see kern/stat.h.
        //  See vnode.h
        result = VOP_STAT(vn, &st);
        if (result)
        {
            vfs_close(vn);
            af_destroy(af);
            return result;
        }

        (*af)->offset = st.st_size;
    }
    
    return 0;
}