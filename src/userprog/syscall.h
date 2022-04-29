#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdio.h>

struct lock filesys_lock;

typedef int pid_t;
//typedef int mapid_t;

struct lock filesys_lock;

void get_argument(void *esp, int *arg, int count);
struct vm_entry * check_address(void *);
void check_valid_buffer(void*, unsigned, bool);
void check_valid_string(const void *);

void syscall_init (void);
void halt(void);
void exit(int status);
pid_t exec(const char *cmd_line);
int wait(pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
void sigaction(int signum, void(*handler)(void));
void sendsig(pid_t pid, int signum);
void sched_yield(void);
int mmap(int, void*);
void munmap(int);

#endif /* userprog/syscall.h */
