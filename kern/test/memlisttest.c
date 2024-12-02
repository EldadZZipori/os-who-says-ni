#include <types.h>
#include <kern/memlist.h>
#include <lib.h>
#include <spinlock.h>
#include <kern/errno.h>
#include <test.h>

static void memlist_test_assert(bool condition, const char *message) {
    if (!condition) {
        kprintf("FAIL: %s\n", message);
        panic("memlist test failed.\n");
    } else {
        kprintf("PASS: %s\n", message);
    }
}

int memlisttest(int nargs, char **args) {

    (void)nargs;
    (void)args;

    // Define a test memory range
    paddr_t start = 0x1000;
    paddr_t end = 0x2000;

    kprintf("Starting memlist test...\n");

    // Test memlist_create
    struct memlist *ml = memlist_create(start, end);
    memlist_test_assert(ml != NULL, "memlist creation");
    memlist_test_assert(ml->start == start, "memlist start address");
    memlist_test_assert(ml->end == end, "memlist end address");
    memlist_test_assert(ml->head != NULL, "memlist head node creation");
    memlist_test_assert(ml->head->sz == (end - start), "memlist initial size");

    // Test allocation (Case 1: Perfect fit)
    paddr_t alloc1 = memlist_get_first_fit(ml, 0x500);
    memlist_test_assert(alloc1 == start, "Allocation (perfect fit)");
    memlist_test_assert(ml->head->allocated == true, "Allocated block marked correctly");

    // Test allocation (Case 2: Partial fit with split)
    paddr_t alloc2 = memlist_get_first_fit(ml, 0x200);
    struct memlist_node *alloc2_node = memlist_get_node(ml, alloc2);
    memlist_test_assert(alloc2 != 0, "Allocation (partial fit)");
    memlist_test_assert(alloc2_node != NULL, "Node for allocated block found");
    memlist_test_assert(alloc2_node->sz == 0x200, "Block size after split");
    memlist_test_assert(alloc2_node->allocated == true, "Allocated block marked correctly");
    memlist_test_assert(alloc2_node->next != NULL, "Split block created");

    // Test deallocation (no merge)
    memlist_remove(ml, alloc2);
    memlist_test_assert(alloc2_node->allocated == false, "Block deallocated (no merge)");

    // Test deallocation (merge with previous)
    memlist_remove(ml, alloc1);
    memlist_test_assert(ml->head->allocated == false, "Block deallocated (merge with previous)");
    memlist_test_assert(ml->head->sz == 0x1000, "Size after merge with previous");

    // test allocate 0x1000 (4096B) in chunks of 0x100 (256B chunks)
    for (int i = 0; i < 16; i++) {
        paddr_t alloc = memlist_get_first_fit(ml, 0x100);
        memlist_test_assert(alloc != 0, "Allocation in 64B chunks");
    }

    // free all 64B chunks 
    for (int i = 0; i < 16; i++) {
        memlist_remove(ml, start + i * 0x100);
    }

    // Test edge case: Allocate larger than available
    paddr_t alloc_fail = memlist_get_first_fit(ml, 0x1200);
    memlist_test_assert(alloc_fail == 0, "Allocation fails for oversized request");

    // Test memlist_copy
    struct memlist *ml_copy = memlist_create(start, end);
    memlist_test_assert(memlist_copy(ml, ml_copy) != NULL, "memlist copy");
    memlist_test_assert(ml_copy->start == ml->start && ml_copy->end == ml->end, "memlist copy maintains bounds");

    // Test memlist_node_set_otherpages and memlist_node_get_otherpages
    memlist_node_set_otherpages(ml->head, 42);
    memlist_test_assert(memlist_node_get_otherpages(ml->head) == 42, "Otherpages set/get");

    // Cleanup
    memlist_destroy(ml);
    memlist_destroy(ml_copy);

    kprintf("Memlist test complete\n");

    return 0;
}
