#ifndef DIR_H_GUARD
#define DIR_H_GUARD

#include <string>
#include <cstdint>

#include "../file/file.h"
#include "dirent.h"

class file_system;

class directory
{
public:
	directory() = default;
	directory(directory&& that) noexcept; // move constructor
	directory(const directory& that); // copy constructor
	// create a new dir
	directory(const std::string& new_dir_name, file_system* fs);
	// open an existing dir
	directory(uint32_t inode_n, file_system* fs);
	explicit directory(file& dir_file);

	void reopen(uint32_t inode_n, file_system* fs) const;

	int add_entry(uint32_t inode_n, const std::string& filename) const;
	int remove_entry(const std::string& filename) const;

	dirent_t find(const std::string& filename) const;
	dirent_t read() const;
	void rewind() const;

	~directory() { delete file_; }

	directory& operator=(const directory& that);
	directory& operator=(directory&& that) noexcept;

	file* get_file() const { return file_; }
private:
	file* file_{nullptr};
};

#endif
