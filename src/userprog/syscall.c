#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "vm/page.h"
#include "threads/malloc.h"

void get_argument(void *esp, int *arg, int count);
struct vm_entry * check_address(void *);
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void get_argument(void *esp, int *arg, int count){
    int i;

    if( count < 4){
        void * sp = esp;
        for(i = 0 ; i<count; i++){
            sp += 4;
            check_address(sp);
            arg[i] = *(int *)sp;
        }
    }else {
        exit(-1);
    }
}

struct vm_entry * check_address(void *addr){
    //struct thread * t = thread_current();
    //uint32_t * pd = t->pagedir;
    void* temp_addr = addr;
    for( int i=0; i<4;i++){
        temp_addr = temp_addr + i;
        bool is_kernel = (int)is_kernel_vaddr(temp_addr);
        if(temp_addr == NULL || is_kernel || temp_addr < (void*)8048000){
           exit(-1);
        } 
       /* if( !is_kernel){
            if(pagedir_get_page(pd,temp_addr) == NULL){
                exit(-1);
            }
        
        }
        */
    }
    
   struct vm_entry * vme = find_vme(addr); 
   return vme;
}

void check_valid_buffer(void * buffer, unsigned size, bool to_write){
    void* addr = pg_round_down(buffer);
    struct vm_entry * vme;
    int tempsize = (int)size;    
    while( tempsize > 0){
            
        vme = check_address(addr);
        
        if(vme == NULL){
            exit(-1);
        }
        if(to_write == 1 && vme->writable==0){
            exit(-1);
        }

        addr += PGSIZE;
        tempsize -= PGSIZE;
    }
}

void check_valid_string(const void *str){

    struct vm_entry *vme = check_address((void*)str);
    if(vme ==NULL){
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
    //check_address((void*)cmd_line);
    pid = process_execute(cmd_line);
    return pid;
}

int wait(pid_t pid){

    int status ;

    status = process_wait((tid_t) pid); 

    return status;

}

bool create(const char *file, unsigned initial_size)
{
    bool retval;
    /*if (file == NULL)
    {
        exit(-1);
    }
    check_address((void *)file);*/
    lock_acquire(&filesys_lock);
    retval = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    return retval;
}

bool remove(const char *file)
{
    bool retval;
    /*if (file == NULL)
    {
        exit(-1);
    }
    check_address((void *)file);*/
    lock_acquire(&filesys_lock);
    retval = filesys_remove(file);
    lock_release(&filesys_lock);
    return retval;
}

int open(const char *file)
{
    int fd;
    struct file *retval;
    struct thread *cur = thread_current();

    /*if (file == NULL)
    {
        exit(-1);
    }
    check_address((void *)file);*/
    lock_acquire(&filesys_lock);
    retval = filesys_open(file);
    if (retval != NULL)
    {
        if (strcmp(cur->name, file) == 0)
        {
            file_deny_write(retval);
        }
        fd = cur->next_fd;
        if(fd>FDT_SIZE){
            return -1;
        }
        cur->fdt[fd] = retval;
    }
    else
    {
        fd = -1;
    }
    cur->next_fd++;
    lock_release(&filesys_lock);

    return fd;
}

int filesize(int fd)
{
    int retval;
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];

    if (fd < 2 || fd > FDT_SIZE || curfile == NULL)
    {
        exit(-1);
    }
    lock_acquire(&filesys_lock);
    retval = (int)file_length(curfile);
    lock_release(&filesys_lock);

    return retval;
}

int read(int fd, void *buffer, unsigned size)
{
    struct thread *cur = thread_current();
    struct file *curfile;
    int retval = -1;

    if (fd < 0 || fd > FDT_SIZE)
    {
        exit(-1);
    }
    //check_address((void *)buffer);
    lock_acquire(&filesys_lock);
    if (fd == 0)
    {
        for (retval = 0; (unsigned int)retval < size; retval++)
        {
            if(input_getc() == '\0')
                break;
        }
    }
    else if (fd > 2)
    {
        curfile = cur->fdt[fd];
        if (curfile == NULL)
        {
            lock_release(&filesys_lock);
            exit(-1);
        }
        retval = (int)file_read(curfile, buffer, size);
    }
    lock_release(&filesys_lock);

    return retval;
}

int write(int fd, const void *buffer, unsigned size)
{
    struct thread *cur = thread_current();
    struct file *curfile;
    int retval = -1;

    if (fd < 0 || fd > FDT_SIZE)
    {
        exit(-1);
    }
    //check_address((void *)buffer);
    lock_acquire(&filesys_lock);
    if (fd == 1)
    {
        putbuf(buffer, size);
        retval = size;
    }
    else if (fd > 2)
    {
        curfile = cur->fdt[fd];
        if (curfile == NULL)
        {
            lock_release(&filesys_lock);
            exit(-1);
        }
        if (curfile->deny_write)
        {
            file_deny_write(curfile);
        }
        retval = (int)file_write(curfile, buffer, size);
    }
    lock_release(&filesys_lock);

    return retval;
}

void seek(int fd, unsigned position)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];
  
    if (fd < 3 || fd > FDT_SIZE || curfile == NULL)
    {
        exit(-1);
    }
    lock_acquire(&filesys_lock);
    file_seek(curfile, position);
    lock_release(&filesys_lock);
}

unsigned tell(int fd)
{
    struct thread *cur = thread_current();
    struct file *curfile = cur->fdt[fd];
    unsigned retval;

    if (fd < 3 || fd > FDT_SIZE || curfile == NULL)
    {
        exit(-1);
    }
    lock_acquire(&filesys_lock);
    retval = (unsigned)file_tell(curfile);
    lock_release(&filesys_lock);

    return retval;
}

void close(int fd)
{
    struct thread *cur = thread_current();

    if (fd < 3 || fd > FDT_SIZE || cur->fdt[fd] == NULL)
    {
        exit(-1);
    }
    lock_acquire(&filesys_lock);
    file_close(cur->fdt[fd]);
    cur->fdt[fd] = NULL;
    lock_release(&filesys_lock);
}

void sigaction(int signum, void(*handler)(void))
{
    struct thread *t = thread_current();

    t->sig[signum].num = signum;
    t->sig[signum].sighandler = handler ;
}
void sendsig(pid_t pid, int signum)
{
   struct thread *t= find_thread((tid_t)pid);
   if( t->sig[signum].num == signum){
        printf("Signum: %d, Action: 0x%x\n",signum,t->sig[signum].sighandler);
   }

}
void sched_yield(void)
{
    thread_yield();
}

int mmap(int fd, void *addr){

    int mapid ;
    struct vm_entry * found_vme;
    struct file* file;
    size_t offset = 0;

    if (fd < 2 || fd > FDT_SIZE)
    {
        return -1;
    }
    if( (int)addr % PGSIZE !=0 || addr == 0){
        return -1;
    }
    if((found_vme = check_address(addr))!=NULL){
        return -1;
    }

    if((file = process_get_file(fd))==NULL){
        return -1;
    }

    file = file_reopen(file);
    int read_bytes = (int)file_length(file);
    if(read_bytes == 0){
        return -1;
    }
    struct mmap_file * mmfile = (struct mmap_file *)malloc(sizeof(struct mmap_file));
    if( mmfile == NULL){
        return -1;
    }
    mmfile->mapid = thread_current()->mapid++;
    mmfile->file = file;
    list_init(&mmfile->vme_list);
    list_push_back(&thread_current()->mmap_list, &mmfile->elem);
    
    while(read_bytes >0){
            
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        
        struct vm_entry * vme = (struct vm_entry*)malloc(sizeof(struct vm_entry));
        vme->type = VM_FILE;
        vme->vaddr = addr;
        vme->writable = true;
        vme->is_loaded= false;
        vme->file = file;
        vme->offset = offset;
        vme->read_bytes = page_read_bytes;
        vme->zero_bytes = page_zero_bytes;

        list_push_back(&mmfile->vme_list, &vme->mmap_elem);
        insert_vme(&thread_current()->vm, vme);
        
        offset += PGSIZE;
        read_bytes -= page_read_bytes;
        addr += PGSIZE;
    }
    
    return mmfile->mapid;
}

void munmap(int mapid){
   struct list_elem *e;
   struct mmap_file *mmfile;


   if(!list_empty(&thread_current()->mmap_list)){
       for( e = list_begin(&thread_current()->mmap_list); e != list_end(&thread_current()->mmap_list);e = list_next(e) ){
            
            mmfile= list_entry(e, struct mmap_file, elem);
            
            if( mmfile-> mapid == mapid){
                do_munmap(mmfile,e);
                return;
            }
       }
   }
}


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
        check_valid_string((const void *)arg[0]);
        f->eax = exec((const char *)arg[0]);
        break;
    case SYS_WAIT :
        get_argument(sp, arg, 1);
        f->eax = wait((pid_t)arg[0]);
        break;
    case SYS_CREATE :
        get_argument(sp, arg, 2);
        check_valid_string((const void *)arg[0]);
        f->eax = create((const char *)arg[0], (unsigned)arg[1]);
        break;
    case SYS_REMOVE :
        get_argument(sp, arg, 1);
        check_valid_string((const char *)arg[0]);
        f->eax = remove((const char *)arg[0]);
        break;
    case SYS_OPEN :
        get_argument(sp, arg, 1);
        check_valid_string((const void *)arg[0]);
        f->eax = open((const char *)arg[0]);
        break;
    case SYS_FILESIZE :
        get_argument(sp, arg, 1);
        f->eax = filesize((int)arg[0]);
        break;
    case SYS_READ :
        get_argument(sp, arg, 3);
        check_valid_buffer((void*)arg[1],(unsigned)arg[2],1);
        f->eax = read((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
        break;
    case SYS_WRITE :
        get_argument(sp, arg, 3);
        check_valid_buffer((void*)arg[1],(unsigned)arg[2],0);
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
        get_argument(sp,arg,2);
        sigaction((int)arg[0],(void*)arg[1]);
        break;
    case SYS_SENDSIG :
        get_argument(sp,arg,2);
        sendsig((pid_t)arg[0],(int)arg[1]);
        break;
    case SYS_YIELD :
        sched_yield(); 
        break;

    /* Project 3 */
    case SYS_MMAP :
        get_argument(sp,arg,2);
        f->eax = mmap((int)arg[0],(void*)arg[1]);
        break;
    case SYS_MUNMAP :
        get_argument(sp,arg,1);
        munmap((int)arg[0]);
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
