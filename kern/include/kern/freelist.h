
#ifndef _FREELIST_H_
#define _FREELIST_H_

struct freelist_node {
	void *addr;
	size_t sz;
    bool allocated;
	struct freelist_node *next;
    struct freelist_node *prev;    
};

struct freelist {
    void *start; 
    void *end;
	struct freelist_node *head;
    struct lock* fl_lk;
};

/* Function definitions */
struct freelist* freelist_create(void* start, void* end); 
void freelist_destroy(struct freelist *fl);
void* freelist_get_first_fit(struct freelist *fl, size_t sz);
void freelist_remove(struct freelist *fl, void *blk, size_t sz);
struct freelist* freelist_copy(struct freelist *src, struct freelist *dst);


#endif /* _VM_H_ */
