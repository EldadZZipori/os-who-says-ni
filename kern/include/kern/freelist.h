
#ifndef _FREELIST_H_
#define _FREELIST_H_

struct freelist_node {
	paddr_t paddr;
	size_t sz;
    bool allocated;
    int otherpages;
	struct freelist_node *next;
    struct freelist_node *prev;    
};

struct freelist {
    paddr_t start; 
    paddr_t end;
	struct freelist_node *head;
    struct lock* fl_lk;
};

/* Function definitions */
struct freelist* freelist_create(paddr_t start, paddr_t end); 
struct freelist_node *freelist_node_create(struct freelist_node *prev, struct freelist_node *next);
void freelist_destroy(struct freelist *fl);
paddr_t freelist_get_first_fit(struct freelist *fl, size_t sz);
void freelist_remove(struct freelist *fl, paddr_t blk, size_t sz);
struct freelist* freelist_copy(struct freelist *src, struct freelist *dst);
struct freelist_node* freelist_get_node(struct freelist *fl, vaddr_t addr);
void freelist_node_set_otherpages(struct freelist_node *n, int otherpages);
int freelist_node_get_otherpages(struct freelist_node *n);


#endif /* _VM_H_ */
