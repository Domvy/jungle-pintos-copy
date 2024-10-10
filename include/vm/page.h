#include <stdbool.h>
#include <stdint.h>
#include "hash.h"

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

bool insert_vme( struct hash *vm, struct vm_entry *vme );
bool delete_vme( struct hash *vm, struct vm_entry *vme );
struct vm_entry *find_vme( void *vaddr );

uint64_t vm_hash_func( const struct hash_elem *e, void *aux );
bool vm_less_func( const struct hash_elem *a, const struct hash_elem *b, void *aux );

void vm_destroy( struct hash *vm );
