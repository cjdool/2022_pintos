#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm/page.h"
#include <threads/thread.h>
#include <list.h>
#include "threads/vaddr.h"
#include "threads/malloc.h"


static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void* aux UNUSED){

    struct vm_entry* vme_a = hash_entry(a, struct vm_entry, elem);
    struct vm_entry* vme_b = hash_entry(b, struct vm_entry, elem);

    return vme_a->vaddr < vme_b->vaddr;
}

static unsigned vm_hash_func(const struct hash_elem *e, void* aux UNUSED){

    struct vm_entry* vme = hash_entry(e, struct vm_entry, elem);
    
    return hash_int((int)vme->vaddr);
}

void vm_action_func(struct hash_elem *e, void *aux UNUSED){
    
    struct vm_entry* vme = hash_entry(e, struct vm_entry, elem);

    free(vme);

}

void vm_init(struct hash *vm){
    
    hash_init(vm, vm_hash_func, vm_less_func, NULL);  

}

bool insert_vme(struct hash *vm, struct vm_entry *vme){
    
    if( hash_insert(vm, &vme->elem) == NULL){
        return true;
    }

    return false;
}

bool delete_vme(struct hash *vm, struct vm_entry *vme){
  
    if( hash_delete(vm, &vme->elem) == NULL ){
        return true;
    }

    return false;
}

struct vm_entry *find_vme(void *vaddr){
    
    struct thread *t = thread_current();
    struct vm_entry vme;
    struct hash_elem *e;

    vme.vaddr = pg_round_down(vaddr);
    e = hash_find(&t->vm, &vme.elem);
    if(e == NULL){
        return NULL;
    }

    return  hash_entry(e, struct vm_entry, elem);     
}

void vm_destroy(struct hash *vm){
    
    hash_destroy(vm, vm_action_func);
}





















