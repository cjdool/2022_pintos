#include "filesys/cache.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"
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
    bool lock = false;

    if(!lock_held_by_current_thread(&bc_lock)){
        lock_acquire(&bc_lock);
        lock = true;
    }

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
        bc->pinned =true;

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

    bc->pinned = false;
    if(lock){
        lock_release(&bc_lock);
    }
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
        bc->pinned = true;

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
    bc->pinned = false;
    
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

    while(bc->pinned || bc_is_accessed(bc)){
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
    bc_cnt = 0;
    bc_clock = NULL;

    lock_release(&bc_lock);

}

static bool dc_less_func(const struct hash_elem *a, const struct hash_elem * b, void* aux UNUSED){

    struct dentry_cache * dc_a = hash_entry(a, struct dentry_cache, elem);
    struct dentry_cache * dc_b = hash_entry(b, struct dentry_cache, elem);
    
    return strcmp(dc_a->path, dc_b->path) < 0 ? true : false;
}

static unsigned dc_hash_func(const struct hash_elem *e, void* aux UNUSED){

    struct dentry_cache * dc = hash_entry(e, struct dentry_cache, elem);
    return hash_string((const char*)dc->path);

}

void dc_action_func(struct hash_elem * e, void * aux UNUSED){
    
    struct dentry_cache * dc = hash_entry(e, struct dentry_cache, elem);
    
    free(dc->path);
    free(dc);
}

void dc_init(void){
    lock_init(&dc_lock); 
    hash_init(&dentry_cache_hash, dc_hash_func, dc_less_func, NULL);



}
bool dc_insert(const char *name, block_sector_t inumber){
    bool success = false;

    struct dentry_cache * dc = malloc(sizeof(struct dentry_cache));
    dc->path =(char*) malloc(strlen(name)+1);
    strlcpy(dc->path, name, strlen(name)+1);
    dc->inumber = inumber;

    //printf("dc->path : %s\n",dc->path);
    lock_acquire(&dc_lock); 
    if(hash_insert(&dentry_cache_hash, &dc->elem)==NULL){
        success = true;
    }
    lock_release(&dc_lock);
    return success;
}

/*bool dc_insert(struct hash *dc_hash, struct dentry_cache * dc){
    bool success = false;
    printf("dc->path : %s\n",dc->path);
    lock_acquire(&dc_lock); 
    if(hash_insert(dc_hash, &dc->elem)==NULL){
        success = true;
    }
    lock_release(&dc_lock);
    return success;
}*/

bool dc_delete(struct hash* dc_hash, struct dentry_cache *dc){
    bool success = false;
    lock_acquire(&dc_lock); 
    
    if( hash_delete(dc_hash, &dc->elem) !=NULL){
        success= true;
        free(dc->path);
        free(dc);
    
    }
    lock_release(&dc_lock);
    return success;
}


struct dentry_cache * dc_find(const char* path){
    
    struct hash_elem *e;
    struct dentry_cache  dc;
    struct dentry_cache  *ret_dc;

    dc.path = path;
    lock_acquire(&dc_lock); 
    e = hash_find(&dentry_cache_hash,&dc.elem);
    if( e == NULL){
        ret_dc = NULL;
    }else{
        ret_dc = hash_entry(e, struct dentry_cache, elem);
    }
    lock_release(&dc_lock); 

    return ret_dc;
}

struct dentry_cache * dc_find2(const char* path, char* file_name){
    
    struct hash_elem *e;
    char * token;
    struct dentry_cache dc;

    char * tmp_name = malloc(strlen(path)+1);
    //char * free_name = tmp_name;

    strlcpy(tmp_name, path, strlen(path)+1);

    token = strrchr(tmp_name, '/');
    
    *token = '\0';
    dc.path = tmp_name;

    struct dentry_cache * ret_dc = dc_find(dc.path);
    *token = '/';
    if(ret_dc != NULL){
        strlcpy(file_name, token+1, strlen(token));
    }
    
    free(tmp_name);
    return ret_dc;
}

void dc_destroy(struct hash* dc_hash){

    hash_destroy(dc_hash, dc_action_func);

}

