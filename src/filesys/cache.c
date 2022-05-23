#include "filesys/cache.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "lib/kernel/list.h"
#include "lib/string.h"

#define BUFFER_CACHE_MAX 64

//struct buffer_cache bc_array[BUFFER_CACHE_MAX];
struct list bc_list;
int bc_cnt;
struct list_elem *bc_clock;
static struct list_elem* get_next_bc_clock(void);

bool bc_isfull(void){
    return bc_cnt >=  BUFFER_CACHE_MAX;
}

void bc_init(void){
    list_init(&bc_list);
    lock_init(&bc_lock);
    bc_cnt = 0;
    bc_clock = NULL;
}
void add_bc_list(struct buffer_cache* bc){
    list_push_back(&bc_list, &bc->elem);
    bc_cnt++;
}

void del_bc_list(struct buffer_cache* bc){
    if (bc_clock == &bc->elem){
        bc_clock = get_next_bc_clock();
    }
    list_remove (&bc->elem);
    bc_cnt--;
}

bool bc_read(block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs){
    
    struct buffer_cache * bc;

    lock_acquire(&bc_lock);

    if( (bc = bc_lookup(sector_idx)) == NULL){ //no sector in buffer cache
        if(bc_isfull()){
            bc_evict();
        }
        bc = (struct buffer_cache *)malloc(sizeof(struct buffer_cache));
        if( bc == NULL){
            lock_release(&bc_lock);
            return false;
        }
        add_bc_list(bc);
        bc->sector = sector_idx;
        bc->dirty = false;
        bc->accessed = false;

        bc->cache = malloc(BLOCK_SECTOR_SIZE);
        if( bc->cache == NULL){
            lock_release(&bc_lock);
            return false;
        }

        block_read(fs_device, sector_idx, bc->cache);
        memcpy(buffer+bytes_read, bc->cache+sector_ofs, chunk_size);

    }else{ // found in buffer cache
        bc->accessed = true;
        memcpy(buffer+bytes_read, bc->cache+sector_ofs, chunk_size);
    }

    lock_release(&bc_lock);
    return true;
}


bool bc_write(block_sector_t sector_idx, const void *buffer, off_t bytes_written, int chunk_size, int sector_ofs){
    
    struct buffer_cache * bc;

    lock_acquire(&bc_lock);

    if( (bc = bc_lookup(sector_idx)) == NULL){ //no sector in buffer cache
        if(bc_isfull()){
            bc_evict();
        }
        bc = (struct buffer_cache *)malloc(sizeof(struct buffer_cache));
        if( bc == NULL){
            lock_release(&bc_lock);
            return false;
        }
        add_bc_list(bc);
        bc->sector = sector_idx;
        bc->dirty = true;
        bc->accessed = false;

        bc->cache = malloc(BLOCK_SECTOR_SIZE);
        if( bc->cache == NULL){
            lock_release(&bc_lock);
            return false;
        }

        if (sector_ofs > 0 || chunk_size < BLOCK_SECTOR_SIZE-sector_ofs){
            block_read (fs_device, sector_idx, bc->cache);  
        }else{
            memset (bc->cache, 0, BLOCK_SECTOR_SIZE);
        }
        
        memcpy(bc->cache+sector_ofs, buffer+bytes_written, chunk_size);

    }else{ // found in buffer cache
        bc->dirty = true;
        bc->accessed = true;
        memcpy(bc->cache+sector_ofs, buffer+bytes_written, chunk_size);
    }

    lock_release(&bc_lock);
    return true;

}

void bc_term(void){
    bc_flush_all();
    bc_free_all();
}

struct buffer_cache * bc_lookup(block_sector_t sector){

    struct list_elem *e ;
    struct buffer_cache *bc;

    for( e = list_begin(&bc_list); e != list_end(&bc_list); e = list_next(e)){
        bc= list_entry(e, struct buffer_cache, elem);
        if( bc->sector == sector){
            return bc;
        }
    }

    return NULL;
}

static struct list_elem* get_next_bc_clock(void){
    struct list_elem *retval;

    if (list_empty(&bc_list)){
        retval = NULL;
    }
    else if (bc_clock == NULL){
        retval = list_begin(&bc_list);
    }
    else if (bc_clock == list_end(&bc_list)){
        retval = list_begin(&bc_list);
    }else if ( list_next(bc_clock) == list_end(&bc_list)){
        retval = list_begin(&bc_list); 
    }
    else{
        retval = list_next(bc_clock);
    }
    
    return retval;
}

bool bc_is_accessed(struct buffer_cache *bc){
    return bc->accessed;
}

void bc_set_accessed(struct buffer_cache *bc, bool accessed){
    bc->accessed = accessed;
}

bool bc_is_dirty(struct buffer_cache *bc){
    return bc->dirty;
}

void  bc_evict(void){
    struct buffer_cache * bc;


    bc_clock = get_next_bc_clock();
    bc = list_entry(bc_clock, struct buffer_cache, elem); 

    while(bc_is_accessed(bc)){
        bc_set_accessed(bc, false);
        bc_clock = get_next_bc_clock();
        bc = list_entry(bc_clock, struct buffer_cache, elem); 
    }

    if(bc_is_dirty(bc)){
        bc_flush(bc);
    }
    
    del_bc_list(bc);

    free(bc->cache);
    free(bc);

}

void bc_flush(struct buffer_cache *flush){
    block_write(fs_device, flush->sector, flush->cache);
    flush->dirty = false;
}

void bc_flush_all(void){

    struct list_elem *e ;
    struct buffer_cache *bc;

    lock_acquire(&bc_lock);

    for( e = list_begin(&bc_list); e != list_end(&bc_list); e = list_next(e)){
        bc= list_entry(e, struct buffer_cache, elem);
        if(bc_is_dirty(bc)){
            bc_flush(bc);
        }
    }

    lock_release(&bc_lock);

}

void bc_free_all(void){

    struct list_elem *e ;
    struct list_elem *next_e;
    struct buffer_cache *bc;

    lock_acquire(&bc_lock);

    for( e = list_begin(&bc_list); e != list_end(&bc_list); ){
        next_e = list_next(e);
        bc= list_entry(e, struct buffer_cache, elem);
        del_bc_list(bc);
        free(bc->cache);
        free(bc);
        e = next_e;
    }

    lock_release(&bc_lock);

}
