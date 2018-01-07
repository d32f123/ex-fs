#ifndef SUPERBLOCK_H_GUARD
#define SUPERBLOCK_H_GUARD

#include <inttypes.h>

typedef struct super_block_struct
{    
    super_block_struct(uint32_t i_count, uint32_t b_count, uint32_t b_size, uint16_t magic)
        : super_block_struct(i_count, i_count, b_count, b_count, b_size, magic) {}

    super_block_struct(uint32_t i_count, uint32_t i_free, 
        uint32_t b_count, uint32_t b_free, uint32_t b_size, uint16_t magic)
        : inodes_count {i_count}, inodes_free {i_free}, blocks_count {b_count},
            blocks_free {b_free}, block_size {b_size}, magic {magic} {}

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