#Todo

## October 11th 2024
- [x] Make process table struct
- [?] Create file struct
- [x] Create file table struct
- [ ] Init file table when system boots in main() in main.c
- [ ] Add syscalls in switch case in kern/arch/mips/syscall/syscall.c


Observations for assignment 5
1. We will need a processes table 
2. Track the current process that makes the systemcall (PID?)


## October 14th 2024
Question  
1. Should all process' open file descriptor be updated everytime a call to `close()` is made? If processes A and B hold fd = 3, and B closes 3, should A's OFDT be updated before B can exit `close()`? a.k.a. should close() be blocked by trying to remove all instances of fd in the proctable? 
2. Does it make sense to have a lock array in the filetable for each of the file descriptors + have a lock for each file that is shared between all the files with the same

- [x] Just creeate a proccess table and make sure that after bootstrap we can see it in gdb, with the kernel proccess.
 
## October 15th 2024
- Worked together in a vscode livesession (this will show up as Eldad's work)

## October 16th 2024
- Worked together vscode livesession (this will show up as Eldad's work)

## October 17th 2024

#### Updated action items
- [ ] read() entry point in syscall.c
- [ ] write() entry point in syscall.c
- [ ] implement lseek() 
- [ ] lseek() entry point in syscall.c 

