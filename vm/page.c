#include <stdbool.h>
#include <stdint.h>
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "vm/uninit.h"
#include "vm/page.h"
#include "hash.h"

void init_vme( struct vm_entry *vme ) {
    // TODO: list_elme 멤버변수 확인
    vme->type = VM_UNINIT;
    vme->vaddr = NULL;
    vme->writable = false;
    vme->is_loaded = false;
    vme->file = NULL;
    vme->offset = 0;
    vme->read_bytes = 0;
    vme->zero_bytes = 0;
    vme->swap_slot = 0;
}

bool insert_vme( struct hash *vm, struct vm_entry *vme ) {
    // hash_insert 함수가 NULL이 아닌 값을 반환하면,
    // 즉 기존에 같은 요소가 있으면 true 반환.
    // 기존에 같은 요소가 없다면 삽입 후 NULL 반환하므로 false 반환.
    if ( hash_insert( vm, &( vme->elem ) ) == NULL ) {
        return true;
    } else {
        return false;
    }
}

bool delete_vme( struct hash *vm, struct vm_entry *vme ) {
    struct hash_elem *found = hash_delete( vm, &vme->elem );

    if ( found != NULL )
        return true;
    else
        return false;
}

struct vm_entry *find_vme( void *vaddr ) {
    struct supplemental_page_table *spt = &( thread_current()->spt );
    uint64_t page_num = pg_round_down( vaddr );
    struct vm_entry temp_entry;
    temp_entry.vaddr = vaddr;

    struct hash_elem *found = hash_find( &spt->hash_table, &temp_entry.elem );

    if ( hash_empty( found ) ) return NULL;
    return hash_entry( found, struct vm_entry, elem );
}

uint64_t vm_hash_func( const struct hash_elem *e, void *aux ) {
    struct vm_entry *i = hash_entry( e, struct vm_entry, elem );
    return hash_int( i->vaddr );
}

bool vm_less_func( const struct hash_elem *a, const struct hash_elem *b, void *aux ) {
    struct vm_entry *item_a = hash_entry( a, struct vm_entry, elem );
    struct vm_entry *item_b = hash_entry( b, struct vm_entry, elem );
    return item_a->vaddr < item_b->vaddr;
}

void destructor_per_elem( struct hash_elem *e, void *aux ) { free( e ); }

void vm_destory( struct hash *vm ) { hash_destroy( vm, &destructor_per_elem ); }
