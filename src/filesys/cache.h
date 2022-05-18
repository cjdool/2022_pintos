#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "filesys/inode.h"
#include "threads/synch.h"

struct lock bc_lock;

struct buffer_cache{
    struct inode * inode;
    block_sector_t sector;
    bool dirty;
    bool accessed;
    void* cache;
    struct list_elem elem;
};


void bc_init(void);
void bc_term(void);
bool bc_read(struct inode *, block_sector_t, void *, off_t, int, int);
bool bc_write( struct inode *,block_sector_t, const void *, off_t , int, int);
struct buffer_cache * bc_lookup(block_sector_t sector);
void bc_evict(void);
void bc_flush(struct buffer_cache *flush);
void bc_flush_all(void);
void bc_free_all(void);
bool bc_isfull(void);
void add_bc_list(struct buffer_cache *);
void del_bc_list(struct buffer_cache *);
bool bc_is_accessed(struct buffer_cache *);
void bc_set_accessed(struct buffer_cache *,bool);
bool bc_is_dirty(struct buffer_cache *);

#endif /* filesys/cache.h */
