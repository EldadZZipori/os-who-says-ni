
#ifndef _FREELIST_H_
#define _FREELIST_H_

struct freelist_node {
	vaddr_t addr;
	size_t size;
	struct freelist_node *next;
    struct freelist_node *prev;
    
    struct lock fl_lk;
};

struct freelist {
	struct freelist_node *head;
}

/* Function definitions */
struct freelist* freelist_create(vaddr_t start, vaddr_t end); 
void freelist_destroy(struct freelist *fl);
vaddr_t freelist_get_first_fit(struct freelist *fl, size_t size);


#endif /* _VM_H_ */
