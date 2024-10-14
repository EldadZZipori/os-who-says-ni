#Todo

## October 11th 2024
- [ ] Make process table struct
- [ ] Create file struct
- [ ] Create file table struct
- [ ] Init file table when system boots in kmain() in main.c
- [ ] Add syscalls in switch case in kern/arch/mips/syscall/syscall.c


Observations for assignment 5
1. We will need a processes table 
2. Track the current process that makes the systemcall (PID?)


## October 14th 2024
1. Just creeate a proccess table and make sure that after bootstrap we can see it in gdb, with the kernel proccess.