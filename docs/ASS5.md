# October 22nd

## All work and commits were done together on a collab session

## fork()
### Requirements 
- create new `proc` struct (return `pid` to the parent)
- Copy address space. 
    - Does this include the stack? or just the heap?
    - Every proc struct has the member addrspace *p_addrspace 
    - Use as_copy() copies an old addrspace to a new one
    - put in new proc struct
- Copy file table
    - Make a helper function for this []
    - Simply copy the indices
    - make sure refcounts are updated accordingly
- Copy architectural state
    - This will be stored in trapframe after entering the syscall
        - trapframe should be on the stack if we copy the stack contents?
    - Does this require to basically copy the threads of the original process?
        - just copy the proc->p_threads?
            - threadarray contains all the pthreads, how do we differentiate from kernel thread and rest of threads?? Will this automatically be copied?
    - Copy the trapframe 
- Copy kernel thread
    - use `thread_fork`
        - will create thread, allocate a stack, copy the cpu field, add to the proc, 
        - this does not copy the stack though ...
    - needs to jump into a helper function that: 
        - copy the stack
        - ?? load in trap frame ?? (passed in as arg from prev step)
        - modify v0 to be zero (fork() return value)
        - manually jump back to usermode 
    - questions:
        - Is this any different then copying the thread array?
            - is the kernel thread also stored in curproc->pthreads
            - is the kernel thread the active thread? 
        - Use thread_fork() ?
            - if we do, we can set it to enter into a custom function that modifies register values potentially? then returns to usermode?
                - entry point takes arguments, so this is possible.
        - will this be the one currently running on the cpu? curthread?
            - copy curthread? 
            - do we start the child 
        - how do we not duplicate the kernel thread twice, if it is already in pthreads, and we do duplicate it using thread_fork?
- Return to user mode 
    - Need to return 0 to the new process 
    - Need to return pid to parent process 
    - enter_new_process() and mips_usermode()
    - child does not go through exeption-mips, only the parent does.
        - set the v0 of the parent in syscall.c
        - set the v0 of the child in the fork syscall.

# October 25th

## All work and commits were done together on a collab session

## Plan
- Create PID system - proctable
    - the index in the proctable->processes is the PID 
    - Find available PID - find_avail_PID() if proctable->processes == NULL is available   
    - Lock for the proctable structure 
    - add the PID to proc.h struct  
- proc_create("Procees [PID]") 
- as_copy() - don't need to implement. Implemented in dumbvm   
- copy the file table __copy_fd_table() 
- create a __copy_tf function.  
- copy the kernel thread - thread_fork()
    - this will always form the kernel thread on the kernel side
    - pass in our newly allocated trapframe from 'copy arch. state' step
    - the entry point is custom function which will do the following:
        - set tf->tf_v0 to 0
        - call mips_usermode, which will load tf into regs and jump to the 'resumed' instr. after fork() call
- call mips_usermode() to go into usermode - do this in enter_forked_process()
- NOTE: parent thread will continue execution in the lines after thread_fork() call, child will jump to execution elsewhere, and eventually return back to the pc.

# Novermber 3rd

## All work and commits were done together on a collab session

# 2024-11-04

## Tests Current status
- forktest [x]
- argtest [?]
- badcall 
    - waitpid: VOD_READ causes deadlock for some reason, NULL argument problem [x]
    - exec []
- bigfile [x]
- crash [x]
- farm: waiting for exec []
- faulter [x]
- multiexec: waiting for exec []
- bigexec: waiting for exec []