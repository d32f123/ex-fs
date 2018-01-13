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

inline std::ostream& operator<<(std::ostream& os, dirent_struct d)
{
	return os << static_cast<unsigned long>(d.inode_n) << ":" << static_cast<uint8_t>(d.f_type) << ":" << d.name;
}

#define INVALID_DIRENT (dirent_t{INVALID_INODE, file_type::other, ""})

#endif
