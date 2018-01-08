#ifndef SUPERBLOCK_H_GUARD
#define SUPERBLOCK_H_GUARD

#include <inttypes.h>

typedef struct super_block_struct
{
    uint32_t inodes_count;
    uint32_t inodes_free;

    uint32_t inode_size;

    uint32_t blocks_count;
    uint32_t blocks_free;

    // block size in sectors
    uint32_t block_size;

    uint32_t inodemap_first_sector;
    uint32_t inode_first_sector;
    uint32_t spacemap_first_sector;
    uint32_t data_first_sector;

    uint32_t inodemap_size;
    uint32_t inodes_size;
    uint32_t spacemap_size;

    uint16_t magic;

    uint8_t padding[457];
} super_block_t;

std::ostream& operator<<(std::ostream& os, const super_block_t sb);

#endif