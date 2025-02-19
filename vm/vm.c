/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"
#include "stddef.h"
#include <threads/mmu.h>
#include <stdlib.h>

static struct list frame_table;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init( void ) {
    vm_anon_init();
    vm_file_init();
#ifdef EFILESYS /* For project 4 */
    pagecache_init();
#endif
    register_inspect_intr();
    /* DO NOT MODIFY UPPER LINES. */
    list_init( &frame_table );
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type page_get_type( struct page *page ) {
    int ty = VM_TYPE( page->operations->type );
    switch ( ty ) {
        case VM_UNINIT:
            return VM_TYPE( page->uninit.type );
        default:
            return ty;
    }
}

/* Helpers */
static struct frame *vm_get_victim( void );
static bool vm_do_claim_page( struct page *page );
static struct frame *vm_evict_frame( void );

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer( enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux ) {
    ASSERT( VM_TYPE( type ) != VM_UNINIT )

    struct supplemental_page_table *spt = &thread_current()->spt;

    /* Check wheter the upage is already occupied or not. */
    if ( spt_find_page( spt, upage ) == NULL ) {
        struct page *page = (struct page *)malloc( sizeof( struct page ) );

        typedef bool ( *initializerFunc )( struct page *, enum vm_type, void * );
        initializerFunc initializer = NULL;

        switch ( VM_TYPE( type ) ) {
            case VM_ANON:
                initializer = anon_initializer;
                break;
        }

        uninit_new( page, upage, init, type, aux, initializer );
        page->writable = writable;

        return spt_insert_page( spt, page );
    }
err:
    return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page( struct supplemental_page_table *spt UNUSED, void *va UNUSED ) {
    struct page *page = (struct page *)malloc( sizeof( struct page ) );
    page->va = pg_round_down( va );
    struct hash_elem *e = hash_find( &spt->spt_hash, &page->hash_elem );
    free( page );

    return e != NULL ? hash_entry( e, struct page, hash_elem ) : NULL;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page( struct supplemental_page_table *spt UNUSED, struct page *page UNUSED ) {
    return hash_insert( &spt->spt_hash, &page->hash_elem ) ? false : true;
}

void spt_remove_page( struct supplemental_page_table *spt, struct page *page ) {
    vm_dealloc_page( page );
    return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim( void ) {
    struct frame *victim = NULL;
    /* TODO: The policy for eviction is up to you. */

    return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame( void ) {
    struct frame *victim UNUSED = vm_get_victim();
    /* TODO: swap out the victim and return the evicted frame. */

    return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame( void ) {
    struct frame *frame = (struct frame *)malloc( sizeof( struct frame ) );
    ASSERT( frame != NULL );

    frame->kva = palloc_get_page( PAL_USER | PAL_ZERO );

    if ( frame->kva == NULL )
        frame = vm_evict_frame();
    else
        list_push_back( &frame_table, &frame->frame_elem );

    frame->page = NULL;
    ASSERT( frame->page == NULL );

    return frame;
}

/* Growing the stack. */
static bool vm_stack_growth() {
    bool success = false;
    void *upage = thread_current()->stack_allocated_boundary - PGSIZE;

    if ( vm_alloc_page( VM_ANON, upage, true ) ) {
        success = vm_claim_page( upage );

        if ( success ) {
            thread_current()->stack_allocated_boundary = upage;
            return true;
        }
    }

    return false;
}

/* Handle the fault on write_protected page */
static bool vm_handle_wp( struct page *page UNUSED ) {}

/* Return true on success */
bool vm_try_handle_fault( struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED ) {
    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
    struct page *page = spt_find_page( &thread_current()->spt, addr );

    if ( addr == NULL || is_kernel_vaddr( addr ) )
        return false;

    if ( !page ) {
        void *stack_pointer = user ? f->rsp : thread_current()->stack_allocated_boundary;
        if ( stack_pointer - 8 <= addr && addr <= USER_STACK && addr >= ( 1 << 20 ) )
            return vm_stack_growth();
        return false;
    }

    return vm_do_claim_page( page );
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page( struct page *page ) {
    destroy( page );
    free( page );
}

/* Claim the page that allocate on VA. */
bool vm_claim_page( void *va UNUSED ) {
    struct page *page = spt_find_page( &thread_current()->spt, va );

    if ( page == NULL )
        return false;

    return vm_do_claim_page( page );
}

/* Claim the PAGE and set up the mmu. */
static bool vm_do_claim_page( struct page *page ) {
    struct frame *frame = vm_get_frame();

    /* Set links */
    frame->page = page;
    page->frame = frame;

    /* TODO: Insert page table entry to map page's VA to frame's PA. */
    if ( !pml4_set_page( thread_current()->pml4, page->va, frame->kva, page->writable ) )
        return false;

    return swap_in( page, frame->kva );  // uninit_initialize
}

#include "hash.h"

// custom 함수
uint64_t hash_func( const struct hash_elem *e, void *aux ) {
    const struct page *p = hash_entry( e, struct page, hash_elem );

    return hash_bytes( &p->va, sizeof( p->va ) );
}

/** Project 3: Memory Management - 오름차순 정렬 */
bool less_func( const struct hash_elem *a, const struct hash_elem *b, void *aux ) {
    const struct page *pa = hash_entry( a, struct page, hash_elem );
    const struct page *pb = hash_entry( b, struct page, hash_elem );

    return pa->va < pb->va;
}

/* Initialize new supplemental page table */
void supplemental_page_table_init( struct supplemental_page_table *spt UNUSED ) {
    hash_init( &spt->spt_hash, hash_func, less_func, NULL );
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy( struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED ) {}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill( struct supplemental_page_table *spt UNUSED ) {
    /* TODO: Destroy all the supplemental_page_table hold by thread and
     * TODO: writeback all the modified contents to the storage. */
}
