#ifndef DISK_H_GUARD
#define DISK_H_GUARD

#include <string>
#include <fstream>
#include <cstdint>

#define SECTOR_SIZE (512)

class disk
{
public:
	disk() = default;
	explicit disk(const std::string & disk_name) { load(disk_name); }
    disk(const std::string & disk_name, const std::size_t size) { create(disk_name, size); }

	disk(const disk& that);
	disk(disk&& that) noexcept;

	disk& operator=(const disk& that);
	disk& operator=(disk&& that) noexcept;
	
	~disk();

    int create(const std::string & disk_name, std::size_t size);
    int load(const std::string & disk_name);
    int unload();

    int read_block(uint32_t start_sector, char * buffer, std::size_t size) const;
    int write_block(uint32_t start_sector, const char * buffer, std::size_t size) const;

    bool is_open() const;
private:
	std::string filename_{};
    std::fstream * disk_file_{nullptr};
};

#endif