#include "dir.h"

#include "../../errors.h"

#include <string.h>

directory::directory(uint32_t inode_n, file_system * fs) : file_(new file(inode_n, fs))
{
    if (file_->inode.f_type != file_type::dir)
        throw std::string("Not a directory");
}

directory::directory(file & dir_file) : file_(&dir_file)
{
}

void directory::reopen(uint32_t inode_n, file_system * fs)
{
    file_->reopen(inode_n, fs);
    if (file_->inode.f_type != file_type::dir)
        throw std::string("Not a directory");
}

dirent_t directory::find(std::string & filename)
{
    int prev_pos = file_->get_curr_pos();
    file_->seek(0);
    
    dirent_t dirent;
    do {
        file_->read(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
        auto curr_filename = std::string(dirent.name);
        if (curr_filename == filename)
        {
            file_->seek(prev_pos);
            return dirent;
        }
    } while (dirent.inode_n != INODE_INVALID);
    
    dirent.inode_n = INODE_INVALID;
    dirent.name[0] = '\0';

    file_->seek(prev_pos);
    return dirent;
}

dirent_t directory::read()
{
    dirent_t dirent;
    file_->read(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
    
    if (dirent.inode_n == INODE_INVALID)
        file_->seek(file_->get_curr_pos() - sizeof(dirent_t));
    return dirent;
}

void directory::rewind()
{
    file_->seek(0);
}

int directory::add_entry(uint32_t inode_n, std::string & filename)
{
    dirent_t dirent = find(filename);

    if (dirent.inode_n != INODE_INVALID)
        return EDIR_FILE_EXISTS;

    dirent.inode_n = inode_n;
    strcpy(dirent.name, filename.c_str());

    auto prev_pos = file_->get_curr_pos();

    do {
        file_->read(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
    } while (dirent.inode_n != INODE_INVALID);

    file_->seek(file_->get_curr_pos() - sizeof(dirent_t));

    file_->write(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
    
    dirent.inode_n = INODE_INVALID;
    dirent.name[0] = '\0';

    file_->write(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));

    file_->seek(prev_pos);

    return 0;
}

int directory::remove_entry(std::string & filename)
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
    } while (dirent.inode_n != INODE_INVALID);

    if (dirent.inode_n == INODE_INVALID)
        return EDIR_FILE_NOT_FOUND;

    deleted_pos = file_->get_curr_pos() - sizeof(dirent_t);

    do {
        file_->read(reinterpret_cast<char *>(&dirent), sizeof(dirent_t));
    } while (dirent.inode_n != INODE_INVALID);
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