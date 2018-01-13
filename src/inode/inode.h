#ifndef INODE_H_GUARD
#define INODE_H_GUARD

#include <cstdint>
#include <iostream>
#include <iomanip>

#define INODE_ROOT_ID       0
#define INODE_BLOCKS_MAX    8
#define INVALID_INODE       ((uint32_t)-1)

enum class file_type : uint8_t { regular = 0, dir = 1, other = 2 };

typedef struct inode_struct
{
    inode_struct() : f_type(file_type::other), blocks(), indirect_block(0), double_indirect_block(0) {}

    file_type f_type;
    //file-type(4 bits)|SUID-SGID-STICKY|r-w-x|r-w-x|r-w-x
    // total-- 16 bits used
    uint16_t permissions{};

    // time of last access
    uint64_t access_time{};
    // time of last change (perms or content)
    uint64_t change_time{};
    // time of last modification (only content)
    uint64_t modify_time{};

    uint32_t links_count{};

    uint32_t blocks[INODE_BLOCKS_MAX];
    uint32_t indirect_block;
    uint32_t double_indirect_block;
} inode_t;

inline std::ostream& operator<<(std::ostream& os, inode_t inode)
{
	using std::endl;
	using std::hex;
	using std::dec;
	os << "f_type: " << static_cast<int>(inode.f_type) << endl
		<< "perms: " << inode.permissions << endl
		<< "access time: " << inode.access_time << endl
		<< "change time: " << inode.change_time << endl
		<< "modify time: " << inode.modify_time << endl
		<< "links: " << inode.links_count << endl
		<< "blocks: " << endl;
	for (auto i = 0; i < INODE_BLOCKS_MAX; ++i)
	{
		os << "[" << i << "]: " << hex << inode.blocks[i] << dec << endl;
	}
	os << "indirect block: " << hex << inode.indirect_block << endl;
	os << "double ind block: " << hex << inode.double_indirect_block << endl << dec;
	return os;
}

#endif
