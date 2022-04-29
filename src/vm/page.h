#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <lib/kernel/hash.h>
#include <filesys/file.h>
#include <list.h>
#include <debug.h>
#include <stdint.h>



#define VM_BIN  0
#define VM_FILE 1
#define VM_ANON 2


struct vm_entry{
    uint8_t type;
    void *vaddr;
    bool writable;

    bool is_loaded;
    struct file* file;

    struct list_elem mmap_elem;

    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    size_t swap_slot;

    struct hash_elem elem;
};

struct mmap_file{
    int mapid;
    struct file* file;
    struct list_elem elem;
    struct list vme_list;

};

void vm_init(struct hash *);
void vm_action_func(struct hash_elem *, void*);
bool insert_vme(struct hash *, struct vm_entry *);
bool delete_vme(struct hash *, struct vm_entry *);
struct vm_entry *find_vme(void *);
void vm_destroy(struct hash *);
void do_munmap(struct mmap_file *,struct list_elem *);
void all_munmap(void);


#endif
