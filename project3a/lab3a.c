#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <inttypes.h>
#include <math.h>

#define BUFFER_SIZE 4096

#define SUPERBLOCK_SIZE 1024
#define SUPERBLOCK_OFFSET 1024

int ifd = 0;
// TODO: test %x output for uint16, 32 and 64
// TODO: direct/indirect block 0 issue: per doc, a value of 0 in this array effectively terminates it with no further block being defined.  All the remaining entries of the array should still be set to 0.

int num_groups;
uint32_t block_size;

struct ext2_super_block {
  uint32_t  s_inodes_count;   /* Inodes count */
  uint32_t  s_blocks_count;   /* Blocks count */
  uint32_t  s_r_blocks_count; /* Reserved blocks count */
  uint32_t  s_free_blocks_count;  /* Free blocks count */
  uint32_t  s_free_inodes_count;  /* Free inodes count */
  uint32_t  s_first_data_block; /* First Data Block */
  uint32_t  s_log_block_size; /* Block size */
  uint32_t  s_log_frag_size;  /* Fragment size */
  uint32_t  s_blocks_per_group; /* # Blocks per group */
  uint32_t  s_frags_per_group;  /* # Fragments per group */
  uint32_t  s_inodes_per_group; /* # Inodes per group */
  uint32_t  s_mtime;    /* Mount time */
  uint32_t  s_wtime;    /* Write time */
  uint16_t  s_mnt_count;    /* Mount count */
  uint16_t  s_max_mnt_count;  /* Maximal mount count */
  uint16_t  s_magic;    /* Magic signature */
  uint16_t  s_state;    /* File system state */
  uint8_t   all_the_rest[SUPERBLOCK_SIZE - 60];
} superblock;

struct ext2_group_desc
{
  uint32_t  bg_block_bitmap;    /* Blocks bitmap block */
  uint32_t  bg_inode_bitmap;    /* Inodes bitmap block */
  uint32_t  bg_inode_table;   /* Inodes table block */
  uint16_t  bg_free_blocks_count; /* Free blocks count */
  uint16_t  bg_free_inodes_count; /* Free inodes count */
  uint16_t  bg_used_dirs_count; /* Directories count */
  uint16_t  bg_pad;
  uint32_t  bg_reserved[3];
};

struct ext2_inode {
  uint16_t  i_mode;   /* File mode */
  uint16_t  i_uid;    /* Low 16 bits of Owner Uid */
  uint32_t  i_size;   /* Size in bytes */
  uint32_t  i_atime;  /* Access time */
  uint32_t  i_ctime;  /* Creation time */
  uint32_t  i_mtime;  /* Modification time */
  uint32_t  i_dtime;  /* Deletion Time */
  uint16_t  i_gid;    /* Low 16 bits of Group Id */
  uint16_t  i_links_count;  /* Links count */
  uint32_t  i_blocks; /* Blocks count */
  uint32_t  i_flags;  /* File flags */
  uint32_t  i_osd1;       /* OS dependent 1 */
  uint32_t  i_block[15];/* Pointers to blocks */
  uint32_t  i_generation; /* File version (for NFS) */
  uint32_t  i_file_acl; /* File ACL */
  uint32_t  i_dir_acl;  /* Directory ACL */
  uint32_t  i_faddr;  /* Fragment address */
  uint16_t  i_osd2[6];        /* OS dependent 2 */
} inode;

struct ext2_directory {
  uint32_t  inode;
  uint16_t  rec_len;
  uint8_t   name_len;
  uint8_t   file_type;
  char      name[256];
} dirent;

// structure containing the linked list of blocks of an inode
struct blk_t {
  int addr;
  struct blk_t *next;
};

struct ext2_group_desc * group_desc_table;

// read the inode by number, if the inode is marked as '1' by inode-bitmap; 
// has to be called after group_desc_table and superblock is populated
// we do not call this in inode traversal, as we don't want the inode-bitmap to be read byte by byte
int read_inode(int inode, struct ext2_inode* return_inode) {
  if (inode > 0) {
    int i = inode - 1;
    int block_group = i / superblock.s_inodes_per_group;
    int block_group_offset = i % superblock.s_inodes_per_group;
    
    uint8_t inode_bitmap;
    pread(ifd, &inode_bitmap, 1, group_desc_table[block_group].bg_inode_bitmap * block_size + block_group_offset / 8);
  
    if (inode_bitmap & (1 << (block_group_offset % 8))) {
      pread(ifd, return_inode, sizeof(struct ext2_inode), group_desc_table[block_group].bg_inode_table * block_size + sizeof(struct ext2_inode) * block_group_offset);
      return 0;
    }
  }
  return -1;
}

struct blk_t* read_inode_indirect_block(struct blk_t* tail, int block_num) {
  int i;
  uint32_t *block = malloc(block_size);
  pread(ifd, block, block_size, block_num * block_size);
  for (i = 0; i < block_size / 4; i++) {
    if (block[i]) {
      struct blk_t* element = malloc(sizeof(struct blk_t));
      element->next = NULL;
      element->addr = block[i] * block_size;
      tail->next = element;
      tail = element;
    }
  }
  free(block);
  return tail;
}

struct blk_t* read_inode_double_indirect_block(struct blk_t* tail, int block_num) {
  int i;
  uint32_t *block = malloc(block_size);
  pread(ifd, block, block_size, block_num * block_size);
  for (i = 0; i < block_size / 4; i++) {
    if (block[i]) {
      tail = read_inode_indirect_block(tail, block[i]);
    }
  }
  free(block);
  return tail;
}

struct blk_t* read_inode_triple_indirect_block(struct blk_t* tail, int block_num) {
  int i;
  uint32_t *block = malloc(block_size);
  pread(ifd, block, block_size, block_num * block_size);
  for (i = 0; i < block_size / 4; i++) {
    if (block[i]) {
      tail = read_inode_double_indirect_block(tail, block[i]);
    }
  }
  free(block);
  return tail;
}

// read the direct/indirect block, store the result into a linked list
struct blk_t* read_inode_blocks(int inode_num) {
  struct ext2_inode this_inode;
  if (read_inode(inode_num, &this_inode) < 0) {
    return NULL;
  } else {
    struct blk_t *head = NULL;
    struct blk_t *tail = NULL;
    int j;

    // direct blocks, linked list elements are allocated on heap so that they persist after function returns
    // only non-0 blocks are considered
    for (j = 0; j < 12 && this_inode.i_block[j]; j++) {
      struct blk_t* element = malloc(sizeof(struct blk_t));
      element->next = NULL;
      element->addr = this_inode.i_block[j] * block_size;
      if (tail == NULL) {
        head = element;
      } else {
        tail->next = element;
      }
      tail = element;
    }
    if (j == 12) {
      // indirect blocks; 0 is not considered as an indirect block
      if (this_inode.i_block[12]) {
        tail = read_inode_indirect_block(tail, this_inode.i_block[12]);
      }
      if (this_inode.i_block[13]) {
        tail = read_inode_double_indirect_block(tail, this_inode.i_block[13]);
      }
      if (this_inode.i_block[14]) {
        tail = read_inode_triple_indirect_block(tail, this_inode.i_block[14]);
      }
    }
    return head;
  }
  return NULL;
}

// write the superblock
int write_superblock() {
  int ret = pread(ifd, &superblock, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET);
  if (ret < SUPERBLOCK_SIZE) {
    perror("Unexpected superblock read");
    exit(1);
  }
  
  // special handing for block_size
  block_size = 1024 << superblock.s_log_block_size;
  
  // special handling for fragment_size
  int32_t fragment_shift = (int32_t) superblock.s_log_frag_size;
  uint32_t fragment_size = 0;
  if (fragment_shift > 0)
    fragment_size = 1024 << fragment_shift;
  else
    fragment_size = 1024 >> -fragment_shift;

  int ofd = creat("my-super.csv", 0666);
  if (ofd < 0) {
    perror("Unable to open output file");
    exit(1);
  }
  char output_buffer[BUFFER_SIZE] = {0};
  ret = sprintf(output_buffer, "%x,%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32"\n", 
    superblock.s_magic, superblock.s_inodes_count, superblock.s_blocks_count, 
    block_size, fragment_size, superblock.s_blocks_per_group, 
    superblock.s_inodes_per_group, superblock.s_frags_per_group, superblock.s_first_data_block);
  return write(ofd, output_buffer, ret);
}

// write the group descriptor
int write_group_descriptor() {
  int start = superblock.s_first_data_block + 1;
  num_groups = ceil((float)superblock.s_blocks_count / (float)superblock.s_blocks_per_group);
  group_desc_table = malloc(sizeof(struct ext2_group_desc) * num_groups);
  
  int size = sizeof(struct ext2_group_desc) * num_groups;
  int ret = pread(ifd, group_desc_table, size, start * block_size);
  if (ret < size) {
    perror("Unexpected group_desc_table read");
    exit(1);
  }

  int ofd = creat("my-group.csv", 0666);
  if (ofd < 0) {
    perror("Unable to open output file");
    exit(1);
  }
  char output_buffer[BUFFER_SIZE] = {0};
  int i = 0;
  ret = 0;
  for (i = 0; i < num_groups; i++) {
    uint32_t blocks_contained = superblock.s_blocks_per_group;
    if (i == num_groups - 1) {
      blocks_contained = superblock.s_blocks_count % superblock.s_blocks_per_group;
    }
    ret = sprintf(output_buffer, "%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%x,%x,%x\n", 
      blocks_contained, group_desc_table[i].bg_free_blocks_count, group_desc_table[i].bg_free_inodes_count,
      group_desc_table[i].bg_used_dirs_count, group_desc_table[i].bg_inode_bitmap, group_desc_table[i].bg_block_bitmap, 
      group_desc_table[i].bg_inode_table);
    ret += write(ofd, output_buffer, ret);
  }

  return ret;
}

// write the bitmap entries
int write_bitmap_entry() {
  int ofd = creat("my-bitmap.csv", 0666);
  if (ofd < 0) {
    perror("Unable to open output file");
    exit(1);
  }
  char output_buffer[BUFFER_SIZE] = {0};
  int i = 0, j = 0, k = 0, ret = 0, inode_idx = 1, inode_upper_bound = 0, block_upper_bound = 0, done = 0, block_idx = 1;
  uint8_t *inode_bitmap_block = (uint8_t *)malloc(sizeof(uint8_t) * block_size);
  uint8_t *block_bitmap_block = (uint8_t *)malloc(sizeof(uint8_t) * block_size);

  for (i = 0; i < num_groups; i++) {
    block_upper_bound += superblock.s_blocks_per_group;
    inode_upper_bound += superblock.s_inodes_per_group;
    if (i == num_groups - 1) {
      block_upper_bound = superblock.s_blocks_count;
      inode_upper_bound = superblock.s_inodes_count;
    }

    done = 0;
    pread(ifd, block_bitmap_block, block_size, group_desc_table[i].bg_block_bitmap * block_size);
    for (j = 0; j < block_size; j++) {
      if (done) {
        break;
      }
      for (k = 0; k < 8; k++) {
        if (block_idx <= block_upper_bound) {
          if ((block_bitmap_block[j] & (1 << k)) == 0) {
            ret = sprintf(output_buffer, "%x,%"PRIu32"\n", 
              group_desc_table[i].bg_block_bitmap, block_idx);
            write(ofd, output_buffer, ret);
          }
          block_idx ++;
        } else {
          done = 1;
          break;
        }
      }
    }
    
    done = 0;
    pread(ifd, inode_bitmap_block, block_size, group_desc_table[i].bg_inode_bitmap * block_size);
    for (j = 0; j < block_size; j++) {
      if (done) {
        break;
      }
      for (k = 0; k < 8; k++) {
        if (inode_idx <= inode_upper_bound) {
          if ((inode_bitmap_block[j] & (1 << k)) == 0) {
            ret = sprintf(output_buffer, "%x,%"PRIu32"\n", 
              group_desc_table[i].bg_inode_bitmap, inode_idx);
            write(ofd, output_buffer, ret);
          }
          inode_idx ++;
        } else {
          done = 1;
          break;
        }
      }
    }
  }
  free(inode_bitmap_block);
  free(block_bitmap_block);
  return 1;
}

// write the inodes
int write_inodes() {
  int ofd = creat("my-inode.csv", 0666);
  if (ofd < 0) {
    perror("Unable to open output file");
    exit(1);
  }
  char output_buffer[BUFFER_SIZE] = {0};
  int i = 0, j = 0, k = 0, m = 0, ret = 0, inode_idx = 1, inode_upper_bound = 0, done = 0;
  uint8_t *inode_bitmap_block = (uint8_t *)malloc(sizeof(uint8_t) * block_size);

  for (i = 0; i < num_groups; i++) {
    inode_upper_bound += superblock.s_inodes_per_group;
    if (i == num_groups - 1) {
      inode_upper_bound = superblock.s_inodes_count;
    }
    
    done = 0;
    pread(ifd, inode_bitmap_block, block_size, group_desc_table[i].bg_inode_bitmap * block_size);
    for (j = 0; j < block_size; j++) {
      if (done) {
        break;
      }
      for (k = 0; k < 8; k++) {
        if (inode_idx <= inode_upper_bound) {
          if ((inode_bitmap_block[j] & (1 << k)) != 0) {
            pread(ifd, &inode, sizeof(struct ext2_inode), group_desc_table[i].bg_inode_table * block_size + sizeof(struct ext2_inode) * (j * 8 + k));
            char file_type = '?';
            if (inode.i_mode & 0x8000) {
              file_type = 'f';
            } else if (inode.i_mode & 0xA000) {
              file_type = 's';
            } else if (inode.i_mode & 0x4000) {
              file_type = 'd';
            }
            uint32_t owner_id = (inode.i_osd2[2] << 16) + inode.i_uid;
            uint32_t group_id = (inode.i_osd2[3] << 16) + inode.i_gid;
            uint32_t i_blocks = inode.i_blocks / (2 << superblock.s_log_block_size);
            ret = sprintf(output_buffer, "%u,%c,%o,%u,%u,%u,%x,%x,%x,%u,%u", 
              inode_idx, file_type, inode.i_mode, owner_id, group_id, 
              inode.i_links_count, inode.i_ctime, inode.i_mtime, inode.i_atime,
              inode.i_size, i_blocks);
            write(ofd, output_buffer, ret);
            for (m = 0; m < 15; m++) {
              ret = sprintf(output_buffer, ",%x", inode.i_block[m]);
              write(ofd, output_buffer, ret);
            }
            write(ofd, "\n", 1);
          }
          inode_idx ++;
        } else {
          done = 1;
          break;
        }
      }
    }
  }
  free(inode_bitmap_block);
  return 1;
}

// write the directory entries
int write_directory_entries() {
  // similar code as inode_traversal; did not record inode_traversal results so that each of these functions can work by themselves
  int ofd = creat("my-directory.csv", 0666);
  if (ofd < 0) {
    perror("Unable to open output file");
    exit(1);
  }
  char output_buffer[BUFFER_SIZE] = {0};
  int i = 0, j = 0, k = 0, ret = 0, inode_idx = 1, inode_upper_bound = 0, done = 0;
  uint8_t *inode_bitmap_block = (uint8_t *)malloc(sizeof(uint8_t) * block_size);

  for (i = 0; i < num_groups; i++) {
    inode_upper_bound += superblock.s_inodes_per_group;
    if (i == num_groups - 1) {
      inode_upper_bound = superblock.s_inodes_count;
    }
    
    done = 0;
    pread(ifd, inode_bitmap_block, block_size, group_desc_table[i].bg_inode_bitmap * block_size);
    for (j = 0; j < block_size; j++) {
      if (done) {
        break;
      }
      for (k = 0; k < 8; k++) {
        if (inode_idx <= inode_upper_bound) {
          if (inode_bitmap_block[j] & (1 << k)) {
            pread(ifd, &inode, sizeof(struct ext2_inode), group_desc_table[i].bg_inode_table * block_size + sizeof(struct ext2_inode) * (j * 8 + k));
            if (inode.i_mode & 0x4000) {
              struct blk_t *head = read_inode_blocks(inode_idx);
              struct blk_t *temp = head;
              uint32_t entry_idx = 0;

              while (temp != NULL) {
                int position = temp->addr;
                memset(dirent.name, 0, 256);
                pread(ifd, &dirent, 8, position);
                position += 8;
                pread(ifd, &(dirent.name), dirent.name_len, position);

                // TODO: debug 2nd field
                if (dirent.inode != 0) {
                  ret = sprintf(output_buffer, "%u,%u,%u,%u,%u,\"%s\"\n", 
                    inode_idx, entry_idx, dirent.rec_len, dirent.name_len, dirent.inode, dirent.name);
                  write(ofd, output_buffer, ret);
                }
                entry_idx++;

                while (dirent.rec_len + position < temp->addr + block_size) {
                  position += (dirent.rec_len - 8);
                  memset(dirent.name, 0, 256);
                  pread(ifd, &dirent, 8, position);
                  position += 8;
                  pread(ifd, &(dirent.name), dirent.name_len, position);

                  if (dirent.inode != 0) {
                    ret = sprintf(output_buffer, "%u,%u,%u,%u,%u,\"%s\"\n", 
                      inode_idx, entry_idx, dirent.rec_len, dirent.name_len, dirent.inode, dirent.name);
                    write(ofd, output_buffer, ret);
                  }
                  entry_idx++;
                }

                temp = temp->next;
              }
              
            }
          }
          inode_idx ++;
        } else {
          done = 1;
          break;
        }
      }
    }
  }
  free(inode_bitmap_block);
  return 1;
}

int main(int argc, char **argv) {
  if (argc > 2) {
    perror("Unexpected amount of arguments");
    exit(1);
  }

  ifd = open(argv[1], O_RDONLY);
  if (ifd < 0) {
    perror("Unable to open image");
    exit(1);
  }
  
  write_superblock();
  write_group_descriptor();
  write_bitmap_entry();
  write_inodes();
  write_directory_entries();

  return 0;
}