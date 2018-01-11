#ifndef FS_H_GUARD
#define FS_H_GUARD

#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include "../disk/disk.h"
#include "../spacemap/spacemap.h"
#include "../superblock/superblock.h"
#include "../entities/file/file.h"
#include "../entities/dir/dir.h"
#include "../entities/dir/dirent.h"
#include "../inode/inode.h"

typedef unsigned int fid_t;
typedef unsigned int did_t;

class file_system
{
public: 
    /* TODO: REMOVAL OF OPENED FILES AND DIRS CORRECTLY */
    // Default constructor
    file_system() 
        : file_system(16) {}
    /* TODO: CACHE */
	explicit file_system(std::size_t)
		: disk_ {}, data_buffer_{ nullptr }, super_block_{} {}

// DISK REGION ----------------
    // Create a new disk image
    int init(const std::string & disk_file, uint32_t inodes_count, 
        std::size_t disk_size, uint32_t block_size);
    // Load a disk image from a file
    int load(const std::string & disk_file);
    // Unload current disk image
    void unload();
    // Sync changes to disk image file
    void sync();
// END DISK REGION -------------

// FILE REGION -----------------
    int create(const std::string & file_name);
    int link(const std::string & original_file, std::string & new_file);
    int unlink(const std::string & file);
    // Open a file
    fid_t open(const std::string & disk_file);
    // Close a file
    int close(fid_t fid);

    int read(fid_t fid, char * buffer, std::size_t size);
    int write(fid_t fid, const char * buffer, std::size_t size);

    int seek(fid_t fid, std::size_t pos);

    int trunc(fid_t fid, std::size_t new_length);
// END FILE REGION -------------
// DIRECTORY REGION ------------
    int mkdir(const std::string & dir_name);
    // must be empty
    int rmdir(const std::string & dir_name);

    // create a dir class, that will implement the read operations et c.
    did_t opendir(const std::string & dir_name);
    int closedir(did_t dir_id);

    dirent_t readdir(did_t dir_id);
    int rewind_dir(did_t dir_id);
// END DIRECTORY REGION --------

    super_block_t get_super_block();
    inline space_map * get_inode_map() { return inode_map_; }
    inline space_map * get_space_map() { return space_map_; }
private:
    disk disk_;
    char * data_buffer_;
    super_block_t super_block_;
    space_map * inode_map_;
	space_map * space_map_;

    std::vector<file> files_;
    std::vector<directory> dirs_;

    uint32_t get_free_block();
    void set_block_status(uint32_t block_id, bool is_busy);

    // proxies for caching
    int read_block(uint32_t start_sector, char * buffer, std::size_t size);
    int write_block(uint32_t start_sector, const char * buffer, std::size_t size);

    int read_object(uint32_t start_sector, std::size_t offset, std::size_t obj_size, void * buffer);
    int write_object(uint32_t start_sector, std::size_t offset, std::size_t obj_size, const void * buffer);

    int write_inode(uint32_t inode_id, const inode_t * inode);
    int read_inode(uint32_t inode_id, inode_t * inode);

    inode_t get_new_inode(file_type f_type, uint16_t permissions);
    uint32_t get_free_inode();
    void set_inode_status(uint32_t inode_num, bool is_busy);

    int get_inode_by_path(const std::string & path, uint32_t * inode_out);

    friend class file;
};

#endif