#ifndef ERRORS_H_GUARD
#define ERRORS_H_GUARD

#include <iostream>
#include <iomanip>

#define EP_NOMEM            -1
#define EP_OPFIL            -2
#define EP_WRFIL            -3
#define EP_RDFIL            -4

#define ED_NODISK           -5
#define ED_OUT_OF_BLOCKS	-18

#define EDIR_FILE_NOT_FOUND -6
#define EDIR_FILE_EXISTS    -7
#define EDIR_INVALID_PATH   -8
#define EDIR_NOT_A_DIR      -9
#define EDIR_NOT_EMPTY      -10

#define EFIL_INVALID_POS    -11
#define EFIL_INVALID_SECTOR -12
#define EFIL_WRONG_TYPE		-17
#define EFIL_TOO_BIG		-19

#define EDID_INVALID_ID     -13
#define EFID_INVALID_ID     -14

#define EIND_INVALID_INODE  -15
#define EIND_OUT_OF_INODES	-16

inline std::string err_to_string(const int err)
{
	if (err >= 0)
		return "Successful";
	switch (err)
	{
	case EP_NOMEM: return "Out of memory";
	case EP_OPFIL: return "Failed to open file";
	case EP_WRFIL: return "Failed to write to file";
	case EP_RDFIL: return "Failed to read from file";
	case ED_NODISK: return "No disk file mounted";
	case ED_OUT_OF_BLOCKS: return "Disk is out of free blocks";
	case EDIR_FILE_NOT_FOUND: return "No such file";
	case EDIR_FILE_EXISTS: return "File already exists";
	case EDIR_INVALID_PATH: return "Path is invalid";
	case EDIR_NOT_A_DIR: return "File is not a directory";
	case EDIR_NOT_EMPTY: return "Directory is not empty";
	case EFIL_INVALID_POS: return "Invalid file seek position";
	case EFIL_INVALID_SECTOR: return "Access to invalid sector";
	case EFIL_WRONG_TYPE: return "File has wrong type";
	case EFIL_TOO_BIG: return "File is too big";
	case EDID_INVALID_ID: return "No such id";
	case EFID_INVALID_ID: return "No such id";
	case EIND_INVALID_INODE: return "Inode was invalid";
	case EIND_OUT_OF_INODES: return "Disk is out of free inodes";
	default: return "Unkown error";
	}
}

#include <iomanip>

struct hex_char_struct
{
	unsigned char c;
	hex_char_struct(unsigned char _c) : c(_c) { }
};

inline std::ostream& operator<<(std::ostream& o, const hex_char_struct& hs)
{
	return (o << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(hs.c));
}

inline hex_char_struct hex(unsigned char _c)
{
	return { _c };
}

#endif