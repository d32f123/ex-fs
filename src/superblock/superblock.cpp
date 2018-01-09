#include <iostream>
#include "superblock.h"

using std::endl;

std::ostream& operator<<(std::ostream& os, const super_block_t sb)
{
    return os << "Inodes total: " << sb.inodes_count << endl
        << "Inodes free: " << sb.inodes_free << endl
        << "Inode struct size: " << sb.inode_size << endl
        << endl
        << "Blocks total: " << sb.blocks_count << endl
        << "Blocks free: " << sb.blocks_free << endl
        << "Block size (in sectors): " << sb.block_size << endl
        << endl
        << "Inode map first sector: " << sb.inodemap_first_sector << endl
        << "Inode map size (in blocks): " << sb.inodemap_size << endl
        << endl
        << "Inodes first sector: " << sb.inode_first_sector << endl
        << "Inodes size (in blocks): " << sb.inodes_size << endl
        << endl 
        << "Space map first sector: " << sb.spacemap_first_sector << endl
        << "Space map size (in blocks): " << sb.spacemap_size << endl
        << endl
        << "Data first sector: " << sb.data_first_sector << endl
        << "Magic: " << sb.magic << endl;
}