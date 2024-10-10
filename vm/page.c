#include "vm/page.h"

// 해시 함수
uint64_t vm_hash_func(const struct hash_elem *e, void *aux) {
    struct vm_entry *i = hash_entry(e, struct vm_entry, elem);
    return hash_int(i->vaddr);
}

// 비교 함수
bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
    struct vm_entry *item_a = hash_entry(a, struct vm_entry, elem);
    struct vm_entry *item_b = hash_entry(b, struct vm_entry, elem);
    return item_a->vaddr < item_b->vaddr; 
}