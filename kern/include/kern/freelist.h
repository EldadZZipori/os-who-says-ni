
#ifndef _FREELIST_H_
#define _FREELIST_H_

struct freelist_node {
	void *addr;
	size_t size;
	struct freelist_node *next;
    struct freelist_node *prev;
    
    struct lock fl_lk;
};

struct freelist {
    void *start; 
    void *end;
	struct freelist_node *head;
}

/* Function definitions */
struct freelist* freelist_create(void* start, void* end); 
void freelist_destroy(struct freelist *fl);
vaddr_t freelist_get_first_fit(struct freelist *fl, size_t size);
void freelist_remove(void *blk, size_t sz);


#endif /* _VM_H_ */
