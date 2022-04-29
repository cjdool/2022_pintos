#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/swap.h"
#include "filesys/file.h"

void lru_list_init(void){
    list_init(&lru_list);
    lock_init(&lru_list_lock);
    lru_clock = NULL;
}

void add_page_to_lru_list(struct page* page){
    lock_acquire(&lru_list_lock);
    list_push_back(&lru_list, &page->lru);
    lock_release(&lru_list_lock);
}

void del_page_from_lru_list(struct page* page){
    if (lru_list == &page->lru){
        lru_clock = list_next(lru_clock);
    }
    list_remove (&page->lru);
}

struct page* alloc_page(enum palloc_flags flags){
    struct page *page;
    lock_acquire(&lru_list_lock);
    void *kaddr = palloc_get_page(flags);
    while (kaddr == NULL)
    {
        try_to_free_pages(flags);
        kaddr = palloc_get_page(flags);
    }
    page = (struct page *)malloc(sizeof(struct page));
    page->kaddr = kaddr;
    page->thread = thread_current();
    add_page_to_lru_list(page);
    lock_release(&lru_list_lock);
    return page;
}

void try_to_free_pages(enum palloc_flags flags){
    struct page *page;
    struct page *victim;

    /*select victim page*/
    lru_clock = get_next_lru_clock();
    page = list_entry(lru_clock, struct page, lru);
    while(page->vme->pinned || pagedir_is_accessed(page->thread->pagedir, page->vme->vaddr)){
        pagedir_set_accessed(page->thread->pagedir, page->vme->vaddr, false);
        lru_clock = get_next_lru_clock();
        page = list_entry(lru_clock, struct page, lru);
    }
    victim = page;
    
    /*handle for dirty case*/
    bool dirty_flag = pagedir_is_dirty(victim->thread->pagedir, victim->vme->vaddr);     
    switch(victim->vme->type){
        case VM_BIN:
            if (dirty_flag) // swap out
            {
                victim->vme->swap_slot = swap_out(victim->kaddr);
                victim->vme->type=VM_ANON;
            }
            break;
        case VM_FILE:
            if(dirty_flag) // write to file
            {
                file_write_at(victim->vme->file, victim->vme->vaddr, victim->vme->read_bytes, victim->vme->offset);
            }
            break;
        case VM_ANON: // swap out
            victim->vme->swap_slot = swap_out(victim->kaddr);
            break;
    }
    victim->vme->is_loaded = false;

    //free page
    _free_page(victim);
}

void __free_page(struct page* page){
    del_page_from_lru_list(page);
    pagedir_clear_page(page->thread->pagedir, pg_round_down(page->vme->vaddr));
    palloc_free_page(page->kaddr);
    free(page);
}

void free_page(void *kaddr){
    struct list_elem *e;
    struct page *page = NULL;
    struct page *candi_page;
    lock_acquire(&lru_list_lock);
    for (e=list_begin(&lru_list); e!=list_end(&lru_list); e=list_next(e))
    {
        candi_page = list_entry(e, struct page, lru);
        if (candi_page->kaddr == kaddr)
        {
            page = candi_page;
            break;
        }
    }
    if (page != NULL)
        _free_page(page);

    lock_release(&lru_list_lock);
}

static struct list_elem* get_next_lru_clock(){
    struct list_elem *retval;

    if (list_empty(&lru_list)){
        retval = NULL;
    }
    else if (lru_clock == NULL){
        retval = list_begin(&lru_list);
    }
    else if (lru_clock == list_end(&lru_list)){
        retval = NULL;
    }
    else{
        retval = list_next(lru_clock);
    }
    
    return retval;
}

