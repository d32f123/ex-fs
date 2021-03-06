#include "disk.h"

#include "../errors.h"
#include <iomanip>

disk::disk(const disk& that)
{
	if (that.disk_file_ && that.disk_file_->is_open())
	{
		load(that.filename_);
	}
	filename_ = that.filename_;
}

disk::disk(disk&& that) noexcept
{
	filename_ = that.filename_;
	that.filename_ = {};

	disk_file_ = that.disk_file_;
	that.disk_file_ = nullptr;
}

disk& disk::operator=(const disk& that)
{
	if (this == &that) return *this;

	if (that.disk_file_ && that.disk_file_->is_open())
	{
		load(that.filename_);
	}
	filename_ = that.filename_;

	return *this;
}

disk& disk::operator=(disk&& that) noexcept
{
	if (this == &that) return *this;

	filename_ = that.filename_;
	that.filename_ = {};

	disk_file_ = that.disk_file_;
	that.disk_file_ = nullptr;

	return *this;
}

disk::~disk()
{
	this->unload();
}

int disk::create(const std::string& disk_name, std::size_t size)
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

int disk::load(const std::string& disk_name)
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

int disk::read_block(const uint32_t start_sector, char* buffer, const std::size_t size) const
{
	if (!(this->disk_file_) || !(this->disk_file_->is_open()))
		return ED_NODISK;
	this->disk_file_->seekg(start_sector * SECTOR_SIZE, std::fstream::beg);
	this->disk_file_->read(buffer, size * SECTOR_SIZE);

	if (this->disk_file_)
		return size * SECTOR_SIZE;
	return EP_RDFIL;
}

int disk::write_block(uint32_t start_sector, const char* buffer, const std::size_t size) const
{
	if (!(this->disk_file_) || !(this->disk_file_->is_open()))
		return ED_NODISK;
	this->disk_file_->seekp(start_sector * SECTOR_SIZE, std::fstream::beg);
	this->disk_file_->write(buffer, size * SECTOR_SIZE);

	if (this->disk_file_)
		return size * SECTOR_SIZE;
	return EP_WRFIL;
}

bool disk::is_open() const
{
	return this->disk_file_ && this->disk_file_->is_open();
}
