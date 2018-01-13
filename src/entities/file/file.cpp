#include "file.h"

#include "../../fs/fs.h"
#include "../../errors.h"

#include <cstring>

file::file(const std::string& filename, file_system* fs)
{
	uint32_t inode_n;
	const auto ret = fs->get_inode_by_path(filename, &inode_n);

	if (ret < 0)
		throw std::exception();

	fs_ = fs;
	inode_n_ = inode_n;
	curr_pos_ = 0;

	fs->read_inode(inode_n, &inode_);
}

file::file(const uint32_t inode_n, file_system* fs) : fs_(fs), inode_n_(inode_n)
{
	fs->read_inode(inode_n, &inode_);
}

void file::reopen(const uint32_t inode_n, file_system* file_sys)
{
	fs_ = file_sys;
	curr_pos_ = 0;
	inode_n_ = inode_n;
	fs_->read_inode(inode_n, &inode_);
}

int file::get_inode(inode_t* inode_out) const
{
	return fs_->read_inode(inode_n_, inode_out);
}

int file::read(char* buffer, std::size_t size)
{
	const auto block_size_bytes = fs_->super_block_.block_size * SECTOR_SIZE;
	const auto curr_block = curr_pos_ / block_size_bytes;
	const auto curr_block_offset = curr_pos_ % block_size_bytes;

	const auto ret = read_unaligned(curr_block, curr_block_offset, size, buffer);

	if (ret < 0)
		return ret;

	return curr_pos_ += size;
}

int file::write(const char* buffer, std::size_t size)
{
	const auto block_size_bytes = fs_->super_block_.block_size * SECTOR_SIZE;
	const auto curr_block = curr_pos_ / block_size_bytes;
	const auto curr_block_offset = curr_pos_ % block_size_bytes;

	const auto ret = write_unaligned(curr_block, curr_block_offset, size, buffer);

	if (ret < 0)
		return ret;

	fs_->read_inode(inode_n_, &inode_);
	inode_.modify_time = time(nullptr);
	fs_->write_inode(inode_n_, &inode_);

	return (curr_pos_ += size);
}

int file::trunc(std::size_t new_size)
{
	// TODO: ASSERT MACRO CHECK 

	const auto block_size_bytes = fs_->super_block_.block_size * SECTOR_SIZE;
	const auto free_blocks = new_size / block_size_bytes + (new_size % block_size_bytes != 0);

	for (auto i = free_blocks; i < INODE_BLOCKS_MAX; ++i)
	{
		if (inode_.blocks[i] != 0)
		{
			fs_->set_block_status(inode_.blocks[i], false);
			inode_.blocks[i] = 0;
		}
	}
	uint32_t i;
	if (inode_.indirect_block != 0)
	{
		uint32_t* buffer = reinterpret_cast<uint32_t *>(fs_->data_buffer_);
		fs_->read_data_block(inode_.indirect_block, fs_->data_buffer_, 1);

		if (free_blocks < INODE_BLOCKS_MAX)
			i = 0;
		else
			i = free_blocks - INODE_BLOCKS_MAX;

		for (; i < block_size_bytes / sizeof(uint32_t); ++i)
		{
			if (buffer[i] != 0)
			{
				fs_->set_block_status(buffer[i], false);
				buffer[i] = 0;
			}
		}
		fs_->write_data_block(inode_.indirect_block, fs_->data_buffer_, 1);
		if (free_blocks <= INODE_BLOCKS_MAX)
		{
			fs_->set_block_status(inode_.indirect_block, false);
			inode_.indirect_block = 0;
		}
	}
	if (inode_.double_indirect_block != 0)
	{
		uint32_t* buffer = reinterpret_cast<uint32_t *>(fs_->data_buffer_);
		uint32_t* second_buffer = new uint32_t[block_size_bytes / sizeof(uint32_t)];

		fs_->read_data_block(inode_.double_indirect_block, fs_->data_buffer_, 1);

		if (free_blocks < INODE_BLOCKS_MAX + (block_size_bytes / sizeof(uint32_t)))
			i = 0;
		else
			i = (free_blocks - INODE_BLOCKS_MAX - (block_size_bytes / sizeof(uint32_t))) / (block_size_bytes / sizeof(uint32_t));

		uint32_t j;
		if (i == 0)
			j = 0;
		else
			j = (free_blocks - INODE_BLOCKS_MAX - (block_size_bytes / sizeof(uint32_t))) % (block_size_bytes / sizeof(uint32_t));

		for (; i < block_size_bytes / sizeof(uint32_t); ++i)
		{
			if (buffer[i] != 0)
			{
				fs_->read_data_block(buffer[i], reinterpret_cast<char *>(second_buffer), 1);
				for (; j < block_size_bytes / sizeof(uint32_t); ++j)
				{
					if (second_buffer[j] != 0)
					{
						fs_->set_block_status(second_buffer[j], false);
						second_buffer[j] = 0;
					}
				}
				fs_->write_data_block(buffer[i], reinterpret_cast<char *>(second_buffer), 1);

				fs_->set_block_status(buffer[i], false);
				buffer[i] = 0;
				j = 0;
			}
		}

		fs_->set_block_status(inode_.double_indirect_block, false);
		inode_.double_indirect_block = 0;
		delete[] second_buffer;
	}
	fs_->write_inode(inode_n_, &inode_);
	if (curr_pos_ >= new_size)
		curr_pos_ = 0;

	fs_->read_inode(inode_n_, &inode_);
	inode_.modify_time = time(nullptr);
	fs_->write_inode(inode_n_, &inode_);

	return 0;
}

int file::seek(const std::size_t pos)
{
	return curr_pos_ = pos;
}

int file::read_unaligned(const uint32_t start_block, std::size_t offset, const std::size_t obj_size, void* buffer)
{
	const auto block_size_bytes = fs_->super_block_.block_size * SECTOR_SIZE;
	std::size_t obj_pos = 0;
	uint32_t i = start_block;
	uint32_t curr_block;

	while (obj_pos < obj_size)
	{
		auto ret = get_sector(i, &curr_block);
		if (ret < 0)
			return ret;
		if (curr_block == 0)
			return EFIL_INVALID_SECTOR;

		ret = fs_->read_data_block(curr_block, fs_->data_buffer_, 1);
		if (ret < 0)
			return ret;

		const auto copy_size = ((obj_size - obj_pos < block_size_bytes - offset)
			                        ? obj_size - obj_pos
			                        : block_size_bytes - offset);
		memcpy(reinterpret_cast<char *>(buffer) + obj_pos, fs_->data_buffer_ + offset, copy_size);
		obj_pos += copy_size;
		offset = 0;
		++i;
	}
	return obj_size;
}

int file::write_unaligned(const uint32_t start_block, std::size_t offset, const std::size_t obj_size,
                          const void* buffer)
{
	const auto block_size_bytes = fs_->super_block_.block_size * SECTOR_SIZE;
	std::size_t obj_pos = 0;
	uint32_t i = start_block;
	uint32_t curr_block;

	while (obj_pos < obj_size)
	{
		auto ret = get_sector(i, &curr_block, true);
		if (ret < 0)
			return ret;
		if (curr_block == 0)
			return EFIL_INVALID_SECTOR;

		const auto copy_size = ((obj_size - obj_pos < block_size_bytes - offset)
			                        ? obj_size - obj_pos
			                        : block_size_bytes - offset);

		if (copy_size == block_size_bytes)
		{
			ret = fs_->write_data_block(curr_block, reinterpret_cast<const char *>(buffer) + obj_pos, 1);

			if (ret < 0)
				return ret;

			obj_pos += copy_size;
			++i;
			continue;
		}

		ret = fs_->read_data_block(curr_block, fs_->data_buffer_, 1);
		if (ret < 0)
			return ret;

		memcpy(fs_->data_buffer_ + offset, reinterpret_cast<const char *>(buffer) + obj_pos, copy_size);
		fs_->write_data_block(curr_block, fs_->data_buffer_, 1);
		obj_pos += copy_size;
		offset = 0;
		++i;
	}
	return 0;
}

int file::get_sector(const uint32_t i, uint32_t* sector_out, const bool do_allocate)
{
	int ret;
	const auto block_size_bytes = fs_->super_block_.block_size * SECTOR_SIZE;
	const auto indirect_max = block_size_bytes / sizeof(uint32_t);
	const auto double_indirect_max = indirect_max * indirect_max;

	if (do_allocate)
		if ((ret = allocate_block(i)) < 0)
			return ret;

	fs_->read_inode(inode_n_, &inode_);
	// direct
	if (i < INODE_BLOCKS_MAX)
	{
		(*sector_out) = inode_.blocks[i];
		if (inode_.blocks[i] == 0)
			return EFIL_INVALID_SECTOR;
	}
		// indirect
	else if (i < INODE_BLOCKS_MAX + indirect_max)
	{
		if (inode_.indirect_block == 0)
			return EFIL_INVALID_SECTOR;
		ret = fs_->read_data_object(inode_.indirect_block, (i - INODE_BLOCKS_MAX) * sizeof(uint32_t), sizeof(uint32_t),
		                            sector_out);
		return ret;
	}
		// double indirect
	else if (i < INODE_BLOCKS_MAX + indirect_max + double_indirect_max)
	{
		const auto index_level_1 = (i - INODE_BLOCKS_MAX - indirect_max) / block_size_bytes;
		const auto index_level_2 = (i - INODE_BLOCKS_MAX - indirect_max) % block_size_bytes;
		uint32_t pointer;

		if (inode_.double_indirect_block == 0)
			return EFIL_INVALID_SECTOR;

		ret = fs_->read_data_object(inode_.double_indirect_block, index_level_1 * sizeof(uint32_t), sizeof(uint32_t),
		                            &pointer);
		if (ret < 0)
			return ret;
		if (pointer == 0)
			return EFIL_INVALID_SECTOR;

		ret = fs_->read_data_object(pointer, index_level_2 * sizeof(uint32_t), sizeof(uint32_t), sector_out);
		return ret;
	}
		// out of bounds for sure
	else
		return EFIL_INVALID_POS;
	return 0;
}

int file::allocate_block(const uint32_t block_index)
{
	int ret;
	const auto block_size_bytes = fs_->super_block_.block_size * SECTOR_SIZE;
	const auto indirect_max = block_size_bytes / sizeof(uint32_t);
	const auto double_indirect_max = indirect_max * indirect_max;
	uint32_t temp;

	uint32_t free_block;

	fs_->read_inode(inode_n_, &inode_);
	if (block_index < INODE_BLOCKS_MAX)
	{
		if (inode_.blocks[block_index] == 0)
		{
			free_block = fs_->get_free_block();
			if (free_block == INVALID_BLOCK)
				return ED_OUT_OF_BLOCKS;

			fs_->set_block_status(free_block, true);
			inode_.blocks[block_index] = free_block;
			fs_->write_inode(inode_n_, &inode_);
			return 0;
		}
	}
		// indirect
	else if (block_index < INODE_BLOCKS_MAX + indirect_max)
	{
		if (inode_.indirect_block == 0)
		{
			free_block = fs_->get_free_block();
			if (free_block == INVALID_BLOCK)
				return ED_OUT_OF_BLOCKS;

			fs_->set_block_status(free_block, true);
			inode_.indirect_block = free_block;
			ret = fs_->write_inode(inode_n_, &inode_);
			if (ret < 0)
				return ret;

			// set mem to 0
			ret = fs_->read_data_block(inode_.indirect_block, fs_->data_buffer_, 1);
			if (ret < 0)
				return ret;
			memset(fs_->data_buffer_, 0, block_size_bytes);
			ret = fs_->write_data_block(inode_.indirect_block, fs_->data_buffer_, 1);
			if (ret < 0)
				return ret;
		}
		ret = fs_->read_data_object(inode_.indirect_block, (block_index - INODE_BLOCKS_MAX) * sizeof(uint32_t),
		                            sizeof(uint32_t), &temp);
		if (ret < 0)
			return ret;

		if (temp == 0)
		{
			free_block = fs_->get_free_block();
			if (free_block == INVALID_BLOCK)
				return ED_OUT_OF_BLOCKS;

			fs_->set_block_status(free_block, true);

			ret = fs_->write_data_object(inode_.indirect_block, (block_index - INODE_BLOCKS_MAX) * sizeof(uint32_t),
			                             sizeof(uint32_t), &free_block);
			if (ret < 0)
				return ret;

			ret = fs_->write_inode(inode_n_, &inode_);
			if (ret < 0)
				return ret;

			return 0;
		}
	}
		// double indirect
	else if (block_index < INODE_BLOCKS_MAX + indirect_max + double_indirect_max)
	{
		const auto index_level_1 = (block_index - INODE_BLOCKS_MAX - indirect_max) / block_size_bytes;
		const auto index_level_2 = (block_index - INODE_BLOCKS_MAX - indirect_max) % block_size_bytes;
		uint32_t pointer;

		if (inode_.double_indirect_block == 0)
		{
			free_block = fs_->get_free_block();
			if (free_block == INVALID_BLOCK)
				return ED_OUT_OF_BLOCKS;

			fs_->set_block_status(free_block, true);
			inode_.double_indirect_block = free_block;
			ret = fs_->write_inode(inode_n_, &inode_);
			if (ret < 0)
				return ret;

			// set mem to 0
			ret = fs_->read_data_block(inode_.double_indirect_block, fs_->data_buffer_, 1);
			if (ret < 0)
				return ret;

			memset(fs_->data_buffer_, 0, block_size_bytes);

			ret = fs_->write_data_block(inode_.double_indirect_block, fs_->data_buffer_, 1);
			if (ret < 0)
				return ret;
		}

		ret = fs_->read_data_object(inode_.double_indirect_block, index_level_1 * sizeof(uint32_t), sizeof(uint32_t),
		                            &pointer);
		if (ret < 0)
			return ret;

		if (pointer == 0)
		{
			free_block = fs_->get_free_block();
			if (free_block == INVALID_BLOCK)
				return ED_OUT_OF_BLOCKS;

			fs_->set_block_status(free_block, true);
			ret = fs_->write_data_object(inode_.double_indirect_block, index_level_1 * sizeof(uint32_t), sizeof(uint32_t),
			                             &free_block);
			if (ret < 0)
				return ret;
			pointer = free_block;

			// set mem to 0
			ret = fs_->read_data_block(free_block, fs_->data_buffer_, 1);
			if (ret < 0)
				return ret;

			memset(fs_->data_buffer_, 0, block_size_bytes);

			ret = fs_->write_data_block(free_block, fs_->data_buffer_, 1);
			if (ret < 0)
				return ret;
		}

		ret = fs_->read_data_object(pointer, index_level_2 * sizeof(uint32_t), sizeof(uint32_t), &temp);
		if (ret < 0)
			return ret;
		if (temp == 0)
		{
			free_block = fs_->get_free_block();
			if (free_block == INVALID_BLOCK)
				return ED_OUT_OF_BLOCKS;

			fs_->set_block_status(free_block, true);
			ret = fs_->write_data_object(pointer, index_level_2 * sizeof(uint32_t), sizeof(uint32_t), &free_block);
			if (ret < 0)
				return ret;
			return 0;
		}
	}
	else
		return EFIL_TOO_BIG;

	return 0;
}
