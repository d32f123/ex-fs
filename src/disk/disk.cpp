#include "disk.h"

int disk::create(std::string disk_name, std::size_t size)
{
    std::fstream * disk = new std::fstream;

    this->unload();

    if (!disk)
        return EP_NOMEM;

    disk->open(disk_name, std::fstream::in | std::fstream::out | std::fstream::trunc);

    if (!(*disk))
        return EP_OPFIL;

    char buffer[SECTOR_SIZE];
    for (std::size_t i = 0; i < size; ++i)
    {
        if (!disk->write(buffer, SECTOR_SIZE))
            return EP_WRFIL;
    }

    this->disk_file = disk;

    disk->flush();
    return 0;
}

int disk::load(std::string disk_name)
{
    std::fstream * disk = new std::fstream;

    this->unload();

    if (!disk)
        return EP_NOMEM;

    disk->open(disk_name, std::fstream::in | std::fstream::out);

    if (!(*disk))
        return EP_OPFIL;

    this->disk_file = disk;
    return 0;
}

int disk::unload()
{
    if (this->disk_file && this->disk_file->is_open())
    {
        this->disk_file->flush();
        this->disk_file->close();
        this->disk_file = nullptr;
    }
}

int disk::read_block(uint32_t start_sector, char * buffer, std::size_t size)
{
    if (!(this->disk_file) || !(this->disk_file->is_open()))
        return ED_NODISK;
    this->disk_file->seekg(start_sector * SECTOR_SIZE, this->disk_file->beg);
    this->disk_file->read(buffer, size * SECTOR_SIZE);

    if (this->disk_file)
        return 0;
    return EP_RDFIL;
}

int disk::write_block(uint32_t start_sector, char * buffer, std::size_t size)
{
    if (!(this->disk_file) || !(this->disk_file->is_open()))
        return ED_NODISK;
    this->disk_file->seekp(start_sector * SECTOR_SIZE, this->disk_file->end);
    this->disk_file->write(buffer, size * SECTOR_SIZE);

    if (this->disk_file)
        return 0;
    return EP_WRFIL;
}

bool disk::is_open()
{
    return this->disk_file && this->disk_file->is_open();
}