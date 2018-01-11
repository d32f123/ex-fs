#ifndef DIRENT_H_GUARD
#define DIRENT_H_GUARD

#define DIRENT_NAME_MAX (32)

#include "../../inode/inode.h"

typedef struct dirent_struct
{
    uint32_t inode_n;
    file_type f_type;
    char name[DIRENT_NAME_MAX];
} dirent_t;

#define DIRENT_INVALID dirent_t{.inode_n = INODE_INVALID, file_type::other, ""}

#endif