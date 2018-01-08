#include "fs.h"

#include <fstream>
#include <math.h>

static uint32_t bytes_to_blocks(uint32_t x, uint32_t bl_size)
{
    auto temp = x / SECTOR_SIZE + (x % SECTOR_SIZE != 0);
    temp = temp / bl_size + (temp % bl_size != 0);
    return temp;
}

inline static uint32_t bits_to_blocks(uint32_t x, uint32_t bl_size)
{
    return bytes_to_blocks((x >> 3) + (x % 8 != 0), bl_size);
}

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

    // init inode map

    auto inodemap_size_bytes = super_block_.inodemap_size * super_block_.block_size * SECTOR_SIZE;
    char * inode_buffer = new char[inodemap_size_bytes];
    this->disk_.read_block(super_block_.inodemap_first_sector, inode_buffer, super_block_.inodemap_size * super_block_.block_size);
    this->inode_map_ = new space_map(reinterpret_cast<uint8_t *>(inode_buffer), inodemap_size_bytes << 3);
    delete[] inode_buffer;

	// init data buffer
    this->data_buffer_ = new char[super_block_.block_size * SECTOR_SIZE];

	// init space map
	auto spacemap_size_bytes = super_block_.spacemap_size * super_block_.block_size * SECTOR_SIZE;
	char * buffer = new char[spacemap_size_bytes];
	this->disk_.read_block(super_block_.spacemap_first_sector, buffer, super_block_.spacemap_size * super_block_.block_size);
	this->space_map_ = new space_map(reinterpret_cast<uint8_t *>(buffer), spacemap_size_bytes << 3);
    delete[] buffer;

    return 0;
}

void file_system::unload()
{
    delete[] data_buffer_;
	data_buffer_ = nullptr;

	delete this->space_map_;
	this->space_map_ = nullptr;

    delete this->inode_map_;
    this->inode_map_ = nullptr;
    
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
    //                      - sectors required for inode map
    //                      - sectors required for inodes
    uint32_t inodes_size = bytes_to_blocks(sizeof(inode_t) * inodes_count, block_size);

    uint32_t inodemap_size = bits_to_blocks(inodes_count, block_size);

    uint32_t blocks_count = (disk_size - 1 - (inodes_size * block_size) -(inodemap_size * block_size)) / block_size;

    uint32_t spacemap_size = bits_to_blocks(blocks_count, block_size);

	super_block_t sb;
	sb.inodes_count = inodes_count;
	sb.inodes_free = inodes_count;
	sb.inode_size = sizeof(inode_t);
	sb.blocks_count = blocks_count;
	sb.blocks_free = blocks_count;
	sb.block_size = block_size;
    sb.inodemap_first_sector = 1;
    sb.inodemap_size = inodemap_size;
	sb.inode_first_sector = inodemap_size * block_size + 1; // TODO: CHANGE
	sb.spacemap_first_sector = sb.inode_first_sector + inodes_size * block_size;
	sb.data_first_sector = sb.spacemap_first_sector;
	sb.inodes_size = inodes_size;
	sb.spacemap_size = spacemap_size;
	sb.magic = 0xBEEF;

    this->super_block_ = sb;
    this->disk_.write_block(0, reinterpret_cast<char *>(&this->super_block_), 1);

    // creating inode map

    this->inode_map_ = new space_map((inodemap_size * block_size * SECTOR_SIZE) << 3);

    this->disk_.write_block(sb.inodemap_first_sector, reinterpret_cast<char *>(this->inode_map_->bits_arr), inodemap_size * block_size);

	// creating the space mapping

	this->space_map_ = new space_map((spacemap_size * block_size * SECTOR_SIZE) << 3);
	for (uint32_t i = 0; i < spacemap_size; ++i)
		this->space_map_->set(true, i);

	this->disk_.write_block(sb.spacemap_first_sector, reinterpret_cast<char *>(this->space_map_->bits_arr), spacemap_size * block_size);

	// init data buffer
	this->data_buffer_ = new char[super_block_.block_size * SECTOR_SIZE];

	return 0;
}