#ifndef SUPERBLOCK_H_GUARD
#define SUPERBLOCK_H_GUARD

#include <inttypes.h>

typedef struct super_block_struct
{    
    uint32_t inodes_count;
    uint32_t inodes_free;

    uint32_t blocks_count;
    uint32_t blocks_free;

    // block size in sectors
    uint32_t block_size;

    uint16_t magic;

    uint8_t padding[490];
} super_block_t;

#endif