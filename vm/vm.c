/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"
#include "vm/page.h"
#include "vm/uninit.h"
#include "userprog/process.h"
#include <list.h>

struct list frame_table;

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
    /* TODO: Your code goes here. */
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
        /* TODO: Create the page, fetch the initialier according to the VM type,
         * TODO: and then create "uninit" page struct by calling uninit_new. You
         * TODO: should modify the field after calling the uninit_new. */

        /* TODO: Insert the page into the spt. */
    }
err:
    return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page( struct supplemental_page_table *spt UNUSED, void *va UNUSED ) {
    struct vm_entry *vme = find_vme( va );
    struct page *page = vme->page;
    return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page( struct supplemental_page_table *spt UNUSED, struct page *page UNUSED ) {
    struct vm_entry *vme = find_vme( page->va );
    return insert_vme( &spt->vm, vme );
}

void spt_remove_page( struct supplemental_page_table *spt, struct page *page ) {
    struct vm_entry *vme = find_vme( page->va );
    delete_vme( &spt->vm, vme );
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

/* Growing the stack. */
static void vm_stack_growth( void *addr UNUSED ) {}

/* Handle the fault on write_protected page */
static bool vm_handle_wp( struct page *page UNUSED ) {}

/* Return true on success */
bool vm_try_handle_fault( struct intr_frame *f UNUSED, void *uva UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED ) {
    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
    struct vm_entry *vme = find_vme( uva );
    struct page *page = vme->page;
    /* TODO: Validate the fault */
    /* TODO: Your code goes here */

    return vm_do_claim_page( page );
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page( struct page *page ) {
    destroy( page );
    free( page );
}

/*
    va > (vm_claim_page) > page > (vm_do_claim_page) > frame > (swap_in) > disk
*/

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame( void ) {
    struct frame *frame = palloc_get_page( PAL_USER );

    if ( frame == NULL )
        PANIC( "TODO: palloc_get_page 할당 안됨" );

    list_insert( &frame_table, &frame->frame_elem );  // TODO: frame_table 전용 util 분리하기

    ASSERT( frame != NULL );
    ASSERT( frame->page == NULL );
    return frame;
}

/* Claim the page that allocate on VA. */
bool vm_claim_page( void *va UNUSED ) {
    struct vm_entry *vme = find_vme( va );
    struct page *page = vme->page;
    return vm_do_claim_page( page );
}

void load_file( void *kva, struct vm_entry *vme ) {
    // static bool load_segment( struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable ) {

    struct file *file = vme->file;
    off_t ofs = vme->offset;
    uint8_t *upage = vme->vaddr;
    uint32_t read_bytes = vme->read_bytes;
    uint32_t zero_bytes = vme->zero_bytes;
    bool writable = vme->writable;

    ASSERT( ( read_bytes + zero_bytes ) % PGSIZE == 0 );
    ASSERT( pg_ofs( upage ) == 0 );
    ASSERT( ofs % PGSIZE == 0 );

    file_seek( file, ofs );
    while ( read_bytes > 0 || zero_bytes > 0 ) {
        /* 이 페이지를 채우는 방법을 계산합니다.
         * FILE에서 PAGE_READ_BYTES 바이트를 읽고
         * 마지막 PAGE_ZERO_BYTES 바이트를 0으로 채웁니다. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        /* 메모리 페이지를 가져옵니다. */
        uint8_t *kpage = palloc_get_page( PAL_USER );
        if ( kpage == NULL ) return false;

        /* 이 페이지를 로드합니다. */
        if ( file_read( file, kpage, page_read_bytes ) != (int)page_read_bytes ) {
            palloc_free_page( kpage );
            return false;
        }
        memset( kpage + page_read_bytes, 0, page_zero_bytes );

        /* 프로세스의 주소 공간에 페이지를 추가합니다. */
        if ( !install_page( upage, kpage, writable ) ) {
            printf( "fail\n" );
            palloc_free_page( kpage );
            return false;
        }

        /* 다음으로 진행합니다. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
    }
    return true;
}

/* Claim the PAGE and set up the mmu. */
static bool vm_do_claim_page( struct page *page ) {  // kva = frame / uva = page
    struct vm_entry *vme = find_vme( page );
    struct frame *kva = vm_get_frame();

    /* Set links */
    kva->page = page;
    page->frame = kva;

    /* Insert page table entry to map page's VA to frame's PA. */
    load_file( kva, vme );
    install_page( page, kva, vme->writable );
    return swap_in( page, kva->kva );
}

/* Initialize new supplemental page table */

void supplemental_page_table_init( struct supplemental_page_table *spt UNUSED ) {
    bool success = hash_init( &spt->vm, vm_hash_func, vm_less_func, NULL );
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy( struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED ) {}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill( struct supplemental_page_table *spt UNUSED ) {
    /* TODO: Destroy all the supplemental_page_table hold by thread and
     * TODO: writeback all the modified contents to the storage. */
}
