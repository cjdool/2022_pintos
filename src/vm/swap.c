#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"

#define BLOCK_PER_PAGE  PGSIZE/BLOCK_SECTOR_SIZE

void swap_init(void){
    struct block *swap_slot = block_get_role(BLOCK_SWAP);
    //swap_bitmap = bitmap_create(block_size(swap_slot));
    swap_bitmap = bitmap_create(1024);
    lock_init(&swap_lock);
}

void swap_in(size_t used_index, void* kaddr){
    int i;
    struct block *swap_slot = block_get_role(BLOCK_SWAP);
    lock_acquire(&swap_lock);
    if(bitmap_test(swap_bitmap, used_index)){
        for (i = 0; i < BLOCK_PER_PAGE; i++){
            block_read(swap_slot, BLOCK_PER_PAGE*used_index+i, BLOCK_SECTOR_SIZE*i+kaddr);
        }
        bitmap_reset(swap_bitmap, used_index);
        lock_release(&swap_lock);
    }else{
        lock_release(&swap_lock);
        exit(-1);
    }
}

size_t swap_out(void* kaddr){
    int i;
    struct block *swap_slot = block_get_role(BLOCK_SWAP);
    size_t swap_index = bitmap_scan(swap_bitmap, 0, 1, false); // start:0 , count: 1, find_value = false;
    lock_acquire(&swap_lock);
    if (swap_index != BITMAP_ERROR){
        for(i = 0; i < BLOCK_PER_PAGE; i++){
            block_write(swap_slot, BLOCK_PER_PAGE*swap_index+i, BLOCK_SECTOR_SIZE*i+kaddr);
        }
        bitmap_set(swap_bitmap, swap_index, true);
        lock_release(&swap_lock);
    }else{
        lock_release(&swap_lock);
        exit(-1);
    }
    return swap_index;
}
