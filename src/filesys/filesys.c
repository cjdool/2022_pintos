#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  bc_init();

  if (format) 
    do_format ();

  free_map_open ();
  struct dir * root = dir_open_root();
  thread_current()->cur_dir = root;
  dir_init(root, root);
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  bc_term();
}


struct dir* parse_path(const char *path_name, char *file_name){
    struct dir* dir;
    char *token, *next_token, *save_ptr;
    struct inode *inode;
    if(thread_current()->cur_dir == NULL){
        return NULL;
    }

    if(path_name == NULL || file_name == NULL){
        return NULL;
    }
    if(strlen(path_name) == 0){
        return NULL;
    }
    char * tmp_name = calloc(1, strlen(path_name)+1);
    char * free_name = tmp_name;

    strlcpy(tmp_name,path_name, strlen(path_name)+1);
    
    if(path_name[0] == '/'){ //absolute path
       dir = dir_open_root(); 
    }else{ //relative path   
        dir = dir_reopen(thread_current()->cur_dir);
    }
    token = strtok_r(tmp_name, "/", &save_ptr);
    next_token = strtok_r(NULL, "/", &save_ptr);

    if(token !=NULL && next_token == NULL){
        if(strlen(token) > NAME_MAX +1){
            dir_close(dir);
            free(free_name);
            return NULL;
        }
    }
    while(token != NULL && next_token != NULL){
        if(strlen(token) >NAME_MAX+1 || strlen(next_token) > NAME_MAX +1){
            dir_close(dir);
            free(free_name);
            return NULL;
        }
        if(!dir_lookup(dir,token,&inode)){
            dir_close(dir);
            free(free_name);
            return NULL;
        }
        dir_close(dir);
        if(!inode_is_dir(inode)){
            free(free_name);
            return NULL;
        }
        dir = dir_open(inode); 

        strlcpy(token, next_token, strlen(next_token)+1);
        next_token = strtok_r(NULL, "/", &save_ptr);
    }


    if( token == NULL ) {
        strlcpy(file_name, ".", 2);
    }else {
        strlcpy(file_name, token, strlen(token)+1);
    }
    free(free_name);
    return dir;
}




/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  char* file_name = calloc(1, NAME_MAX+1);
  struct dir *dir = parse_path (name, file_name);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, 0)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  free(file_name);
  dir_close (dir);

  return success;
}

bool filesys_create_dir (const char *name){
  if(strcmp(name, "")==0){
    return false;
  }
  block_sector_t inode_sector = 0;
  char* dir_name = calloc(1, NAME_MAX +1 );
  struct dir *dir = parse_path (name, dir_name);
  struct dir *child_dir ;
  struct inode *inode;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 16)
                  && dir_add (dir, dir_name, inode_sector));
  if (!success && inode_sector != 0) {
    free_map_release (inode_sector, 1);
  }else{
    dir_lookup(dir, dir_name, &inode); 
    child_dir = dir_open(inode);
    dir_init(dir, child_dir);
    dir_close(child_dir);
  }

  free(dir_name);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char* file_name = calloc(1, NAME_MAX +1);
  struct dir *dir = parse_path (name, file_name);
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);
  free(file_name);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char * file_name = calloc(1, NAME_MAX +1 );
  struct dir *dir = parse_path (name, file_name);
  bool success = dir != NULL && dir_remove (dir, file_name);
  
  free(file_name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

bool filesys_chdir(const char *name){

    char* dir_name = calloc(1, NAME_MAX+1);
    struct dir* dir = parse_path(name,dir_name);
    struct inode *inode = NULL;
    struct dir* chdir;
    bool success = false;

    if(dir != NULL){
        if(!dir_lookup(dir,dir_name, &inode)){
            goto done;
        }
        chdir = dir_open(inode);
        if(thread_current()->cur_dir !=NULL){
            dir_close(thread_current()->cur_dir);
        }
        thread_current()->cur_dir = chdir;
        success = true;
    }
done:

    free(dir_name);
    return success;
    
}
