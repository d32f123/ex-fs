#include "fs.h"

#include <fstream>

int file_system::load(std::string disk_file)
{
    std::fstream * disk = new std::fstream;

    disk->open(disk_file, std::fstream::in | std::fstream::out);
    if (!(*disk))
        return E_OPEN_FILE;

    this->disk = disk;

    (*disk).read((char *)&super_block, SECTOR_SIZE);

    return 0;
}