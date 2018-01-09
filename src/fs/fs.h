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
#include "../entities/dir/dir.h"
#include "../entities/dir/dirent.h"

typedef unsigned int fid_t;
typedef unsigned int did_t;

class file_system
{
public: 
    // Default constructor
    file_system() 
        : file_system(16) {}
    /* TODO: CACHE */
	explicit file_system(std::size_t)
		: disk_ {}, data_buffer_{ nullptr }, super_block_{} {}

// DISK REGION ----------------
    // Create a new disk image
    int init(std::string & disk_file, uint32_t inodes_count, 
        std::size_t disk_size, uint32_t block_size);
    // Load a disk image from a file
    int load(std::string & disk_file);
    // Unload current disk image
    void unload();
    // Sync changes to disk image file
    void sync();
// END DISK REGION -------------

// FILE REGION -----------------
    int create(std::string & file_name);
    int link(std::string & original_file, std::string & new_file);
    int unlink(std::string & file);
    // Open a file
    fid_t open(std::string & disk_file);
    // Close a file
    void close(fid_t fid);

    int read(fid_t fid, char * buffer, std::size_t size);
    int write(fid_t fid, char * buffer, std::size_t size);

    int seek(fid_t fid, std::size_t pos);
// END FILE REGION -------------
// DIRECTORY REGION ------------
    int mkdir(std::string & dir_name);
    // must be empty
    int rmdir(std::string & dir_name);

    // create a dir class, that will implement the read operations et c.
    did_t opendir(std::string & dir_name);
    int closedir(did_t dir_id);

    dirent_t readdir(did_t dir_id);
// END DIRECTORY REGION --------

    inline super_block_t get_super_block() { return super_block_; }
    inline space_map * get_inode_map() { return inode_map_; }
    inline space_map * get_space_map() { return space_map_; }
private:
    disk disk_;
    char * data_buffer_;
    super_block_t super_block_;
    space_map * inode_map_;
	space_map * space_map_;

    int read_object(uint32_t start_sector, std::size_t offset, std::size_t obj_size, void * buffer);
    int write_object(uint32_t start_sector, std::size_t offset, std::size_t obj_size, const void * buffer);

    uint32_t get_free_inode();
    void set_inode_status(uint32_t inode_num, bool is_busy);
    
    int do_mkdir(std::string & dir_name);
};

#endif