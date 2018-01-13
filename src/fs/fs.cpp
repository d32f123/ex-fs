#include "fs.h"

#include <iostream>
#include <vector>
#include <ctime>
#include <cstring>
#include <limits>
#include <tuple>

#include "../inode/inode.h"
#include "../spacemap/spacemap.h"
#include "../superblock/superblock.h"
#include "../errors.h"
#include "../disk/disk.h"

std::vector<std::string> split(const std::string& text, const char sep)
{
	std::vector<std::string> tokens;
	std::size_t start = 0, end;
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

int file_system::load(const std::string& disk_file)
{
	if (this->disk_.is_open())
		this->unload();

	auto ret = this->disk_.load(disk_file);
	if (ret < 0)
		return ret;

	// read the superblock
	ret = this->disk_.read_block(0, reinterpret_cast<char *>(&super_block_), 1);
	if (ret < 0)
		return ret;

	// init data buffer
	this->data_buffer_ = new char[DATABUFFER_SIZE * super_block_.block_size * SECTOR_SIZE];

	// init inode map
	this->inode_map_ = new space_map(super_block_.inodes_count);
	read_object(super_block_.inodemap_first_block, 0, inode_map_->get_bytes_count(), inode_map_->bits_arr);

	// init space map
	this->space_map_ = new space_map(super_block_.blocks_count);
	read_object(super_block_.spacemap_first_block, 0, space_map_->get_bytes_count(), space_map_->bits_arr);

	std::cout << super_block_;

	// init cwd
	cwd_ = directory(INODE_ROOT_ID, this);

	return 0;
}

void file_system::unload()
{
	sync();

	cache_.clear();

	delete[] data_buffer_;
	data_buffer_ = nullptr;

	delete this->space_map_;
	this->space_map_ = nullptr;

	delete this->inode_map_;
	this->inode_map_ = nullptr;

	this->disk_.unload();
}

int file_system::sync()
{
	int ret;
	if (sb_dirty_)
	{
		ret = this->disk_.write_block(SUPERBLOCK_SECT, reinterpret_cast<char *>(&super_block_), 1);
		if (ret < 0)
			return ret;
		sb_dirty_ = false;
	}
	if (im_dirty_)
	{
		ret = write_object(super_block_.inodemap_first_block, 0, inode_map_->get_bytes_count(), inode_map_->bits_arr);
		if (ret < 0)
			return ret;
		im_dirty_ = false;
	}
	if (sm_dirty_)
	{
		ret = write_object(super_block_.spacemap_first_block, 0, space_map_->get_bytes_count(), space_map_->bits_arr);
		if (ret < 0)
			return ret;
		sm_dirty_ = false;
	}
	return 0;
}

void file_system::trace()
{
	using std::cout;
	using std::endl;
	for (uint32_t i = 0; i < inode_map_->get_bits_count(); ++i)
	{
		const auto occupied = inode_map_->get(i);
		cout << "Inode " << i << ": " << (!occupied ? "free" : "occupied") << endl;
		if (occupied)
		{
			inode_t ind;
			read_inode(i, &ind);
			cout << ind;
		}
		cout << endl << "-------------------------" << endl;
	}
	cout << "Inode map:\n";
	cout << *inode_map_ << endl;
	cout << "Space map:\n";
	cout << *space_map_ << endl;
	cout << std::dec;
}

void file_system::traceblock(uint32_t block)
{
	using std::cout;
	using std::setw;
	using std::setfill;
	using std::endl;
	using std::dec;

	read_data_block(block, data_buffer_, 1);
	// DATABUFFER_SIZE * super_block_.block_size * SECTOR_SIZE
	for (uint32_t i = 0; i < 64; ++i)
	{
		if (i % 16 == 0)
			cout << endl << setw(4) << setfill('0') << std::hex << static_cast<int>(i << 4) << dec << ": ";
		cout << hex(data_buffer_[i]);
	}
	cout << std::dec;
	cout << endl;
}

file_system::file_system(const file_system& that) : super_block_(that.super_block_)
{
	disk_ = that.disk_;

	sb_dirty_ = that.sb_dirty_;
	im_dirty_ = that.im_dirty_;
	sm_dirty_ = that.sm_dirty_;

	cache_ = that.cache_;

	files_ = that.files_;
	dirs_ = that.dirs_;

	cwd_ = that.cwd_;

	data_buffer_ = new char[DATABUFFER_SIZE * super_block_.block_size * SECTOR_SIZE];
	memcpy(data_buffer_, that.data_buffer_, DATABUFFER_SIZE * super_block_.block_size * SECTOR_SIZE);

	inode_map_ = new space_map(*that.inode_map_);
	space_map_ = new space_map(*that.space_map_);
}

file_system::file_system(file_system&& that) noexcept : super_block_(that.super_block_)
{
	disk_ = std::move(that.disk_);

	sb_dirty_ = that.sb_dirty_;
	that.sb_dirty_ = false;
	im_dirty_ = that.im_dirty_;
	that.im_dirty_ = false;
	sm_dirty_ = that.sm_dirty_;
	that.sm_dirty_ = false;

	cache_ = std::move(that.cache_);

	files_ = std::move(that.files_);
	dirs_ = std::move(that.dirs_);

	cwd_ = std::move(that.cwd_);

	data_buffer_ = that.data_buffer_;
	that.data_buffer_ = nullptr;

	inode_map_ = that.inode_map_;
	that.inode_map_ = nullptr;
	space_map_ = that.space_map_;
	that.space_map_ = nullptr;
}

file_system::~file_system()
{
	sync();
	delete[] data_buffer_;
}

file_system& file_system::operator=(const file_system& that)
{
	if (this == &that) return *this;

	disk_ = that.disk_;

	sb_dirty_ = that.sb_dirty_;
	im_dirty_ = that.im_dirty_;
	sm_dirty_ = that.sm_dirty_;

	cache_ = that.cache_;

	files_ = that.files_;
	dirs_ = that.dirs_;

	cwd_ = that.cwd_;

	data_buffer_ = new char[DATABUFFER_SIZE * super_block_.block_size * SECTOR_SIZE];
	memcpy(data_buffer_, that.data_buffer_, DATABUFFER_SIZE * super_block_.block_size * SECTOR_SIZE);

	inode_map_ = new space_map(*that.inode_map_);
	space_map_ = new space_map(*that.space_map_);

	return *this;
}

file_system& file_system::operator=(file_system&& that) noexcept
{
	if (this == &that) return *this;

	disk_ = std::move(that.disk_);

	sb_dirty_ = that.sb_dirty_;
	that.sb_dirty_ = false;
	im_dirty_ = that.im_dirty_;
	that.im_dirty_ = false;
	sm_dirty_ = that.sm_dirty_;
	that.sm_dirty_ = false;

	cache_ = std::move(that.cache_);

	files_ = std::move(that.files_);
	dirs_ = std::move(that.dirs_);

	cwd_ = std::move(that.cwd_);

	data_buffer_ = that.data_buffer_;
	that.data_buffer_ = nullptr;

	inode_map_ = that.inode_map_;
	that.inode_map_ = nullptr;
	space_map_ = that.space_map_;
	that.space_map_ = nullptr;

	return *this;
}

int file_system::init(const std::string& disk_file, const uint32_t inodes_count,
                      std::size_t disk_size, const uint32_t block_size)
{
	if (this->disk_.is_open())
		this->unload();

	auto ret = this->disk_.create(disk_file, disk_size);
	if (ret < 0)
		return ret;

	// init the super_block struct
	// total blocks we have = total disk size (in sectors)
	//                      -1 sector for superblock
	//                      - sectors required for inode map
	//                      - sectors required for inodes
	const auto inodes_size = bytes_to_blocks(sizeof(inode_t) * inodes_count, block_size);

	const auto inodemap_size = bits_to_blocks(inodes_count, block_size);

	const auto blocks_count = (disk_size - 1 - (inodes_size * block_size) - (inodemap_size * block_size)) / block_size;

	const auto spacemap_size = bits_to_blocks(blocks_count, block_size);

	super_block_t sb;
	sb.inodes_count = inodes_count;
	sb.inodes_free = inodes_count;
	sb.inode_size = sizeof(inode_t);
	sb.blocks_count = blocks_count;
	sb.blocks_free = blocks_count;
	sb.block_size = block_size;
	sb.block_offset = 1;
	sb.inodemap_first_block = 0;
	sb.inodemap_size = inodemap_size;
	sb.inode_first_block = inodemap_size;
	sb.spacemap_first_block = sb.inode_first_block + inodes_size;
	sb.data_first_block = sb.spacemap_first_block;
	sb.inodes_size = inodes_size;
	sb.spacemap_size = spacemap_size;
	sb.magic = 0xBEEF;

	this->super_block_ = sb;

	std::cout << super_block_;

	this->disk_.write_block(0, reinterpret_cast<char *>(&this->super_block_), 1);

	// init data buffer
	this->data_buffer_ = new char[DATABUFFER_SIZE * super_block_.block_size * SECTOR_SIZE];

	// creating inode map

	this->inode_map_ = new space_map(inodes_count);

	inode_map_->set(true, INODE_ROOT_ID);

	this->write_object(sb.inodemap_first_block, 0, inode_map_->get_bytes_count(), inode_map_->bits_arr);

	// create root inode
	const auto curr_time = time(nullptr);
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

	this->space_map_ = new space_map(blocks_count);
	for (uint32_t i = 0; i < spacemap_size; ++i)
		this->space_map_->set(true, i);

	this->write_object(sb.spacemap_first_block, 0, space_map_->get_bytes_count(), space_map_->bits_arr);

	// init cwd
	cwd_ = directory(INODE_ROOT_ID, this);
	ret = cwd_.add_entry(INODE_ROOT_ID, ".");
	if (ret < 0)
		return ret;
	ret = cwd_.add_entry(INODE_ROOT_ID, "..");
	if (ret < 0)
		return ret;

	return 0;
}

int file_system::create(const std::string& file_name)
{
	return do_create(file_name, file_type::regular);
}

int file_system::link(const std::string& original_file, const std::string& new_file)
{
	if (original_file.empty() || new_file.empty())
		return EDIR_INVALID_PATH;

	uint32_t orig_file_inode;
	auto ret = get_inode_by_path(original_file, &orig_file_inode);
	if (ret < 0)
		return ret;

	// set up the new file
	uint32_t last_dir_inode;
	auto vec = get_dir_and_file(new_file);
	const auto dir_path = vec[0];
	const auto small_name = vec[1];
	ret = get_inode_by_path(dir_path, &last_dir_inode);
	if (ret < 0)
		return ret;

	directory dir = directory(last_dir_inode, this);

	// check if file exists
	const auto dirent = dir.find(small_name);
	if (dirent.inode_n != INVALID_INODE)
	{
		return EDIR_FILE_EXISTS;
	}

	// up the counter
	inode_t orig_file;
	ret = read_inode(orig_file_inode, &orig_file);
	if (ret < 0)
		return ret;

	const auto curr_time = time(nullptr);
	orig_file.access_time = curr_time;
	orig_file.change_time = curr_time;
	++orig_file.links_count;

	ret = write_inode(orig_file_inode, &orig_file);
	if (ret < 0)
		return ret;

	// add entry
	ret = dir.add_entry(orig_file_inode, small_name);
	if (ret < 0)
		return ret;
	return 0;
}

int file_system::unlink(const std::string& file_name)
{
	return do_unlink(file_name, false);
}

fid_t file_system::open(const std::string& disk_file)
{
	uint32_t inode;
	const auto ret = get_inode_by_path(disk_file, &inode);
	if (ret < 0)
		return ret;
	const auto fid = files_.insert(file(inode, this));
	if (fid == std::numeric_limits<std::size_t>::max())
		return INVALID_FID;
	return fid;
}

int file_system::close(fid_t fid) const
{
	try
	{
		files_.remove(fid);
		return 0;
	}
	catch (std::exception&)
	{
		return EFID_INVALID_ID;
	}
}

int file_system::read(fid_t fid, char* buffer, std::size_t size)
{
	try
	{
		return files_[fid].read(buffer, size);
	}
	catch (std::exception&)
	{
		return EFID_INVALID_ID;
	}
}

int file_system::write(fid_t fid, const char* buffer, std::size_t size)
{
	try
	{
		return files_[fid].write(buffer, size);
	}
	catch (std::exception&)
	{
		return EFID_INVALID_ID;
	}
}

int file_system::seek(fid_t fid, std::size_t pos)
{
	try
	{
		return files_[fid].seek(pos);
	}
	catch (std::exception&)
	{
		return EFID_INVALID_ID;
	}
}

int file_system::trunc(fid_t fid, std::size_t new_length)
{
	try
	{
		return files_[fid].trunc(new_length);
	}
	catch (std::exception&)
	{
		return EFID_INVALID_ID;
	}
}

int file_system::cd(const std::string& new_dir)
{
	if (new_dir.empty())
		return EDIR_INVALID_PATH;
	uint32_t new_inode_n;

	const auto ret = get_inode_by_path(new_dir, &new_inode_n);
	if (ret < 0)
		return ret;

	cwd_ = directory(new_inode_n, this);
	return 0;
}

int file_system::mkdir(const std::string& dir_name)
{
	uint32_t prev_dir_inode;
	uint32_t inode_out;
	auto ret = do_create(dir_name, file_type::dir, &inode_out, &prev_dir_inode);
	if (ret < 0)
		return ret;
	auto dir = directory(inode_out, this);
	ret = dir.add_entry(inode_out, ".");
	if (ret < 0)
	{
		do_unlink(dir_name, true);
		return ret;
	}
	ret = dir.add_entry(prev_dir_inode, "..");
	if (ret < 0)
	{
		do_unlink(dir_name, true);
		return ret;
	}
	return 0;
}

int file_system::rmdir(const std::string& dir_name)
{
	if (dir_name.empty())
		return EDIR_INVALID_PATH;

	uint32_t dir_inode;
	const auto ret = get_inode_by_path(dir_name, &dir_inode);
	if (ret < 0)
		return ret;

	if (dir_inode == INODE_ROOT_ID)
		return EDIR_INVALID_PATH;

	// check if dir is empty or not
	auto dir = directory(dir_inode, this);
	dir.read(); // . dir
	dir.read(); // .. dir
	const auto dirent = dir.read(); // should be invalid 
	if (dirent.inode_n != INVALID_INODE)
	{
		return EDIR_NOT_EMPTY;
	}

	// todo: notify dirs that are opened that they are actually deleted
	return do_unlink(dir_name, true);
}

did_t file_system::opendir(const std::string& dir_name)
{
	uint32_t dir_inode;
	const auto ret = get_inode_by_path(dir_name, &dir_inode);
	if (ret < 0)
		return ret;
	const auto index = dirs_.insert(directory(dir_inode, this));
	if (index == std::numeric_limits<std::size_t>::max())
		return INVALID_FID;
	return index;
}

int file_system::closedir(did_t dir_id) const
{
	try
	{
		dirs_.remove(dir_id);
		return 0;
	}
	catch (std::exception&)
	{
		return EDID_INVALID_ID;
	}
}

dirent_t file_system::readdir(did_t dir_id)
{
	try
	{
		return dirs_[dir_id].read();
	}
	catch (std::exception&)
	{
		return INVALID_DIRENT;
	}
}

int file_system::rewind_dir(did_t dir_id)
{
	try
	{
		dirs_[dir_id].rewind();
		return 0;
	}
	catch (std::exception&)
	{
		return EDID_INVALID_ID;
	}
}

std::string file_system::concat_paths(const std::string& path1, const std::string& path2)
{
	if (path1.empty())
		return path2;
	if (path2.empty())
		return path1;
	if (path1[path1.length() - 1] == '/' && path2[0] == '/')
		return path1 + path2.substr(1);
	if (path1[path1.length() - 1] == '/' || path2[0] == '/')
		return path1 + path2;
	return path1 + "/" + path2;
}

int file_system::get_inode_by_path(const std::string& path, uint32_t* inode_out)
{
	if (path == "/")
	{
		(*inode_out) = INODE_ROOT_ID;
		return 0;
	}
	if (path.length() == 0)
	{
		(*inode_out) = cwd_.get_file()->get_inode_n();
		return 0;
	}

	auto tokens = split(path, '/');
	directory curr_dir;

	if (tokens[0].length() != 0)
		curr_dir = cwd_;
	else
		curr_dir = directory(INODE_ROOT_ID, this);

	uint32_t i = (tokens[0].length() != 0 ? 0 : 1);
	for (; i < tokens.size() - 1; ++i)
	{
		const auto dirent = curr_dir.find(tokens[i]);
		// if a dir does not exist
		if (dirent.inode_n == INVALID_INODE)
		{
			(*inode_out) = INVALID_INODE;
			return EDIR_INVALID_PATH;
		}
		// check if a file is not a dir
		try
		{
			curr_dir.reopen(dirent.inode_n, this);
		}
		catch (std::string&)
		{
			(*inode_out) = INVALID_INODE;
			return EDIR_NOT_A_DIR;
		}
	}

	if (tokens[tokens.size() - 1].length() != 0)
	{
		const auto ret = curr_dir.find(tokens[tokens.size() - 1]);
		if (ret.inode_n == INVALID_INODE)
		{
			(*inode_out) = INVALID_INODE;
			return EDIR_FILE_NOT_FOUND;
		}

		(*inode_out) = ret.inode_n;
	}
	else
		(*inode_out) = curr_dir.get_file()->get_inode_n();
	return 0;
}

int file_system::do_unlink(const std::string& file_name, bool force)
{
	if (file_name.empty())
		return EDIR_INVALID_PATH;

	uint32_t file_inode;
	uint32_t last_dir_inode;
	auto vec = get_dir_and_file(file_name);
	const auto dir_path = vec[0];
	const auto small_name = vec[1];
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

	if (!force && inode.f_type == file_type::dir)
		return EFIL_WRONG_TYPE;

	const auto curr_time = time(nullptr);
	inode.access_time = curr_time;
	inode.change_time = curr_time;
	--inode.links_count;

	// check if links count is 0 (or somehow less than 0)
	if (inode.links_count == 0)
	{
		if (inode.links_count > super_block_.inodes_count)
		{
			std::cerr << "inode " << file_inode << " has link count: " << inode.links_count << std::endl;
		}
		// clear the space
		file tmp = file(file_inode, this);
		tmp.trunc(0);
		// clear the inode
		set_inode_status(file_inode, false);
	}
	else
	{
		ret = write_inode(file_inode, &inode);
		if (ret < 0)
			return ret;
	}
	// remove file from directory
	ret = dir.remove_entry(small_name);
	if (ret < 0)
		return ret;

	return 0;
}

int file_system::do_create(const std::string& file_name, file_type f_type,
                           uint32_t* inode_out, uint32_t* prev_dir_inode_out)
{
	auto vec = get_dir_and_file(file_name);
	const auto dir_path = vec[0];
	const auto small_name = vec[1];

	uint32_t last_dir_inode;
	auto ret = get_inode_by_path(dir_path, &last_dir_inode);
	if (ret < 0)
		return ret;

	directory dir;
	try
	{
		dir = directory(last_dir_inode, this);
	}
	catch (std::exception&)
	{
		return EDIR_INVALID_PATH;
	}

	// check if file exists
	const auto dirent = dir.find(small_name);
	if (dirent.inode_n != INVALID_INODE)
	{
		return EDIR_FILE_EXISTS;
	}

	const auto inode_num = get_free_inode();
	if (inode_num == INVALID_INODE)
		return EIND_OUT_OF_INODES;
	auto inode = get_new_inode(f_type, 0755);

	ret = write_inode(inode_num, &inode);

	if (ret < 0)
		return ret;

	set_inode_status(inode_num, true);
	ret = dir.add_entry(inode_num, small_name);
	if (ret < 0)
		return ret;

	if (inode_out != nullptr)
		(*inode_out) = inode_num;
	if (prev_dir_inode_out != nullptr)
		(*prev_dir_inode_out) = dir.get_file()->get_inode_n();

	return 0;
}


/**
 * \brief reads a block of storage into memory
 * \param start_block starting block
 * \param buffer buffer to read info into
 * \param size size in blocks
 * \return error code
 */
int file_system::read_block(uint32_t start_block, char* buffer, const std::size_t size)
{
	const auto block_bytes = super_block_.block_size * SECTOR_SIZE;
	std::tuple<uint32_t, std::size_t> curr_read;
	int ret;

	//std::cout << "r:" << start_block << ":" << size << std::endl;

	// check through all the blocks and see if any of those are in cache
	auto prev_push = static_cast<std::size_t>(-1);
	for (std::size_t i = 0; i < size; ++i)
	{
		const auto offset = i * block_bytes;
		if (cache_.contains(start_block + i))
		{
			// check if we need to read from mem
			if (prev_push != static_cast<std::size_t>(-1))
			{
				const auto b_start = std::get<0>(curr_read);
				const auto b_size = std::get<1>(curr_read);

				const auto sector = super_block_.block_offset + b_start * super_block_.block_size;
				const auto mem_offset = (b_start - start_block) * block_bytes;

				ret = disk_.read_block(sector, buffer + mem_offset, b_size);
				if (ret < 0)
					return ret;
				// place it in cache
				for (std::size_t j = 0; j < b_size; ++j)
				{
					auto vec = std::vector<char>(block_bytes);
					memcpy(vec.data(), buffer + (b_start - start_block + j) * block_bytes, block_bytes);
					cache_.insert(b_start + j, vec);
				}

				prev_push = static_cast<std::size_t>(-1);
			}
			// if in cache, just copy it straight inwards
			memcpy(buffer + offset, cache_.get(start_block + i).data(), block_bytes);
		}
		else
		{
			if (i != 0 && i == prev_push + 1)
				std::get<1>(curr_read)++;
			else
				curr_read = std::make_tuple(start_block + i, 1);
			prev_push = i;
		}
	}


	// check if we need to read from mem
	if (prev_push != static_cast<std::size_t>(-1))
	{
		const auto b_start = std::get<0>(curr_read);
		const auto b_size = std::get<1>(curr_read);

		const auto sector = super_block_.block_offset + b_start * super_block_.block_size;
		const auto mem_offset = (b_start - start_block) * block_bytes;

		ret = disk_.read_block(sector, buffer + mem_offset, b_size * super_block_.block_size);
		if (ret < 0)
			return ret;
		// place it in cache
		for (std::size_t j = 0; j < b_size; ++j)
		{
			auto vec = std::vector<char>(block_bytes);
			memcpy(vec.data(), buffer + (b_start - start_block + j) * block_bytes, block_bytes);
			cache_.insert(b_start + j, vec);
		}
	}

	disk_.read_block(super_block_.block_offset + start_block * super_block_.block_size, buffer,
	                 size * super_block_.block_size);
	return size * block_bytes;
}

/**
* \brief writes a block of memopry into storage
* \param start_block starting block
* \param buffer buffer to write info from
* \param size size in blocks
* \return error code
*/
int file_system::write_block(uint32_t start_block, const char* buffer, std::size_t size)
{
	const auto block_bytes = super_block_.block_size * SECTOR_SIZE;

	//std::cout << "w:" << start_block << ":" << size << std::endl;

	for (std::size_t i = 0; i < size; ++i)
	{
		auto vec = std::vector<char>(block_bytes);
		memcpy(vec.data(), buffer + i * block_bytes, block_bytes);
		cache_.insert(start_block + i, vec);
	}

	return disk_.write_block(super_block_.block_offset + start_block * super_block_.block_size,
	                         buffer, size * super_block_.block_size);
}

int file_system::read_data_block(uint32_t start_block, char* buffer, std::size_t size)
{
	return read_block(super_block_.data_first_block + start_block, buffer, size);
}

int file_system::write_data_block(uint32_t start_block, const char* buffer, std::size_t size)
{
	return write_block(super_block_.data_first_block + start_block, buffer, size);
}

int file_system::read_object(uint32_t start_block, std::size_t offset, std::size_t obj_size, void* buffer)
{
	auto curr_block = start_block;
	const auto block_size_bytes = super_block_.block_size * SECTOR_SIZE;
	int ret;

	if (offset + obj_size <= (block_size_bytes * DATABUFFER_SIZE))
	{
		ret = read_block(curr_block, data_buffer_, DATABUFFER_SIZE);
		if (ret < 0)
			return ret;
		memcpy(buffer, data_buffer_ + offset, obj_size);
		return 0;
	}

	std::size_t obj_pos = 0;
	while (obj_pos < obj_size)
	{
		ret = read_block(curr_block, data_buffer_, DATABUFFER_SIZE);
		if (ret < 0)
			return ret;

		const auto copy_size = ((obj_size - obj_pos < block_size_bytes * DATABUFFER_SIZE - offset)
			                        ? obj_size - obj_pos
			                        : block_size_bytes * DATABUFFER_SIZE - offset);
		memcpy(reinterpret_cast<char *>(buffer) + obj_pos, data_buffer_ + offset, copy_size);
		obj_pos += copy_size;
		offset = 0;
		curr_block += DATABUFFER_SIZE;
	}
	return 0;
}

int file_system::write_object(const uint32_t start_block, std::size_t offset, const std::size_t obj_size,
                              const void* buffer)
{
	auto curr_block = start_block;
	const auto block_size_bytes = super_block_.block_size * SECTOR_SIZE;
	int ret;

	if (offset + obj_size <= block_size_bytes * DATABUFFER_SIZE)
	{
		ret = read_block(curr_block, data_buffer_, DATABUFFER_SIZE);
		if (ret < 0)
			return ret;
		memcpy(data_buffer_ + offset, buffer, obj_size);
		write_block(curr_block, data_buffer_, DATABUFFER_SIZE);
		return obj_size;
	}

	std::size_t obj_pos = 0;
	while (obj_pos < obj_size)
	{
		const auto copy_size = ((obj_size - obj_pos < block_size_bytes * DATABUFFER_SIZE - offset)
			                        ? obj_size - obj_pos
			                        : block_size_bytes * DATABUFFER_SIZE - offset);

		// if copy size is adjacent with block size, we dont have to read anything
		if ((copy_size % (block_size_bytes * DATABUFFER_SIZE)) == 0)
		{
			ret = write_block(curr_block, reinterpret_cast<const char *>(buffer) + obj_pos, copy_size / block_size_bytes);

			if (ret < 0)
				return ret;

			obj_pos += copy_size;
			curr_block += copy_size / block_size_bytes;
			continue;
		}

		ret = read_block(curr_block, data_buffer_, DATABUFFER_SIZE);
		if (ret < 0)
			return ret;

		memcpy(data_buffer_ + offset, reinterpret_cast<const char *>(buffer) + obj_pos, copy_size);

		ret = write_block(curr_block, data_buffer_, DATABUFFER_SIZE);
		if (ret < 0)
			return ret;

		obj_pos += copy_size;
		offset = 0;
		curr_block += DATABUFFER_SIZE;
	}
	return obj_size;
}

int file_system::read_data_object(uint32_t start_block, std::size_t offset, std::size_t obj_size, void* buffer)
{
	return read_object(super_block_.data_first_block + start_block, offset, obj_size, buffer);
}

int file_system::write_data_object(uint32_t start_block, std::size_t offset, std::size_t obj_size, const void* buffer)
{
	return write_object(super_block_.data_first_block + start_block, offset, obj_size, buffer);
}

int file_system::write_inode(uint32_t inode_id, const inode_t* inode)
{
	const auto t = time(nullptr);
	inode->access_time = t;
	inode->change_time = t;

	const auto block = super_block_.inode_first_block + inode_id * sizeof(inode_t) / (super_block_.block_size * SECTOR_SIZE
	);
	return write_object(block, (inode_id * sizeof(inode_t)) % (super_block_.block_size * SECTOR_SIZE), sizeof(inode_t),
	                    inode);
}

int file_system::read_inode(uint32_t inode_id, inode_t* inode)
{
	if (!inode_map_->get(inode_id))
	{
		return EIND_INVALID_INODE;
	}
	const auto sector = super_block_.inode_first_block + inode_id * sizeof(inode_t) / (super_block_.block_size *
		SECTOR_SIZE);
	const auto ret = read_object(sector, (inode_id * sizeof(inode_t)) % (super_block_.block_size * SECTOR_SIZE), sizeof(inode_t),
	                   inode);
	if (ret < 0)
		return ret;
	inode->access_time = time(nullptr);
	return ret;
}

inode_t file_system::get_new_inode(const file_type f_type, const uint16_t permissions)
{
	const auto curr_time = time(nullptr);

	inode_t inode;
	inode.permissions = permissions;
	inode.f_type = f_type;
	inode.access_time = curr_time;
	inode.modify_time = curr_time;
	inode.change_time = curr_time;
	inode.links_count = 1;

	return inode;
}

uint32_t file_system::get_free_inode() const
{
	const auto ret = inode_map_->find_first_of(false);
	if (ret == std::numeric_limits<std::size_t>::max())
		return INVALID_INODE;
	return static_cast<uint32_t>(ret);
}

super_block_t file_system::get_super_block() const
{
	return this->super_block_;
}

uint32_t file_system::get_free_block() const
{
	const auto ret = space_map_->find_first_of(false);
	if (ret == std::numeric_limits<std::size_t>::max())
		return INVALID_BLOCK;
	return static_cast<uint32_t>(ret);
}

void file_system::set_block_status(uint32_t block_id, bool is_busy)
{
	if (is_busy)
		--super_block_.blocks_free;
	else
		++super_block_.blocks_free;
	sb_dirty_ = true;

	space_map_->set(is_busy, block_id);
	sm_dirty_ = true;
}

void file_system::set_inode_status(uint32_t inode_num, bool is_busy)
{
	if (is_busy)
		--super_block_.inodes_free;
	else
		++super_block_.inodes_free;
	sb_dirty_ = true;

	inode_map_->set(is_busy, inode_num);
	im_dirty_ = true;
}

std::vector<std::string> file_system::get_dir_and_file(const std::string& file_name)
{
	if (file_name.empty())
		return {"", ""};

	const auto last_slash = file_name.find_last_of('/');

	std::string dir_path;
	std::string small_name;

	if (last_slash != std::string::npos)
	{
		dir_path = file_name.substr(0, last_slash + 1);
		small_name = file_name.substr(last_slash + 1, std::string::npos);
	}
	else
	{
		dir_path = "";
		small_name = file_name;
	}

	return {dir_path, small_name};
}
