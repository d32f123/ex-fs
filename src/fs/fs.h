#ifndef FS_H_GUARD
#define FS_H_GUARD

#include <stdlib.h>
#include <string>
#include <fstream>

#include "../disk/disk.h"
#include "../superblock/superblock.h"
#include "../inode/inode.h"
#include "../errors.h"
#include "../spacemap/spacemap.h"

typedef unsigned int fid_t;

class file_system
{
public: 
    // Default constructor
    file_system() 
        : file_system(16) {}
    /* TODO: CACHE */
	explicit file_system(std::size_t)
		: disk_ {}, data_buffer_{ nullptr }, super_block_{} {}

    // Create a new disk image
    int init(std::string & disk_file, uint32_t inodes_count, 
        std::size_t disk_size, uint32_t block_size);
    // Load a disk image from a file
    int load(std::string & disk_file);
    // Unload current disk image
    void unload();
    // Sync changes to disk image file
    void sync();
    
    // Open a file
    fid_t open(std::string disk_file);
    // Close a file
    void close(fid_t fid);

    int read(fid_t fid, char * buffer, std::size_t size);
    int write(fid_t fid, char * buffer, std::size_t size);

    int seek(fid_t fid, std::size_t pos);

    super_block_t get_super_block() { return super_block_; }
private:
    disk disk_;
    char * data_buffer_;
    super_block_t super_block_;
    space_map * inode_map_;
	space_map * space_map_;
};

#endif