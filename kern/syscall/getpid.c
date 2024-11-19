#include <types.h>
#include <proc.h>
#include <thread.h>
#include <syscall.h>
#include <proctable.h>
#include <synch.h>
#include <current.h>

int
sys_getpid(int* retval)
{
    lock_acquire(kproc_table->pid_lk);
    *retval = curproc->p_pid;
    lock_release(kproc_table->pid_lk);
    return 0;
}