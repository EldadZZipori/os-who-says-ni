# 2024-11-7

## Readings 
[MMU and TLB](http://www.os161.org/documentation/sys161-2.0.2/mips.html)
[shootdown](https://people.ece.ubc.ca/~os161/download/tlb-shootdown.txt)

Todo:

1. Decide on the structure of the page table
    - 20 bit page number
    - global: 1 bit; if set, ignore the pid bits in the TLB.
    - valid: 1 bit; set if the TLB entry contains a valid translation.
    - dirty: 1 bit; enables writing to the page referenced by the entry; if this bit is 0, the page is only accessible for reading.
    - nocache: 1 bit; unused in System/161. In a real processor, indicates that the hardware cache will be disabled when accessing this page.
    - pid: 6 bits; a process or address space ID that can be used to allow entries to remain in the TLB after a process switch.
2. Look at the requirnments for sbrk()
3. Pick our favorite page table replacement algorithem
4. Look at tests km1-km4


# 2024-11-11

Notes:
1. Create macros extract data from a page table

Office Hourse Question:
1. What is the best division to choose for the virtual addresses 
    - 20 bit virtual page table 


# 2024-11-29

- [OS161 TLB Miss and Page Fault](http://jhshi.me/2012/04/27/os161-tlb-miss-and-page-fault/index.html)
