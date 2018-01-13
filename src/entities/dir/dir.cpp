#define _CRT_SECURE_NO_WARNINGS
#include "dir.h"

#include "../../errors.h"
#include "../../fs/fs.h"

#include <cstring>

directory::directory(directory&& that) noexcept
{
	file_ = that.file_;
	that.file_ = nullptr;
}

directory::directory(const std::string& new_dir_name, file_system* fs)
{
	auto ret = fs->mkdir(new_dir_name);
	if (ret < 0)
		throw std::exception();

	file_ = new file(new_dir_name, fs);
}

directory& directory::operator=(directory&& that) noexcept
{
	if (this == &that) return *this;

	delete file_;
	file_ = that.file_;
	that.file_ = nullptr;
	return *this;
}

directory::directory(const directory & that)
{
	if (that.file_ == nullptr)
		file_ = nullptr;
	else
		file_ = new file(that.file_->inode_n_, that.file_->fs_);
}

directory::directory(uint32_t inode_n, file_system * fs) : file_(new file(inode_n, fs))
{
    if (file_->inode_.f_type != file_type::dir)
        throw std::exception();
}

directory::directory(file & dir_file) : file_(&dir_file)
{
}

void directory::reopen(uint32_t inode_n, file_system * fs)
{
    file_->reopen(inode_n, fs);
    if (file_->inode_.f_type != file_type::dir)
        throw std::exception();
}

dirent_t directory::find(const std::string & filename)
{
    int prev_pos = file_->get_curr_pos();
    file_->seek(0);
    
    dirent_t dirent;
    do {
        auto ret = file_->read(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
		if (ret < 0)
			break;
        auto curr_filename = std::string(dirent.name);
        if (curr_filename == filename)
        {
            file_->seek(prev_pos);
            return dirent;
        }
    } while (dirent.inode_n != INVALID_INODE);
    
    dirent.inode_n = INVALID_INODE;
    dirent.name[0] = '\0';

    file_->seek(prev_pos);
    return dirent;
}

dirent_t directory::read()
{
    dirent_t dirent;
    auto ret = file_->read(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));

	if (ret < 0)
		return INVALID_DIRENT;
    
    if (dirent.inode_n == INVALID_INODE)
        file_->seek(file_->get_curr_pos() - sizeof(dirent_t));
    return dirent;
}

void directory::rewind()
{
    file_->seek(0);
}

directory & directory::operator=(const directory & that)
{
	if (this != &that)
	{
		delete file_;
		file_ = new file(that.file_->inode_n_, that.file_->fs_);
	}
	return *this;
}

int directory::add_entry(uint32_t inode_n, const std::string & filename)
{
    dirent_t dirent = find(filename);

    if (dirent.inode_n != INVALID_INODE)
        return EDIR_FILE_EXISTS;

    file tmp = file(inode_n, file_->fs_);
    inode_t inode;
    auto ret = tmp.get_inode(&inode);

    if (ret < 0)
        return ret;

    auto prev_pos = file_->get_curr_pos();

    do {
        ret = file_->read(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
    } while (dirent.inode_n != INVALID_INODE && ret >= 0);

	dirent.inode_n = inode_n;
	dirent.f_type = inode.f_type;
	strcpy(dirent.name, filename.c_str());

	if (file_->get_curr_pos() >= sizeof(dirent_t))
		file_->seek(file_->get_curr_pos() - sizeof(dirent_t));
	else
		file_->seek(0);

    ret = file_->write(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
	if (ret < 0)
		return ret;
    
    dirent.inode_n = INVALID_INODE;
    dirent.name[0] = '\0';

    ret = file_->write(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
	if (ret < 0)
		return ret;

    file_->seek(prev_pos);

    return 0;
}

int directory::remove_entry(const std::string & filename)
{
    // TODO: MACRO ERROR CHECK ASSERT
    std::size_t deleted_pos;
    std::size_t end_pos;
    auto prev_pos = file_->get_curr_pos();

    dirent_t dirent;

    do {
        file_->read(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
        auto curr_filename = std::string(dirent.name);
        if (curr_filename == filename)
            break;
    } while (dirent.inode_n != INVALID_INODE);

    if (dirent.inode_n == INVALID_INODE)
        return EDIR_FILE_NOT_FOUND;

    deleted_pos = file_->get_curr_pos() - sizeof(dirent_t);

    do {
        file_->read(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
    } while (dirent.inode_n != INVALID_INODE);
    end_pos = file_->get_curr_pos() - sizeof(dirent_t);

    char * buffer = new char[end_pos - deleted_pos];

    file_->seek(deleted_pos + sizeof(dirent_t));
    file_->read(buffer, end_pos - deleted_pos - sizeof(dirent_t));

    dirent.inode_n = -1;
    dirent.name[0] = '\0';

    reinterpret_cast<dirent_t *>(buffer)
        [(end_pos - deleted_pos) / sizeof(dirent_t) - 1] = dirent;

    file_->seek(deleted_pos);
    file_->write(buffer, end_pos - deleted_pos);

    file_->trunc(end_pos);
    delete[] buffer;

    if (prev_pos > deleted_pos)
        file_->seek(0);

    return 0;
}