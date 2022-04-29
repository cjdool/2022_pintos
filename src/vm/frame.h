#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "vm/page.h"
#include "threads/palloc.h"

struct list lru_list;
struct lock lru_list_lock;
struct list_elem *lru_clock;

void lru_list_init(void);
void add_page_to_lru_list(struct page* page);
void del_page_from_lru_list(struct page* page);
struct page* alloc_page(enum palloc_flags flags);
void free_page(void *kaddr);
static struct list_elem* get_next_lru_clock();

#endif 
