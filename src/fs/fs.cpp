#include "fs.h"

#include <fstream>

int file_system::load(std::string disk_file)
{
    if (this->disk.is_open())
        this->unload();

    int ret;
    ret = this->disk.load(disk_file);
    if (ret < 0)
        return ret;

    ret = this->disk.read_block(0, (char *)(&super_block), 1);
    return ret;
}

void file_system::unload()
{
    this->disk.unload();
}

int file_system::init(std::string disk_file, uint32_t inodes_count, 
    std::size_t disk_size, uint32_t block_size)
{
    if (this->disk.is_open())
        this->unload();
    
    int ret;
    ret = this->disk.create(disk_file, disk_size);
    if (ret < 0)
        return ret;

    // init the super_block struct
    // total blocks we have = total disk size (in sectors)
    //                      -1 sector for superblock
    uint32_t blocks_count = disk_size - 1 - 
    super_block_t super_block(inodes_count, )

}