#include <types.h>
#include <proc.h>
#include <synch.h>
#include <abstractfile.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <stat.h>
#include <vfs.h>
#include <current.h>
#include <filetable.h>

#define STD_DEVICE "con:"

// Make sure to destroy
void 
ft_bootstrap() 
{

    /* Allocation of all memory needed for a file */
    kfile_table = (struct filetable*)kmalloc(sizeof(struct filetable));

    if (kfile_table == NULL) 
    { 
        panic("Could not create file table\n");
    }

    kfile_table->files = (struct abstractfile**)kmalloc(FILETABLE_INIT_SIZE*sizeof(struct abstractfile*));
    if (kfile_table->files == NULL) 
    {
        panic("Could not create file table\n");
    }

    // kfile_table->files_lk = (struct lock**)kmalloc(FILETABLE_INIT_SIZE*sizeof(struct lock*));
    // if (kfile_table->files_lk == NULL) 
    // {
    //     panic("Could not create file table\n");
    // }

    kfile_table->location_lk = lock_create("ft lock");
    if(kfile_table->location_lk == NULL)
    {
        panic("cannot create location lock for file table");
    }

	//char *ft_loc = (char *) kmalloc(32 * sizeof(char));
	// for (unsigned int i = 0; i < FILETABLE_INIT_SIZE; i++)
	// {
	// 	//snprintf(ft_loc, 32, "kfile_table lock %d", i);
	// 	kfile_table->files_lk[i] = lock_create("file_table location lock");

	// 	if (kfile_table->files_lk[i] == NULL)
	// 	{
	// 		panic("Creating locks for file table failed\n");
	// 	}
	// }
    
    /* Make sure to track the size of the file - room to make it dynamic in the future */
    kfile_table->curr_size = FILETABLE_INIT_SIZE;

    /* Create the standard input/out/error for all processes */
    kfile_table->files_counter = 3; 

    struct abstractfile* stdin = NULL;
    struct abstractfile* stdout = NULL;
    struct abstractfile* stderr = NULL;

    // FIX!!! BAD FIX HERE - DONT KNOW HOW HELP
    
    char device1[] = "con:";
    char device2[] = "con:";
    char device3[] = "con:";

    if( __open(device1, O_RDONLY, &stdin))
    {
        panic("Could not open std");
    }

    if(__open(device2, O_WRONLY, &stderr))
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

    
}

void 
ft_destroy(struct filetable* ft)
{
    (void)ft;
    unsigned int i;
    // for (i = 0; i < ft->curr_size; i++)
    // {
    //     lock_destroy(ft->files_lk[i]);
    // }

    for (i = 0; i < ft->files_counter; i++)
    {
        af_destroy(&ft->files[i]);
    }

    lock_destroy(kfile_table->location_lk);
    
    kfree(ft);

}


void 
ft_adjust_size(void) 
{
    return;
}

int 
ft_add_file(struct abstractfile** file, int* location) 
{
    KASSERT(kfile_table != NULL); 
    KASSERT(kfile_table->files != NULL);
    
    // TODO: add check for a valid file
    
    if ((kfile_table->files_counter % FILETABLE_INIT_SIZE )== 0)
    {
        ft_adjust_size();
    }

    lock_acquire(kfile_table->location_lk);
    /* Find an empty spot in the table */
    for(unsigned int i = 0; i < kfile_table->curr_size; i++)
    {
        if(kfile_table->files[i] == NULL)
        {
            kfile_table->files[i] = *file;
            *location = i;     // Exit the loop
            break;
        }
        else if(i == (kfile_table->curr_size -1))
        {
            lock_release(kfile_table->location_lk);
            /* Either have this or make a table that dynamiclly adjusts size */
            return ENFILE;
        }
    }
    
    kfile_table->files_counter++;
    lock_release(kfile_table->location_lk);

    return 0;

}

void 
ft_remove_file(unsigned int index) 
{
    KASSERT(kfile_table != NULL); 
    KASSERT(kfile_table->files != NULL);
    KASSERT(index < kfile_table->curr_size);

    /*
     * Completely remove the file from the table 
     */

    // No need to decrease ref on the vnode as vfs_close does that
    af_destroy(&kfile_table->files[index]);
    kfile_table->files[index] = NULL;

    lock_acquire(kfile_table->location_lk);
    kfile_table->files_counter--; // This should be the only place that files_counter of the main file table decreases
    lock_release(kfile_table->location_lk);

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

int
__close(struct proc* cur_proc, int fd)
{
    // check if the file descriptor is valid
    int result;
    result = __check_fd(fd);
    if (result)
    {
        return result;
    }

    int index_in_fd = cur_proc->fdtable[fd];

    if (index_in_fd == FDTABLE_EMPTY)
    {
        return EBADF;
    }
    /*
     * Removes the file descriptor for this process only
     * Makes it available to reuse.
     * Note that at this point we should have the lock for the current process's file table
     */ 
    cur_proc->fdtable[fd] = FDTABLE_EMPTY; 
    cur_proc->fdtable_num_entries--; // This shuld be the only place where the entries count of the individual process decreases

    // We probably want to lock this index before writing to it
    //lock_acquire(kfile_table->files_lk[index_in_fd]);
    lock_acquire(kfile_table->location_lk);
    kfile_table->files[index_in_fd]->ref_count --; // this should be the only place where ref_count of an absreact file decreases
    lock_release(kfile_table->location_lk);
    //lock_release(kfile_table->files_lk[index_in_fd]);
    /* 
     *  We are assuming here that the vfs sructre knows to remove the v-node 
     *  once it has no references.
     */
    if (kfile_table->files[index_in_fd]->vn != NULL)
    {
        vfs_close(kfile_table->files[index_in_fd]->vn);
    }
    

    if (kfile_table->files[index_in_fd]->ref_count == 0)
    {
        ft_remove_file(index_in_fd);
    }
    return 0;
}