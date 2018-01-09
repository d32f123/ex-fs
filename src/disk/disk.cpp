#include "disk.h"

int disk::create(std::string disk_name, std::size_t size)
{
	auto disk = new std::fstream;

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

    this->disk_file_ = disk;

    disk->flush();
    return 0;
}

int disk::load(std::string & disk_name)
{
	auto disk = new std::fstream;

    this->unload();

    if (!disk)
        return EP_NOMEM;

    disk->open(disk_name, std::fstream::in | std::fstream::out);

    if (!(*disk))
        return EP_OPFIL;

    this->disk_file_ = disk;
    return 0;
}

int disk::unload()
{
    if (this->disk_file_ && this->disk_file_->is_open())
    {
        this->disk_file_->flush();
        this->disk_file_->close();
        delete this->disk_file_;
        this->disk_file_ = nullptr;
        return 0;
    }
    return ED_NODISK;
}

int disk::read_block(const uint32_t start_sector, char * buffer, const std::size_t size)
{
    if (!(this->disk_file_) || !(this->disk_file_->is_open()))
        return ED_NODISK;
    this->disk_file_->seekg(start_sector * SECTOR_SIZE, std::fstream::beg);
    this->disk_file_->read(buffer, size * SECTOR_SIZE);

    if (this->disk_file_)
        return 0;
    return EP_RDFIL;
}

int disk::write_block(uint32_t start_sector, const char * buffer, const std::size_t size)
{
    if (!(this->disk_file_) || !(this->disk_file_->is_open()))
        return ED_NODISK;
    this->disk_file_->seekp(start_sector * SECTOR_SIZE, std::fstream::beg);
    this->disk_file_->write(buffer, size * SECTOR_SIZE);

    if (this->disk_file_)
        return 0;
    return EP_WRFIL;
}

bool disk::is_open() const
{
    return this->disk_file_ && this->disk_file_->is_open();
}
