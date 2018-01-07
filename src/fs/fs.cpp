#include "fs.h"

#include <fstream>
#include <math.h>
#include <stdint.h>

int file_system::load(std::string & disk_file)
{
    if (this->disk_.is_open())
        this->unload();

    int ret;
    ret = this->disk_.load(disk_file);
    if (ret < 0)
        return ret;

	// read the superblock
    ret = this->disk_.read_block(0, reinterpret_cast<char *>(&super_block_), 1);
    if (ret < 0)
        return ret;

	// init data buffer
    this->data_buffer_ = new char[super_block_.block_size * SECTOR_SIZE];

	// init space map
	auto spacemap_size_bytes = super_block_.spacemap_size * super_block_.block_size * SECTOR_SIZE;
	auto * buffer = new char[spacemap_size_bytes];
	this->disk_.read_block(super_block_.spacemap_first_sector, buffer, super_block_.spacemap_size * super_block_.block_size);
	space_map_ = new space_map(reinterpret_cast<uint8_t *>(buffer), spacemap_size_bytes << 3);

    return 0;
}

void file_system::unload()
{
    delete[] data_buffer_;
	data_buffer_ = nullptr;

	delete this->space_map_;
	this->space_map_ = nullptr;
    
    this->disk_.unload();
}

int file_system::init(std::string & disk_file, const uint32_t inodes_count, 
    std::size_t disk_size, const uint32_t block_size)
{
    if (this->disk_.is_open())
        this->unload();
    
    int ret;
    ret = this->disk_.create(disk_file, disk_size);
    if (ret < 0)
        return ret;

    // init the super_block struct
    // total blocks we have = total disk size (in sectors)
    //                      -1 sector for superblock
    //                      - sectors required for inodes
    uint32_t inodes_size = sizeof(inode_t) * inodes_count; // in bytes
    inodes_size = inodes_size / SECTOR_SIZE + (inodes_size % SECTOR_SIZE != 0); // in sectors
    inodes_size = inodes_size / block_size + (inodes_size % block_size != 0); // in blocks

    uint32_t blocks_count = (disk_size - 1 - (inodes_size * block_size)) / block_size;

    uint32_t spacemap_size = (blocks_count >> 3) + (blocks_count % 8 != 0); // in bytes
    spacemap_size = spacemap_size / SECTOR_SIZE + (spacemap_size % SECTOR_SIZE != 0); // in sectors
    spacemap_size = spacemap_size / block_size + (spacemap_size % block_size != 0); // in blocks

	super_block_t sb;
	sb.inodes_count = inodes_count;
	sb.inodes_free = inodes_count;
	sb.inode_size = sizeof(inode_t);
	sb.blocks_count = blocks_count;
	sb.blocks_free = blocks_count;
	sb.block_size = block_size;
	sb.inode_first_sector = 1;
	sb.spacemap_first_sector = inodes_size * block_size + 1;
	sb.data_first_sector = inodes_size * block_size + 1;
	sb.inodes_size = inodes_size;
	sb.spacemap_size = spacemap_size;
	sb.magic = 0xBEEF;

    this->super_block_ = sb;
    this->disk_.write_block(0, reinterpret_cast<char *>(&this->super_block_), 1);


	// creating the space mapping

	this->space_map_ = new space_map(spacemap_size * block_size * SECTOR_SIZE);
	for (uint32_t i = 0; i < spacemap_size; ++i)
		this->space_map_->set(true, i);

	this->disk_.write_block(sb.spacemap_first_sector, reinterpret_cast<char *>(this->space_map_->bits_arr), spacemap_size * block_size);

	// init data buffer
	this->data_buffer_ = new char[super_block_.block_size * SECTOR_SIZE];

	// TODO: INIT INODES
	// TODO: ADD FREE INODE MAP

	return 0;
}