#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct file* process_get_file(int fd);
struct thread * get_child_process(int pid);
void remove_child_process(struct thread *t);
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool expand_stack(void *addr);
bool verify_stack(void *fault_addr, void *esp);
bool install_page (void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */
