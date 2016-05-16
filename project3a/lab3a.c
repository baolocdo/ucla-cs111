#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <inttypes.h>

#define BUFFER_SIZE 4096

#define SUPERBLOCK_SIZE 1024
#define SUPERBLOCK_OFFSET 1024

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

int main(int argc, char **argv) {
  if (argc > 2) {
    perror("Unexpected amount of arguments");
    exit(1);
  }

  int ifd = open(argv[1], O_RDONLY);
  int ret = 0;
  if (ifd < 0) {
    perror("Unable to open image");
    exit(1);
  }
  char buffer[BUFFER_SIZE] = {0};
  char output_buffer[BUFFER_SIZE] = {0};

  // Read the superblock
  ret = pread(ifd, &superblock, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET);
  if (ret < SUPERBLOCK_SIZE) {
    perror("Unexpected superblock read");
    exit(1);
  }

  uint32_t block_size = 1024 << superblock.s_log_block_size;
  
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
  ret = sprintf(output_buffer, "%04x,%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32"\n", 
    superblock.s_magic, superblock.s_inodes_count, superblock.s_blocks_count, 
    block_size, fragment_size, superblock.s_blocks_per_group, 
    superblock.s_inodes_per_group, superblock.s_frags_per_group, superblock.s_first_data_block);
  write(ofd, output_buffer, ret);

  return 0;
}