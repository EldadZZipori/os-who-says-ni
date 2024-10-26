#include <types.h>
#include <proc.h>
#include <thread.h>
#include <syscall.h>
#include <current.h>

int
sys_getpid(int* retval)
{
    *retval = curproc->my_pid;
    return 0;
}