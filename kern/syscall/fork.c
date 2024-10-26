#include <types.h>
#include <kern/errno.h>
#include <synch.h>
#include <proc.h>
#include <mips/trapframe.h>
#include <addrspace.h>
#include <proctable.h>
#include <current.h>
#include <filetable.h>
#include <syscall.h>
#include <copyinout.h>



/**
 * Function prototypes
 */
//static int copy_trapframe(struct trapframe* old_tf, struct trapframe* new_tf);
static void child_return(void* data1, unsigned long data2);


/** 
 * @brief system-level function for forking a process 
 *
 * @param tf user-space pointer to the current process' trapframe upon exception entry
 * 
 * @return 0 on success, error code on failure
 */
int
sys_fork(struct trapframe tf, int *retval)
{
    int err;
    pid_t pid;
    char proc_name[20];
    struct trapframe parent_tf; 
    struct proc *new_proc;

    // lock the proctable 
    lock_acquire(kproc_table->pid_lk);

    pid = pt_find_avail_pid(); // No point in doing anything if there is no available one
    if (pid == MAX_PID_REACHED)
    {
        lock_release(kproc_table->pid_lk);
        return ENPROC;
    }

    // TODO Assignment 5: do we need to copyin a trapframe? 
    // copy user-level trapframe to kernel stack var 
    // err = copyin((const_userptr_t)tf, &parent_tf, sizeof(struct trapframe));
    // if (err) 
    // {
    //     lock_release(kproc_table->pid_lk);
    //     return err;
    // }

    parent_tf = tf;

    // check user doesn't already have too many processes
    // if already too many (for this user), return EMPROC, *not* ENPROC
    // TODO Assignment 5: Double check - there seems to be no user-process limit

    // create string called "process {pid}" as a stack var
    snprintf(proc_name, 20, "process %d", pid);

    // create proc
    new_proc = proc_create_runprogram(proc_name);
    if (new_proc == NULL) { 
        lock_release(kproc_table->pid_lk);
        return ENOMEM; // ran out of space when kmalloc-ing proc
    }

    // 1. copy address space
    err = as_copy(curproc->p_addrspace, &new_proc->p_addrspace);
    if (err) {
        lock_release(kproc_table->pid_lk);
        proc_destroy(new_proc);
        return ENOMEM;
    }
    // 2. copy file table 
    // TODO Assignment 5: Does curproc need to be copyin'd???
    err = __copy_fd_table(curproc, new_proc);
    if (err) {
        lock_release(kproc_table->pid_lk);
        proc_destroy(new_proc);
        return ENOMEM;
    }

    // 3. copy architectural state - in tf
    // struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
    // if (child_tf == NULL) {
    //     lock_release(kproc_table->pid_lk);
    //     proc_destroy(new_proc);
    //     return ENOMEM;
    // }

    // err = copy_trapframe(&parent_tf, child_tf);
    // if (err) {
    //     lock_release(kproc_table->pid_lk);
    //     proc_destroy(new_proc);
    //     kfree(child_tf);
    //     return err;
    // }

    // add to proctable
    err = pt_add_proc(new_proc, pid);
    if (err) {
        lock_release(kproc_table->pid_lk);
        proc_destroy(new_proc);
        return ENPROC;
    }

    // 4. copy kernel thread 
    // entrypoint: enter_forked_process(struct trapframe *tf)
    // arg: tf
    // to user this function we must have a function (void*, unsigned long)
    // TODO: change thread name to be unique 
    err = thread_fork("forked thread", 
                new_proc,
                child_return,
                &parent_tf,
                0);

    if (err) {
        pt_remove_proc(new_proc);
        lock_release(kproc_table->pid_lk);
        proc_destroy(new_proc);
        return err;
    }

    // now that thread_fork has been called, only the parent thread executes the following
    // return child pid (only parent runs this)
    *retval = pid;
    return 0;
}
/**
 * @brief a thread_fork() compatible function to run a new thread in the child and return
 * Should only actually activate things in the child here
 */
static
void
child_return(void* data1, unsigned long data2)
{
    (void) data2;
    struct trapframe* tf = (struct trapframe*) data1;

    as_activate();  // Activates the new address space for the process

    enter_forked_process(tf);
}
// /**
//  * @brief private function for copying trapframe 
//  * 
// */
// static int 
// copy_trapframe(struct trapframe* old_tf, struct trapframe* new_tf)
// {
//     if (old_tf == NULL || new_tf == NULL)
//     {
//         return EINVAL;
//     }

//     new_tf->tf_vaddr = old_tf->tf_vaddr;
//     new_tf->tf_status = old_tf->tf_status;
//     new_tf->tf_cause = old_tf->tf_cause;
//     new_tf->tf_lo = old_tf->tf_lo;
//     new_tf->tf_hi = old_tf->tf_hi;
//     new_tf->tf_ra = old_tf->tf_ra;
//     new_tf->tf_at = old_tf->tf_at;
//     new_tf->tf_v0 = old_tf->tf_v0;
//     new_tf->tf_v1 = old_tf->tf_v1;
//     new_tf->tf_a0 = old_tf->tf_a0;
//     new_tf->tf_a1 = old_tf->tf_a1;
//     new_tf->tf_a2 = old_tf->tf_a2;
//     new_tf->tf_a3 = old_tf->tf_a3;
//     new_tf->tf_t0 = old_tf->tf_t0;
//     new_tf->tf_t1 = old_tf->tf_t1;
//     new_tf->tf_t2 = old_tf->tf_t2;
//     new_tf->tf_t3 = old_tf->tf_t3;
//     new_tf->tf_t4 = old_tf->tf_t4;
//     new_tf->tf_t5 = old_tf->tf_t5;
//     new_tf->tf_t6 = old_tf->tf_t6;
//     new_tf->tf_t7 = old_tf->tf_t7;
//     new_tf->tf_s0 = old_tf->tf_s0;
//     new_tf->tf_s1 = old_tf->tf_s1;
//     new_tf->tf_s2 = old_tf->tf_s2;
//     new_tf->tf_s3 = old_tf->tf_s3;
//     new_tf->tf_s4 = old_tf->tf_s4;
//     new_tf->tf_s5 = old_tf->tf_s5;
//     new_tf->tf_s6 = old_tf->tf_s6;
//     new_tf->tf_s7 = old_tf->tf_s7;
//     new_tf->tf_t8 = old_tf->tf_t8;
//     new_tf->tf_t9 = old_tf->tf_t9;
//     new_tf->tf_k0 = old_tf->tf_k0;
//     new_tf->tf_k1 = old_tf->tf_k1;
//     new_tf->tf_gp = old_tf->tf_gp;
//     new_tf->tf_sp = old_tf->tf_sp;
//     new_tf->tf_s8 = old_tf->tf_s8;
//     new_tf->tf_epc = old_tf->tf_epc;

//     return 0;
// }