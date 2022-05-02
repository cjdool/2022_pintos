#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm/page.h"
#include <threads/thread.h>
#include <list.h>
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "vm/frame.h"


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
    
   // palloc_free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
   // pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
    free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
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

        free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
       // palloc_free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
       // pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
        free(vme);

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
    file_close(thread_current()->binary_file);
}


void do_munmap(struct mmap_file * mmfile, struct list_elem * e){
    
   struct vm_entry *vme;

   while( !list_empty(&mmfile->vme_list)){
        
        vme = list_entry(list_pop_front(&mmfile->vme_list), struct vm_entry, mmap_elem);

        if(vme->is_loaded && pagedir_is_dirty(thread_current()->pagedir, vme->vaddr)){
           if(vme->read_bytes != (size_t)file_write_at(vme->file,vme->vaddr, vme->read_bytes,vme->offset)){
                exit(-1);
           }
        }
        vme->is_loaded = false;
        delete_vme(&thread_current()->vm, vme);
   }

    list_remove(e);
    file_close(mmfile->file);
    free(mmfile);


}

void all_munmap(void){
    
   struct list_elem *e;
   struct list_elem *next_e;
   struct mmap_file *mmfile;

   if(!list_empty(&thread_current()->mmap_list)){
       for( e = list_begin(&thread_current()->mmap_list); e != list_end(&thread_current()->mmap_list); ){
            
            next_e = list_next(e);
            mmfile= list_entry(e, struct mmap_file, elem);
            do_munmap(mmfile,e);
            e = next_e;  
        }
   }
}
