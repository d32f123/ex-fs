#include "fs.h"

#include <fstream>
#include <vector>
#include <math.h>
#include <time.h>
#include <string.h>

#include "../inode/inode.h"
#include "../spacemap/spacemap.h"
#include "../superblock/superblock.h"
#include "../errors.h"
#include "../disk/disk.h"

std::vector<std::string> split(const std::string &text, char sep) 
{
    std::vector<std::string> tokens;
    std::size_t start = 0, end = 0;
    while ((end = text.find(sep, start)) != std::string::npos)
    {
        tokens.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(text.substr(start));
    return tokens;
}

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
    this->data_buffer_ = new char[super_block_.block_size * SECTOR_SIZE];

    // creating inode map

    this->inode_map_ = new space_map((inodemap_size * block_size * SECTOR_SIZE) << 3);

    inode_map_->set(true, INODE_ROOT_ID);

    this->disk_.write_block(sb.inodemap_first_sector, reinterpret_cast<char *>(this->inode_map_->bits_arr), inodemap_size * block_size);

    // create root inode
    auto curr_time = time(NULL);
    inode_t root;
    root.f_type = file_type::dir;
    root.access_time = curr_time;
    root.change_time = curr_time;
    root.modify_time = curr_time;
    root.links_count = 1;

    ret = write_inode(INODE_ROOT_ID, &root);
    if (ret < 0)
        return ret;

	// creating the space mapping

    this->space_map_ = new space_map((spacemap_size * block_size * SECTOR_SIZE) << 3);
	for (uint32_t i = 0; i < spacemap_size; ++i)
		this->space_map_->set(true, i);

	this->disk_.write_block(sb.spacemap_first_sector, reinterpret_cast<char *>(this->space_map_->bits_arr), spacemap_size * block_size);

	// init data buffer
	

	return 0;
}

int file_system::create(std::string & file_name)
{
    if (file_name.empty())
        return EDIR_INVALID_PATH;

    auto last_slash = file_name.find_last_of('/');
    if (last_slash == std::string::npos)
        return EDIR_INVALID_PATH;
    
    uint32_t last_dir_inode;
    auto dir_path = file_name.substr(0, last_slash);
    auto small_name = file_name.substr(last_slash + 1, std::string::npos);
    auto ret = get_inode_by_path(dir_path, &last_dir_inode);
    if (ret < 0)
        return ret;

    directory dir = directory(last_dir_inode, this);

    // check if file exists
    auto dirent = dir.find(small_name);
    if (dirent.inode_n != INODE_INVALID)
    {
        return EDIR_FILE_EXISTS;
    }

    uint32_t inode_num = get_free_inode();
    auto inode = get_new_inode(file_type::regular, 0755);

    ret = write_inode(inode_num, &inode);

    if (ret < 0)
        return ret;

    inode_map_->set(true, inode_num);
    dir.add_entry(inode_num, small_name);
    return 0;
}

int file_system::link(std::string & original_file, std::string & new_file)
{
    if (original_file.empty() || new_file.empty())
        return EDIR_INVALID_PATH;

    auto last_slash_o = original_file.find_last_of('/');
    if (last_slash_o == std::string::npos)
        return EDIR_INVALID_PATH;

    auto last_slash_n = new_file.find_last_of('/');
    if (last_slash_n == std::string::npos)
        return EDIR_INVALID_PATH;

    uint32_t orig_file_inode;
    auto ret = get_inode_by_path(original_file, &orig_file_inode);
    if (ret < 0)
        return ret;

    // set up the new file
    uint32_t last_dir_inode;
    auto dir_path = new_file.substr(0, last_slash_n);
    auto small_name = new_file.substr(last_slash_n + 1, std::string::npos);
    ret = get_inode_by_path(dir_path, &last_dir_inode);
    if (ret < 0)
        return ret;

    directory dir = directory(last_dir_inode, this);

    // check if file exists
    auto dirent = dir.find(small_name);
    if (dirent.inode_n != INODE_INVALID)
    {
        return EDIR_FILE_EXISTS;
    }

    // up the counter
    inode_t orig_file;
    ret = read_inode(orig_file_inode, &orig_file);
    if (ret < 0)
        return ret;

    auto curr_time = time(NULL);
    orig_file.access_time = curr_time;
    orig_file.change_time = curr_time;
    ++orig_file.links_count;

    ret = write_inode(orig_file_inode, &orig_file);
    if (ret < 0)
        return ret;

    // add entry
    dir.add_entry(orig_file_inode, small_name);
    return 0;
}

int file_system::unlink(std::string & file_name)
{
    if (file_name.empty())
        return EDIR_INVALID_PATH;

    auto last_slash = file_name.find_last_of('/');
    if (last_slash == std::string::npos)
        return EDIR_INVALID_PATH;
    
    uint32_t file_inode;
    uint32_t last_dir_inode;
    auto dir_path = file_name.substr(0, last_slash);
    auto small_name = file_name.substr(last_slash + 1, std::string::npos);
    // get dir inode
    auto ret = get_inode_by_path(dir_path, &last_dir_inode);
    if (ret < 0)
        return ret;
    // get file inode
    ret = get_inode_by_path(file_name, &file_inode);
    if (ret < 0)
        return ret;

    directory dir = directory(last_dir_inode, this);
    
    // get the counter down
    inode_t inode;
    ret = read_inode(file_inode, &inode);
    if (ret < 0)
        return ret;

    auto curr_time = time(NULL);
    inode.access_time = curr_time;
    inode.change_time = curr_time;
    --inode.links_count;

    // check if links count is 0 (or somehow less than 0)
    if (inode.links_count > super_block_.inodes_count)
    {
        std::cerr << "inode " << file_inode << " has link count: " << inode.links_count << std::endl;
    }
    if (inode.links_count <= 0)
    {
        // clear the space
        file tmp = file(file_inode, this);
        tmp.trunc(0);
        // clear the inode
        inode_map_->set(false, file_inode);
    }
    else
    {
        ret = write_inode(file_inode, &inode);
        if (ret < 0)
            return ret;
    }
    // remove file from directory
    dir.remove_entry(small_name);

    return 0;
}

fid_t file_system::open(std::string & disk_file)
{
    uint32_t inode;
    auto ret = get_inode_by_path(disk_file, &inode);
    if (ret < 0)
        return ret;
    auto tmp = file(inode, this);
    files_.push_back(tmp);
    return files_.size() - 1;
}

int file_system::close(fid_t fid)
{
    if (fid > files_.size())
        return EFID_INVALID_ID;
    files_.erase(files_.begin() + fid);
    return 0;
}

int file_system::read(fid_t fid, char * buffer, std::size_t size)
{
    if (fid > files_.size())
        return EFID_INVALID_ID;
    files_[fid].read(buffer, size);
    return 0;
}

int file_system::write(fid_t fid, const char * buffer, std::size_t size)
{
    if (fid > files_.size())
        return EFID_INVALID_ID;
    files_[fid].write(buffer, size);
    return 0;
}

int file_system::seek(fid_t fid, std::size_t pos)
{
    if (fid > files_.size())
        return EFID_INVALID_ID;
    files_[fid].seek(pos);
    return 0;
}

int file_system::trunc(fid_t fid, std::size_t new_length)
{
    if (fid > files_.size())
        return EFID_INVALID_ID;
    files_[fid].trunc(new_length);
    return 0;
}

int file_system::mkdir(std::string & dir_name)
{
    if (dir_name.empty())
        return EDIR_INVALID_PATH;
    
    auto last_slash = dir_name.find_last_of('/');
    if (last_slash == std::string::npos)
        return EDIR_INVALID_PATH;

    uint32_t last_dir_inode;
    auto dir_path = dir_name.substr(0, last_slash);
    auto small_name = dir_name.substr(last_slash + 1, std::string::npos);
    auto ret = get_inode_by_path(dir_path, &last_dir_inode);
    if (ret < 0)
        return ret;
    
    directory dir = directory(last_dir_inode, this);

    // check if filename already exists
    auto dirent = dir.find(small_name);
    if (dirent.inode_n != INODE_INVALID)
    {
        return EDIR_FILE_EXISTS;
    }

    uint32_t inode_num = get_free_inode();
    auto inode = get_new_inode(file_type::dir, 0755);

    ret = write_inode(inode_num, &inode);

    if (ret < 0)
        return ret;

    inode_map_->set(true, inode_num);
    dir.add_entry(inode_num, small_name);
    return 0;
}

int file_system::rmdir(std::string & dir_name)
{
    if (dir_name.empty())
        return EDIR_INVALID_PATH;

    uint32_t dir_inode;
    auto ret = get_inode_by_path(dir_name, &dir_inode);
    if (ret < 0)
        return ret;


    // check if dir is empty or not
    directory dir = directory(dir_inode, this);
    auto dirent = dir.read();
    if (dirent.inode_n != INODE_INVALID)
    {
        return EDIR_NOT_EMPTY;
    }

    // todo: notify dirs that are opened that they are actually deleted
    return unlink(dir_name);
}

did_t file_system::opendir(std::string & dir_name)
{
    uint32_t dir_inode;
    auto ret = get_inode_by_path(dir_name, &dir_inode);
    if (ret < 0)
        return ret;
    auto dir = directory(dir_inode, this);
    dirs_.push_back(dir);
    return dirs_.size() - 1;
}

int file_system::closedir(did_t dir_id)
{
    if (dir_id > dirs_.size())
        return EDID_INVALID_ID;
    dirs_.erase(dirs_.begin() + dir_id);
    return 0;
}

dirent_t file_system::readdir(did_t dir_id)
{
    if (dir_id > dirs_.size())
        return DIRENT_INVALID;
    return dirs_[dir_id].read();
}

int file_system::rewind_dir(did_t dir_id)
{
    if (dir_id > dirs_.size())
        return EDID_INVALID_ID;
    dirs_[dir_id].rewind();
    return 0;
}

int file_system::get_inode_by_path(std::string & path, uint32_t * inode_out)
{
    if (path.length() == 0)
    {
        (*inode_out) = INODE_INVALID;
        return EDIR_INVALID_PATH;   
    }
    if (path == "/")
    {
        (*inode_out) = INODE_ROOT_ID;
        return 0;
    }

    auto tokens = split(path, '/');
    // TODO: relative path resolution
    // if first dir is not root (/)
    if (tokens[0].length() != 0)
    {
        (*inode_out) = INODE_INVALID;
        return EDIR_INVALID_PATH;   
    }

    directory curr_dir = directory(INODE_ROOT_ID, this);
    for (uint32_t i = 1; i < tokens.size() - 1; ++i)
    {
        auto dirent = curr_dir.find(tokens[i]);
        // if a dir does not exist
        if (dirent.inode_n == INODE_INVALID)
        {
            (*inode_out) = INODE_INVALID;
            return EDIR_INVALID_PATH;
        }
        // check if a file is not a dir
        try 
        {
            curr_dir.reopen(dirent.inode_n, this);
        }
        catch (std::string e)
        {
            (*inode_out) = INODE_INVALID;
            return EDIR_NOT_A_DIR;
        }
    }

    auto ret = curr_dir.find(tokens[tokens.size() - 1]);
    if (ret.inode_n == INODE_INVALID)
    {
        (*inode_out) = INODE_INVALID;
        return EDIR_FILE_NOT_FOUND;
    }

    (*inode_out) = ret.inode_n;
    return 0;
}


int file_system::read_block(uint32_t start_sector, char * buffer, std::size_t size)
{
    // TODO: CACHING
    return disk_.read_block(start_sector, buffer, size);
}

int file_system::write_block(uint32_t start_sector, const char * buffer, std::size_t size)
{
    // TODO: CACHING
    return disk_.write_block(start_sector, buffer, size);
}

int file_system::read_object(uint32_t start_sector, std::size_t offset, std::size_t obj_size, void * buffer)
{
    uint32_t curr_sector = start_sector;
    uint32_t block_size_bytes = super_block_.block_size * SECTOR_SIZE;
    int ret;
    
    if (offset + obj_size <= block_size_bytes)
    {
        ret = read_block(curr_sector, data_buffer_, super_block_.block_size);
        if (ret < 0)
            return ret;
        memcpy(buffer, data_buffer_ + offset, obj_size);
        return 0;
    }

    std::size_t obj_pos = 0;
    std::size_t copy_size;
    while (obj_pos < obj_size)
    {
        ret = read_block(curr_sector, data_buffer_, super_block_.block_size);
        if (ret < 0)
            return ret;

        copy_size = ((obj_size - obj_pos < block_size_bytes - offset) ? obj_size - obj_pos : block_size_bytes - offset); 
        memcpy(reinterpret_cast<char *>(buffer) + obj_pos, data_buffer_ + offset, copy_size);
        obj_pos += copy_size;
        offset = 0;
        curr_sector += super_block_.block_size;
    }
    return 0;
}

int file_system::write_object(uint32_t start_sector, std::size_t offset, std::size_t obj_size, const void * buffer)
{
    uint32_t curr_sector = start_sector;
    uint32_t block_size_bytes = super_block_.block_size * SECTOR_SIZE;
    int ret;
    
    if (offset + obj_size <= block_size_bytes)
    {
        ret = read_block(curr_sector, data_buffer_, super_block_.block_size);
        if (ret < 0)
            return ret;
        memcpy(data_buffer_ + offset, buffer, obj_size);
        ret = write_block(curr_sector, data_buffer_, super_block_.block_size);
        return 0;
    }

    std::size_t obj_pos = 0;
    std::size_t copy_size;
    while (obj_pos < obj_size)
    {
        copy_size = ((obj_size - obj_pos < block_size_bytes - offset) ? obj_size - obj_pos : block_size_bytes - offset); 

        if (copy_size == block_size_bytes)
        {
            ret = write_block(curr_sector, reinterpret_cast<const char *>(buffer) + obj_pos, super_block_.block_size);

            if (ret < 0)
                return ret;

            obj_pos += copy_size;
            curr_sector += super_block_.block_size;
            continue;
        }

        ret = read_block(curr_sector, data_buffer_, super_block_.block_size);
        if (ret < 0)
            return ret;
        
        memcpy(data_buffer_ + offset, reinterpret_cast<const char *>(buffer) + obj_pos, copy_size);
        obj_pos += copy_size;
        offset = 0;
        curr_sector += super_block_.block_size;
    }
    return 0;
}

int file_system::write_inode(uint32_t inode_id, const inode_t * inode)
{
    uint32_t sector = super_block_.inode_first_sector + (inode_id * sizeof(inode_t) / SECTOR_SIZE);
    return write_object(sector, (inode_id * sizeof(inode_t)) % SECTOR_SIZE, sizeof(inode_t), &inode);
}

int file_system::read_inode(uint32_t inode_id, inode_t * inode)
{
    uint32_t sector = super_block_.inode_first_sector + (inode_id * sizeof(inode_t) / SECTOR_SIZE);
    return read_object(sector, (inode_id * sizeof(inode_t)) % SECTOR_SIZE, sizeof(inode_t), &inode);
}

inode_t file_system::get_new_inode(file_type f_type, uint16_t permissions)
{
    auto curr_time = time(NULL);

    inode_t inode;
    inode.permissions = permissions;
    inode.f_type = f_type;
    inode.access_time = curr_time;
    inode.modify_time = curr_time;
    inode.change_time = curr_time;
    inode.links_count = 1;

    return inode;
}

uint32_t file_system::get_free_inode()
{
    uint32_t bytes = inode_map_->get_bytes_count();
    for (uint32_t i = 0; i < bytes; ++i)
    {
        if (inode_map_->bits_arr[i] != 0xFF)
        {
            auto start_bit = i << 3;
            for (uint32_t j = start_bit; j < start_bit + 8; ++j)
            {
                if (!inode_map_->get(j))
                    return j;
            }
        }
    }
    return -1;
}

super_block_t file_system::get_super_block()
{
    return this->super_block_;
}

uint32_t file_system::get_free_block()
{
    uint32_t bytes = space_map_->get_bytes_count();
    for (uint32_t i = 0; i < bytes; ++i)
    {
        if (space_map_->bits_arr[i] != 0xFF)
        {
            auto start_bit = i << 3;
            for (uint32_t j = start_bit; j < start_bit + 8; ++j)
            {
                if (!space_map_->get(j))
                    return j;
            }
        }
    }
    return -1;
}

void file_system::set_block_status(uint32_t block_id, bool is_busy)
{
    // TODO: DIRTY FLAG
    space_map_->set(is_busy, block_id);
}

void file_system::set_inode_status(uint32_t inode_num, bool is_busy)
{
    // TODO: DIRTY FLAG
    inode_map_->set(is_busy, inode_num);
}