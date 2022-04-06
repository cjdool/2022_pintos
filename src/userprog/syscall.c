#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "devices/input.h"

void get_argument(void *esp, int *arg, int count);
void check_address(void *);
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void get_argument(void *esp, int *arg, int count){
    int i;
    check_address(esp);

    if( count < 4){
        void * sp = esp;
        for(i = 0 ; i<count; i++){
            sp += 4;
            arg[i] = *(int *)sp;
        }
    }else {
        exit(-1);
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
    struct thread *child = get_child_process((int)pid);

    status = process_wait((tid_t) pid); 

    return status;

}

bool create(const char *file, unsigned initial_size)
{
    bool retval;
    lock_acquire(&filesys_lock);
    retval = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    return retval;
}

bool remove(const char *file)
{
    bool retval;
    lock_acquire(&filesys_lock);
    retval = filesys_remove(file);
    lock_release(&filesys_lock);
    return retval;
}

int open(const char *file)
{
    int fd;
    struct thread *cur = thread_current();

    fd = cur->next_fd;
    lock_acquire(&filesys_lock);
    cur->fdt[fd] = filesys_open(file);
    lock_release(&filesys_lock);
    cur->next_fd++;

    return fd;
}

int filesize(int fd)
{
    int retval;
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];

    lock_acquire(&filesys_lock);
    retval = (int)file_length(curfile);
    lock_release(&filesys_lock);

    return retval;
}

int read(int fd, void *buffer, unsigned size)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];
    int retval = -1;

    lock_acquire(&filesys_lock);
    if (fd == 0)
    {
        retval = (int)input_getc();
    }
    else
    {
        retval = (int)file_read(curfile, buffer, size);
    }
    lock_release(&filesys_lock);

    return retval;
}

int write(int fd, const void *buffer, unsigned size)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];
    int retval = -1;

    lock_acquire(&filesys_lock);
    if (fd == 1)
    {
        putbuf(buffer, size);
        retval = size;
    }
    else
    {
        retval = (int)file_write(curfile, buffer, size);
    }
    lock_release(&filesys_lock);

    return retval;
}

void seek(int fd, unsigned position)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];
   
    lock_acquire(&filesys_lock);
    file_seek(curfile, position);
    lock_release(&filesys_lock);
}

unsigned tell(int fd)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];
    unsigned retval;

    lock_acquire(&filesys_lock);
    retval = (unsigned)file_tell(curfile);
    lock_release(&filesys_lock);

    return retval;
}

void close(int fd)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];

    lock_acquire(&filesys_lock);
    file_close(curfile);
    lock_release(&filesys_lock);
}

/*void sigaction(int signum, void(*handler)(void))
{

}
void sendsig(pid_t pid, int signum)
{

}
void sched_yield(void)
{

}*/

static void
syscall_handler (struct intr_frame *f ) 
{
    int arg[3];
    uint32_t *sp =f->esp;
    check_address((void*)sp);
    uint32_t number = *sp;
  
  switch(number) {
    case SYS_HALT :
        halt();
        break;
    case SYS_EXIT :
        get_argument(sp, arg, 1);
        exit((int)arg[0]);
        break;
    case SYS_EXEC :
        get_argument(sp, arg, 1);
        f->eax = exec((const char *)arg[0]);
        break;
    case SYS_WAIT :
        get_argument(sp, arg, 1);
        f->eax = wait((pid_t)arg[0]);
        break;
    case SYS_CREATE :
        get_argument(sp, arg, 2);
        f->eax = create((const char *)arg[0], (unsigned)arg[1]);
        break;
    case SYS_REMOVE :
        get_argument(sp, arg, 1);
        f->eax = remove((const char *)arg[0]);
        break;
    case SYS_OPEN :
        get_argument(sp, arg, 1);
        f->eax = open((const char *)arg[0]);
        break;
    case SYS_FILESIZE :
        get_argument(sp, arg, 1);
        f->eax = filesize((int)arg[0]);
        break;
    case SYS_READ :
        get_argument(sp, arg, 3);
        f->eax = read((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
        break;
    case SYS_WRITE :
        get_argument(sp, arg, 3);
        f->eax = write((int)arg[0], (const void *)arg[1], (unsigned)arg[2]);
        break;
    case SYS_SEEK :
        get_argument(sp, arg, 2);
        seek((int)arg[0], (unsigned)arg[1]);
        break;
    case SYS_TELL :
        get_argument(sp, arg, 1);
        f->eax = tell((int)arg[0]);
        break;
    case SYS_CLOSE :
        get_argument(sp, arg, 1);
        close((int)arg[0]);
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
