#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
/* Number of direct block data pointer (128-4) */
#define DIRECT_BLOCK_ENTRIES (BLOCK_SECTOR_SIZE / sizeof(uint32_t) - 4)
/* Number of indirect block data pointer (128) */
#define INDIRECT_BLOCK_ENTRIES (BLOCK_SECTOR_SIZE / sizeof(block_sector_t))

/* How to point disk block number. */
enum direct_t {
    NORMAL_DIRECT,
    INDIRECT,
    DOUBLE_INDIRECT,
    OUT_LIMIT
};

/* How to access a block address and save offset in index block. */
struct sector_location
{
    int directness;
    uint32_t index1;
    uint32_t index2;
};

/* index block. */
struct inode_indirect_block
{
    block_sector_t map_table[INDIRECT_BLOCK_ENTRIES];
};

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
    off_t length;                                          /* File size in bytes */
    unsigned magic;                                        /* Magic number */
    block_sector_t direct_map_table[DIRECT_BLOCK_ENTRIES]; /* Direct block data pointer */
    block_sector_t indirect_block_sec;                     /* Indirect block data pointer */
    block_sector_t double_indirect_block_sec;              /* Double indirect block data pointer */
};

/* In-memory inode. */
struct inode 
{
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock extend_lock;            /* Semaphore lock. */
};

/* Reads inode from buffer cache and return it. */
static bool get_disk_inode (const struct inode *inode, struct inode_disk *inode_disk)
{
    return bc_read(inode->sector, inode_disk, 0, sizeof(inode_disk), 0);
}

/* Sets sector location structure. */
static void locate_byte (off_t pos, struct sector_location *sec_loc)
{
    off_t pos_sector = pos / BLOCK_SECTOR_SIZE;
    
    /* Direct index case */
    if (pos_sector < DIRECT_BLOCK_ENTRIES){
        sec_loc->directness = NORMAL_DIRECT;
        sec_loc->index1 = pos_sector;
    }
    /* Indirect index case */
    else if (pos_sector < (off_t)(DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES)){
        sec_loc->directness = INDIRECT;
        sec_loc->index1 = pos_sector;
    }
    /* Double indirect index case */
    else if (pos_sector < (off_t)(DIRECT_BLOCK_ENTRIES + 
             INDIRECT_BLOCK_ENTRIES * (INDIRECT_BLOCK_ENTRIES + 1))){
        sec_loc->directness = DOUBLE_INDIRECT;
        sec_loc->index1 = pos_sector / INDIRECT_BLOCK_ENTRIES;
        sec_loc->index2 = pos_sector % INDIRECT_BLOCK_ENTRIES;
    }
    else{
        sec_loc->directness = OUT_LIMIT;
    }
}

/* Returns offset to bytes value. */
static inline off_t map_table_offset (int index)
{
    return (off_t)(index * sizeof(block_sector_t));
}

/* Updates new disk block number to inode_disk. */
static bool register_sector (struct inode_disk *inode_disk, 
                             block_sector_t new_sector, 
                             struct sector_location sec_loc){
    block_sector_t *blk_sec;
    struct inode_indirect_block *new_block, *new_block2; 

    switch(sec_loc.directness)
    {
        case NORMAL_DIRECT:
            inode_disk->direct_map_table[sec_loc.index1] = new_sector;
            break;
        case INDIRECT:
            blk_sec = &inode_disk->indirect_block_sec;
            new_block2 = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
            if (new_block2 == NULL)
                goto rs_return_phase;
            if (*blk_sec == (block_sector_t) - 1) // first use case
            {
                if(!free_map_allocate(1, blk_sec))
                    goto rs_free_phase2;
                memset(new_block2, -1, sizeof(struct inode_indirect_block));
            }
            else // not first use case
            {
                if(!bc_read(*blk_sec, new_block2, 0, sizeof(struct inode_indirect_block), 0))
                    goto rs_free_phase2;
            }
            if(new_block2->map_table[sec_loc.index1] == (block_sector_t) - 1)
                new_block2->map_table[sec_loc.index1] = new_sector;
            if(!bc_write(*blk_sec, new_block2, 0, sizeof(struct inode_indirect_block), 0))
                goto rs_free_phase2;
            free(new_block2);
            break;
        case DOUBLE_INDIRECT:
            blk_sec = &inode_disk->double_indirect_block_sec;
            new_block2 = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
            if (new_block2 == NULL)
                goto rs_return_phase;
            if (*blk_sec == (block_sector_t) - 1) // first use case
            {
                if(!free_map_allocate(1, blk_sec))
                    goto rs_free_phase2;
                memset(new_block2, -1, sizeof(struct inode_indirect_block));
            }
            else // not first use case
            {
                if (!bc_read(*blk_sec, new_block2, 0, sizeof(struct inode_indirect_block), 0))
                    goto rs_free_phase2;
            }
            *blk_sec = new_block2->map_table[sec_loc.index2];
            new_block = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
            if (new_block == NULL)
                goto rs_free_phase2;
            if (*blk_sec == (block_sector_t) - 1) // first use case
            {
                if(!free_map_allocate(1, blk_sec))
                    goto rs_free_phase1;
                memset(new_block, -1, sizeof(struct inode_indirect_block));
            }
            else // not first use case
            {
                if (!bc_read(*blk_sec, new_block, 0, sizeof(struct inode_indirect_block), 0))
                    goto rs_free_phase1;
            }
            if (new_block->map_table[sec_loc.index1] == (block_sector_t) - 1)
            {
                new_block->map_table[sec_loc.index1] = new_sector;
            }
            if (!bc_write(inode_disk->double_indirect_block_sec, new_block2, 0, sizeof(struct inode_indirect_block), 0)) // write double indirect table
                goto rs_free_phase1;
            if (!bc_write(*blk_sec, new_block, 0, sizeof(struct inode_indirect_block), 0)) // write indirect table
                goto rs_free_phase1;
            free(new_block);
            free(new_block2);
            break;
        default:
            goto rs_return_phase;
    }
    return true;

rs_free_phase1:
    free(new_block);
rs_free_phase2:
    free(new_block2);
rs_return_phase:
    return false;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode_disk *inode_disk, off_t pos) 
{
    ASSERT (inode_disk != NULL);
    struct inode_indirect_block *ind_block;
    struct sector_location sec_loc;
    block_sector_t result_sec = inode_disk->indirect_block_sec;

    if (pos < inode_disk->length){
        locate_byte(pos, &sec_loc);
        switch (sec_loc.directness){
            case NORMAL_DIRECT:
                result_sec = inode_disk->direct_map_table[sec_loc.index1];
                break;
            case INDIRECT:
                ind_block = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
                if (ind_block == NULL)
                    return -1;
                if (result_sec == (block_sector_t) - 1)
                    goto bts_free_phase;
                if (!bc_read(result_sec, &ind_block, 0, sizeof(struct inode_indirect_block), 0))
                    goto bts_free_phase;
                result_sec = ind_block->map_table[sec_loc.index1];
                free (ind_block);
                break;
            case DOUBLE_INDIRECT:
                ind_block = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
                if (ind_block == NULL)
                    goto bts_return_phase;
                if (inode_disk->double_indirect_block_sec == (block_sector_t) - 1)
                    goto bts_free_phase;
                if (!bc_read(inode_disk->double_indirect_block_sec, &ind_block, 0, sizeof(struct inode_indirect_block), 0))
                    goto bts_free_phase;
                result_sec = ind_block->map_table[sec_loc.index2];
                if (result_sec == (block_sector_t) - 1)
                    goto bts_free_phase;
                if (!bc_read(result_sec, &ind_block, 0, sizeof(struct inode_indirect_block), 0))
                    goto bts_free_phase;
                result_sec = ind_block->map_table[sec_loc.index1];
                free (ind_block);
                break;
            default:
                result_sec = -1;
        }
    }
    else
    {
        result_sec = -1;
    }
    return result_sec;

bts_free_phase:
    free(ind_block);
bts_return_phase:
    return -1;
}

/* Frees all data blocks in inode. */
static void free_inode_sectors (struct inode_disk *inode_disk){
    int i, j;
    struct inode_indirect_block ind_block, ind_block2;

    /* Double indirect index case */
    if(inode_disk->double_indirect_block_sec > 0){
        i = 0;
        bc_read(inode_disk->double_indirect_block_sec, &ind_block, 0, sizeof(struct inode_indirect_block), 0);
        while (ind_block.map_table[i] > 0){
            j = 0;
            bc_read(ind_block.map_table[i], &ind_block2, 0, sizeof(struct inode_indirect_block), 0);
            while (ind_block2.map_table[j] > 0){
                free_map_release(ind_block2.map_table[j], 1);
                j++;
            }
            free_map_release(ind_block.map_table[i], 1);
            i++;
        }
        free_map_release(inode_disk->double_indirect_block_sec, 1);
    }

    /* Indirect index case */
    if(inode_disk->indirect_block_sec > 0){
        i = 0;
        bc_read(inode_disk->indirect_block_sec, &ind_block, 0, sizeof(struct inode_indirect_block), 0);
        while(ind_block.map_table[i] > 0){
            free_map_release(ind_block.map_table[i], 1);
            i++;
        }
        free_map_release(inode_disk->indirect_block_sec, 1); 
    }

    /* Direct index case */
    i = 0;
    while (inode_disk->direct_map_table[i] > 0){
        free_map_release(inode_disk->direct_map_table[i], 1);
        i++;
    }
}

/* Allocates new disk blocks and updates inode. */
static bool inode_update_file_length (struct inode_disk *inode_disk, off_t start_pos, off_t end_pos){
    static char zeroes[BLOCK_SECTOR_SIZE];
    struct sector_location sec_loc;
    block_sector_t sector_idx;
    int size = end_pos - start_pos;
    int offset = start_pos;

    if (size == 0)
        return true;
    if (size < 0)
        return false;

    inode_disk->length = end_pos;
    while(size > 0){
        sector_idx = byte_to_sector(inode_disk, offset);
        if(sector_idx == (block_sector_t) -1){ // not already allocated case 
            if(!free_map_allocate(1, &sector_idx))
                return false;
            locate_byte(offset, &sec_loc);
            if (!register_sector (inode_disk, sector_idx, sec_loc))
                return false; 
            if(!bc_write(sector_idx, zeroes, 0, BLOCK_SECTOR_SIZE, 0))
                return false;
        }
        size -= BLOCK_SECTOR_SIZE;
        offset += BLOCK_SECTOR_SIZE;
    }
    return true;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
      memset(disk_inode, -1, sizeof(struct inode_disk));
      disk_inode->length = 0;
      if (!inode_update_file_length(disk_inode, disk_inode->length, length))
      {
          free(disk_inode);
          return false;
      }
      disk_inode->magic = INODE_MAGIC;
      bc_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0); 
      free (disk_inode);
      success = true;
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->extend_lock);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  struct inode_disk inode_disk;
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
      {
          get_disk_inode(inode, &inode_disk);
          free_inode_sectors(&inode_disk);
          free_map_release (inode->sector, 1);
      }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  struct inode_disk inode_disk;

  lock_acquire(&inode->extend_lock);
  get_disk_inode(inode, &inode_disk);
  while (size > 0) 
  {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (&inode_disk, offset);
      lock_release(&inode->extend_lock);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_disk.length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0){
        lock_acquire(&inode->extend_lock); // exit trick
        break;
      }

      if(!bc_read(sector_idx, buffer, bytes_read, chunk_size, sector_ofs)){
          lock_acquire(&inode->extend_lock); // exit trick
          break; 
      }
    
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;

      lock_acquire(&inode->extend_lock);
  }

  lock_release(&inode->extend_lock);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  struct inode_disk inode_disk;

  if (inode->deny_write_cnt)
    return 0;
  lock_acquire(&inode->extend_lock);
  get_disk_inode(inode, &inode_disk);
  if(inode_disk.length < offset + size)
  {
      inode_update_file_length(&inode_disk, inode_disk.length, offset + size);
      bc_write(inode->sector, &inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
  }
  while (size > 0) 
  {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (&inode_disk, offset);
      lock_release(&inode->extend_lock);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_disk.length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0){
          lock_acquire(&inode->extend_lock); // exit trick
          break;
      }

     if(!bc_write(sector_idx, buffer, bytes_written, chunk_size, sector_ofs)){
         lock_acquire(&inode->extend_lock); // exit trick
         break;
     }
     /* Advance. */
     size -= chunk_size;
     offset += chunk_size;
     bytes_written += chunk_size;

     lock_acquire(&inode->extend_lock);
  }

  lock_release(&inode->extend_lock);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk inode_disk;
  get_disk_inode(inode, &inode_disk);
  return inode_disk.length;
}
