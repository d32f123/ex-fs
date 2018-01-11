#ifndef INODE_H_GUARD
#define INODE_H_GUARD

#include <cstdint>

#define INODE_ROOT_ID       0
#define INODE_BLOCKS_MAX    8
#define INODE_INVALID       ((uint32_t)-1)

enum class file_type : uint8_t { regular = 0, dir = 1, other = 2 };

typedef struct inode_struct
{
    inode_struct() : f_type(file_type::other), blocks(), indirect_block(0), double_indirect_block(0) {}

    file_type f_type;
    //file-type(4 bits)|SUID-SGID-STICKY|r-w-x|r-w-x|r-w-x
    // total-- 16 bits used
    uint16_t permissions{};

    // time of last access
    uint32_t access_time{};
    // time of last change (perms or content)
    uint32_t change_time{};
    // time of last modification (only content)
    uint32_t modify_time{};

    uint32_t links_count{};

    uint32_t blocks[INODE_BLOCKS_MAX];
    uint32_t indirect_block;
    uint32_t double_indirect_block;
} inode_t;

#endif