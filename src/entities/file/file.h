#ifndef FILE_H_GUARD
#define FILE_H_GUARD

#include <stdlib.h>

#include "../../inode/inode.h"

class file_system;
class directory;

class file
{
public:
    file(uint32_t inode_n, file_system * fs);
    void reopen(uint32_t inode_n, file_system * fs);

    int read(char * buffer, std::size_t size);
    int write(const char * buffer, std::size_t size);

    int seek(std::size_t pos);

    int trunc(std::size_t new_size);

    std::size_t get_curr_pos() { return curr_pos; }
private:
    file_system * fs;
    inode_t inode;
    uint32_t inode_n_;
    std::size_t curr_pos;

    int get_inode(inode_t * inode_out);

    int get_sector(uint32_t block_index, uint32_t * sector_out, bool do_allocate = false);
    void allocate_block(uint32_t block_index);

    int read_unaligned(uint32_t start_block, std::size_t offset, std::size_t obj_size, void * buffer);
    int write_unaligned(uint32_t start_block, std::size_t offset, std::size_t obj_size, const void * buffer);

    friend class directory;
};

#endif