#ifndef FS_H_GUARD
#define FS_H_GUARD

#include <stdlib.h>
#include <string>
#include <fstream>

#include "disk.h"
#include "../superblock/superblock.h"

#define E_OPEN_FILE     -1

typedef unsigned int fid_t;

class file_system
{
public: 
    // Default constructor
    file_system() 
        : file_system(16) {}
    file_system(std::size_t)
        : disk {nullptr}, data_buffer {new char[SECTOR_SIZE]} {};

    // Load a disk image from a file
    int load(std::string disk_file);
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
private:
    std::fstream * disk;
    char * data_buffer;
    super_block_t super_block;


};

#endif