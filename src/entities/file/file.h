#ifndef FILE_H_GUARD
#define FILE_H_GUARD

#include <cstdlib>
#include <string>

#include "../../inode/inode.h"

class file_system;
class directory;

class file
{
public:
	file() = default;
	file(const std::string& filename, file_system* fs);
	file(uint32_t inode_n, file_system* fs);

	file(const file& that) = default;
	file(file&& that) = default;

	file& operator=(const file& that) = default;
	file& operator=(file&& that) = default;

	~file() = default;

	void reopen(uint32_t inode_n, file_system* file_sys);

	int read(char* buffer, std::size_t size);
	int write(const char* buffer, std::size_t size);

	int seek(std::size_t pos);

	int trunc(std::size_t new_size);

	std::size_t get_curr_pos() const { return curr_pos_; }
	uint32_t get_inode_n() const { return inode_n_; }
private:
	file_system* fs_{nullptr};
	inode_t inode_{};
	uint32_t inode_n_{INVALID_INODE};
	std::size_t curr_pos_{0};

	int get_inode(inode_t* inode_out) const;

	int get_sector(uint32_t i, uint32_t* sector_out, bool do_allocate = false);
	int allocate_block(uint32_t block_index);

	int read_unaligned(uint32_t start_block, std::size_t offset, std::size_t obj_size, void* buffer);
	int write_unaligned(uint32_t start_block, std::size_t offset, std::size_t obj_size, const void* buffer);

	friend class directory;
};

#endif
