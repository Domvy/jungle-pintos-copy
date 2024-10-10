#include <stdint.h>
#include <stddef.h>
#include "list.h"
#include "hash.h"
#include <stdbool.h>

struct vm_entry {
    uint8_t type;
    void *vaddr;
    bool writable;
    bool is_loaded;
    struct file *file;
    struct list_elem mmap_elem;
    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;
    size_t swap_slot;
    struct hash_elem elem;
};


uint64_t vm_hash_func(const struct hash_elem *e, void *aux);
bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);