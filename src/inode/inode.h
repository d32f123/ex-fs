#ifndef INODE_H_GUARD
#define INODE_H_GUARD

#include <inttypes.h>

typedef struct inode_struct
{
    //file-type(4 bits)|SUID-SGID-STICKY|r-w-x|r-w-x|r-w-x
    // total-- 16 bits used
    uint16_t permissions;

    // time of last access
    uint32_t access_time;
    // time of last change (perms or content)
    uint32_t change_time;
    // time of last modification (only content)
    uint32_t modify_time;

    uint32_t links_count;
} inode_t;

#endif