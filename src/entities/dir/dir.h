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
	directory(const directory& that);
    // create a new dir
    directory(std::string & new_dir_name, file_system * fs);
    // open an existing dir
    directory(uint32_t inode_n, file_system * fs);
    directory(file & dir_file);

    void reopen(uint32_t inode_n, file_system * fs);

    int add_entry(uint32_t inode_n, std::string & filename);
    int remove_entry(std::string & filename);

    dirent_t find(std::string & filename);
    dirent_t read();
    void rewind();

    ~directory() {delete file_;}

	directory& operator=(const directory & that);
private:
    file * file_;
};

#endif