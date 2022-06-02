#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "filesys/inode.h"
#include "threads/synch.h"
#include "lib/kernel/hash.h"

struct lock bc_lock;
struct hash dentry_cache_hash;
struct lock dc_lock;

struct buffer_cache{
    block_sector_t sector;
    bool dirty;
    bool accessed;
    bool pinned;
    void* cache;
    struct list_elem elem;
};

struct dentry_cache{
    
    char* path;
    block_sector_t inumber;

    struct hash_elem elem;
};

void bc_init(void);
void bc_term(void);
bool bc_read(block_sector_t, void *, off_t, int, int);
bool bc_write(block_sector_t, const void *, off_t , int, int);
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

void dc_action_func(struct hash_elem * e, void * aux );
void dc_init(void);
bool dc_insert(const char*, block_sector_t);
//bool dc_insert(struct hash *dc_hash, struct dentry_cache * dc);
bool dc_delete(struct hash* dc_hash, struct dentry_cache *dc);
struct dentry_cache * dc_find(const char* path);
struct dentry_cache * dc_find2(const char* path, char*);
void dc_destroy(struct hash* dc_hash);
#endif /* filesys/cache.h */
