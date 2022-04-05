#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

typedef int pid_t;

void check_address(void *);
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
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void get_argument(void *esp, int *arg, int count){
    int i;

    check_address((void *)esp);
    void * sp = esp;
    for(i = 0 ; i<count; i++){
        sp += 4;
        arg[i] = *(int *)sp;

    }

}

void check_address(void *addr){
    struct thread * t = thread_current();
    uint32_t * pd = t->pagedir;
    if(addr == NULL || !is_user_vaddr(addr) || pagedir_get_page(pd,addr) == NULL ){
       exit(-1);
    }
}

void halt(void){
    shutdown_power_off();
}

void exit(int status){
    struct thread * t = thread_current();
    printf("%s: exit(%d)\n",t->name,status);

    t->by_exit = 1;
    t->exit_status = status;
    thread_exit();
}

pid_t exec(const char *cmd_line){
    pid_t pid;

    pid = process_execute(cmd_line);
    
    return pid;
}

int wait(pid_t pid){

    int status ;
    struct thread * child = get_child_process((int)pid);

    status = process_wait((tid_t) pid); 

    return status;

}

/*bool create(const char *file, unsigned initial_size)
{
    return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
    return filesys_remove(file);
}

int open(const char *file)
{
    int fd;
    struct thread *cur = thread_current();
    struct file *openfile = filesys_open(file);

    fd = cur->next_fd;
    cur->fdt[fd] = openfile;
    cur->next_fd++;

    return fd;
}

int filesize(int fd)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];

    return (int)file_length(curfile);
}

int read(int fd, void *buffer, unsigned size)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];
    int retvali = -1;

    if (fd == 0)
    {
        retval = (int)input_getc();
    }
    else
    {
        retval = (int)file_read(curfile, buffer, size);
    }

    return retval;
}

int write(int fd, const void *buffer, unsigned size)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];
    int retval = -1;

    if (fd == 1)
    {
        putbuf(buffer, size);
        retval = size;
    }
    else
    {
        retval = (int)file_write(curfile, buffer, size);
    }

    return retval;
}

void seek(int fd, unsigned position)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];
    
    file_seek(curfile, position);
}

unsigned tell(int fd)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];

    return (unsigned)file_tell(curfile);
}

void close(int fd)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];

    file_close(curfile);
    cur->fdt[fd] = NULL;
}
*/
static void
syscall_handler (struct intr_frame *f ) 
{
    int *arg[3];
    uint32_t *sp =f->esp;
    check_address((void*)sp);
    uint32_t number = *sp;
  // printf("syscall number : %d\n",number);
  
  switch(number) {
    case SYS_HALT :
        halt();
        break;
    case SYS_EXIT :
        get_argument(sp, arg, 1);
        exit(arg[0]);
        break;
    case SYS_EXEC :
        get_argument(sp, arg, 1);
        f->eax = exec((const char *)arg[0]);
        break;
    case SYS_WAIT :
        get_argument(sp, arg, 1);
        f->eax = wait(arg[0]);
        break;
    case SYS_CREATE :
        break;
    case SYS_REMOVE :
        break;
    case SYS_OPEN :
        break;
    case SYS_FILESIZE :
        break;
    case SYS_READ :
        break;
    case SYS_WRITE :
        break;
    case SYS_SEEK :
        break;
    case SYS_TELL :
        break;
    case SYS_CLOSE :
        break;
    case SYS_SIGACTION :
        break;
    case SYS_SENDSIG :
        break;
    case SYS_YIELD :
        break;

    /* Project 3 */
    case SYS_MMAP :
        break;
    case SYS_MUNMAP :
        break;
    /* Project 4 */
    case SYS_CHDIR :
        break;
    case SYS_MKDIR :
        break;
    case SYS_READDIR :
        break;
    case SYS_ISDIR :
        break;
    case SYS_INUMBER :
        break;
    default :
        exit(-1);
  }
  
}
