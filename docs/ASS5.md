# October 22nd

## All work and commits were done together on a collab session

## fork()
### Requirements 
- create a new proc struct
    - proc_create()
    - return PID to the parent
    - if child, continue to next steps
- Copy address space. 
    - Every proc struct has the member addrspace *p_addrspace 
    - Use as_copy() copies an old addrspace to a new one
    - put in new proc struct
- Copy file table
    - Make a helper function for this []
    - Simply copy the indecies
    - make sure refcounts are updated accordingly
- Copy architectural state
    - This will be stored in trapframe after entering the syscall
    - Does this require to basiclly copy the threads of the original process?
    - Copy the trapframe 
- Copy kernel thread
    - Is this any different then copying the thread array?
    - Use thread_fork() ?
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