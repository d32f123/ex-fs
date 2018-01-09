#ifndef DISK_H_GUARD
#define DISK_H_GUARD

#include "../errors.h"

#include <inttypes.h>
#include <string>
#include <fstream>

#define SECTOR_SIZE (512)

class disk
{
public:
    disk() : disk_file_ {nullptr} { }
	explicit disk(std::string disk_name) : disk() { load(disk_name); }
    disk(std::string & disk_name, const std::size_t size) : disk() { create(disk_name, size); }

    int create(std::string disk_name, std::size_t size);
    int load(std::string & disk_name);
    int unload();

    int read_block(uint32_t start_sector, char * buffer, std::size_t size);
    int write_block(uint32_t start_sector, const char * buffer, std::size_t size);

    bool is_open() const;
private:
    std::fstream * disk_file_;
};

#endif