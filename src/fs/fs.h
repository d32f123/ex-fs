#ifndef FS_H_GUARD
#define FS_H_GUARD

#include <string>
#include <vector>

#include "../disk/disk.h"
#include "../spacemap/spacemap.h"
#include "../superblock/superblock.h"
#include "../entities/file/file.h"
#include "../entities/dir/dir.h"
#include "../entities/dir/dirent.h"
#include "../inode/inode.h"
#include "../cache/cache.h"
#include "../storage/storage.h"

#define SUPERBLOCK_SECT	(0)
#define STORAGE_SIZE	(128)
#define DATABUFFER_SIZE	(1)
#define CACHE_SIZE_DEF	(6)

typedef unsigned int fid_t;
typedef unsigned int did_t;

#define INVALID_FID		(static_cast<fid_t>(-1))
#define INVALID_DID		(static_cast<did_t>(-1))
#define INVALID_BLOCK	(static_cast<uint32_t>(-1))

class file_system
{
public:
	// Default constructor
	file_system()
		: file_system(CACHE_SIZE_DEF) {}

	explicit file_system(std::size_t cache_size)
		: data_buffer_{nullptr}, super_block_{},
		  inode_map_(nullptr), space_map_(nullptr), cache_{cache_size} {}

	void trace();
	void traceblock(uint32_t block);

	// RULE OF FIVE REGION --------
	file_system(const file_system& that);
	file_system(file_system&& that) noexcept;

	~file_system();

	file_system& operator=(const file_system& that);
	file_system& operator=(file_system&& that) noexcept;
	// END RULE OF FIVE REGION ----

	// DISK REGION ----------------
	// Create a new disk image
	int init(const std::string& disk_file, uint32_t inodes_count,
	         std::size_t disk_size, uint32_t block_size);
	// Load a disk image from a file
	int load(const std::string& disk_file);
	// Unload current disk image
	void unload();
	// Sync changes to disk image file
	int sync();
	// END DISK REGION -------------

	// FILE REGION -----------------
	int create(const std::string& file_name);
	int link(const std::string& original_file, const std::string& new_file);
	int unlink(const std::string& file_name);
	// Open a file
	fid_t open(const std::string& disk_file);
	// Close a file
	int close(fid_t fid) const;

	int read(fid_t fid, char* buffer, std::size_t size);
	int write(fid_t fid, const char* buffer, std::size_t size);

	int seek(fid_t fid, std::size_t pos);

	int trunc(fid_t fid, std::size_t new_length);
	// END FILE REGION -------------
	// DIRECTORY REGION ------------
	int cd(const std::string& new_dir);

	int mkdir(const std::string& dir_name);
	// must be empty
	int rmdir(const std::string& dir_name);

	// create a dir class, that will implement the read operations et c.
	did_t opendir(const std::string& dir_name);
	int closedir(did_t dir_id) const;

	dirent_t readdir(did_t dir_id);
	int rewind_dir(did_t dir_id);
	// END DIRECTORY REGION --------

	static std::string concat_paths(const std::string& path1, const std::string& path2);
	super_block_t get_super_block() const;
	space_map* get_inode_map() const { return inode_map_; }
	space_map* get_space_map() const { return space_map_; }
private:
	disk disk_;
	char* data_buffer_;
	super_block_t super_block_;
	space_map* inode_map_;
	space_map* space_map_;

	bool sb_dirty_{false};
	bool im_dirty_{false};
	bool sm_dirty_{false};

	cache<uint32_t, std::vector<char>> cache_{CACHE_SIZE_DEF};

	storage<file> files_{STORAGE_SIZE};
	storage<directory> dirs_{STORAGE_SIZE};
	directory cwd_;

	uint32_t get_free_block() const;
	void set_block_status(uint32_t block_id, bool is_busy);

	// proxies for caching
	int read_block(uint32_t start_block, char* buffer, std::size_t size);
	int write_block(uint32_t start_block, const char* buffer, std::size_t size);

	int read_data_block(uint32_t start_block, char* buffer, std::size_t size);
	int write_data_block(uint32_t start_block, const char* buffer, std::size_t size);

	int read_object(uint32_t start_block, std::size_t offset, std::size_t obj_size, void* buffer);
	int write_object(uint32_t start_block, std::size_t offset, std::size_t obj_size, const void* buffer);

	int read_data_object(uint32_t start_block, std::size_t offset, std::size_t obj_size, void* buffer);
	int write_data_object(uint32_t start_block, std::size_t offset, std::size_t obj_size, const void* buffer);

	int write_inode(uint32_t inode_id, const inode_t* inode);
	int read_inode(uint32_t inode_id, inode_t* inode);

	static inode_t get_new_inode(file_type f_type, uint16_t permissions);
	uint32_t get_free_inode() const;
	void set_inode_status(uint32_t inode_num, bool is_busy);

	static std::vector<std::string> get_dir_and_file(const std::string& file_name);
	int get_inode_by_path(const std::string& path, uint32_t* inode_out);

	int do_unlink(const std::string& file_name, bool force);
	int do_create(const std::string& file_name, file_type f_type,
	              uint32_t* inode_out = nullptr, uint32_t* prev_dir_inode_out = nullptr);

	friend class file;
};

#endif
