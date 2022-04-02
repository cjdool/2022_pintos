#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"


static void syscall_handler (struct intr_frame *);
void halt(void);
void exit(int);
void check_address(void *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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

    t->exit_status = status;
    thread_exit();

}
static void
syscall_handler (struct intr_frame *f ) 
{
  uint32_t number = f->vec_no;
  
  switch(number) {
    case SYS_HALT :
        halt();
        break;
    case SYS_EXIT :
       // exit();
        break;
    case SYS_EXEC :
        break;
    case SYS_WAIT :
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
