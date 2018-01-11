#include "file.h"

#include "../../fs/fs.h"
#include "../../errors.h"

#include <string.h>

file::file(uint32_t inode_n, file_system * fs) : fs(fs), inode_n_(inode_n), curr_pos(0)
{
    fs->read_inode(inode_n, &inode);
}

void file::reopen(uint32_t inode_n, file_system * file_sys)
{
    fs = file_sys;
    curr_pos = 0;
    inode_n_ = inode_n;
    fs->read_inode(inode_n, &inode); 
}

int file::read(char * buffer, std::size_t size)
{
    uint32_t block_size_bytes = fs->super_block_.block_size * SECTOR_SIZE;
    uint32_t curr_block = curr_pos / block_size_bytes;
    uint32_t curr_block_offset = curr_pos % block_size_bytes;

    int ret = read_unaligned(curr_block, curr_block_offset, size, buffer);

    if (ret < 0)
        return ret;

    return curr_pos += size;
}

int file::write(const char * buffer, std::size_t size)
{
    uint32_t block_size_bytes = fs->super_block_.block_size * SECTOR_SIZE;
    uint32_t curr_block = curr_pos / block_size_bytes;
    uint32_t curr_block_offset = curr_pos % block_size_bytes;

    int ret = write_unaligned(curr_block, curr_block_offset, size, buffer);

    if (ret < 0)
        return ret;

    return curr_pos += size;
}

int file::trunc(std::size_t new_size)
{
    // TODO: ASSERT MACRO CHECK 

    uint32_t block_size_bytes = fs->super_block_.block_size * SECTOR_SIZE;
    uint32_t free_blocks = new_size / block_size_bytes + (new_size % block_size_bytes != 0);
    // todo: fix curr_pos
    for (uint32_t i = free_blocks; i < INODE_BLOCKS_MAX; ++i)
    {
        if (inode.blocks[i] != 0)
        {
            fs->set_block_status(inode.blocks[i], false);
            inode.blocks[i] = 0;
        }
    }
    uint32_t i;
    if (inode.indirect_block != 0)
    {
        uint32_t * buffer = reinterpret_cast<uint32_t *>(fs->data_buffer_);
        fs->read_block(inode.indirect_block, fs->data_buffer_, fs->super_block_.block_size);

        if (free_blocks < INODE_BLOCKS_MAX)
            i = 0;
        else
            i = free_blocks = INODE_BLOCKS_MAX;

        for (; i < block_size_bytes; ++i)
        {
            if (buffer[i] != 0)
            {
                fs->set_block_status(buffer[i], false);
                buffer[i] = 0;
            }
        }
        fs->write_block(inode.indirect_block, fs->data_buffer_, fs->super_block_.block_size);
        if (free_blocks <= INODE_BLOCKS_MAX)
        {
            fs->set_block_status(inode.indirect_block, false);
            inode.indirect_block = 0;
        }
    }
    if (inode.double_indirect_block != 0)
    {
        uint32_t * buffer = reinterpret_cast<uint32_t *>(fs->data_buffer_);
        uint32_t * second_buffer = new uint32_t[block_size_bytes / sizeof(uint32_t)];

        fs->read_block(inode.double_indirect_block, fs->data_buffer_, fs->super_block_.block_size);

        i = (free_blocks - INODE_BLOCKS_MAX - (block_size_bytes / sizeof(uint32_t))) / (block_size_bytes / sizeof(uint32_t));
        if (free_blocks < INODE_BLOCKS_MAX + (block_size_bytes / sizeof(uint32_t)))
            i = 0;
        else
            i = (free_blocks - INODE_BLOCKS_MAX - (block_size_bytes / sizeof(uint32_t))) / (block_size_bytes / sizeof(uint32_t));
        
        uint32_t j;
        if (i == 0)
            j = 0;
        else
            j = (free_blocks - INODE_BLOCKS_MAX - (block_size_bytes / sizeof(uint32_t))) % (block_size_bytes / sizeof(uint32_t));

        for (; i < block_size_bytes; ++i)
        {
            if (buffer[i] != 0)
            {
                fs->read_block(buffer[i], reinterpret_cast<char *>(second_buffer), fs->super_block_.block_size);
                for (; j < block_size_bytes; ++j)
                {
                    if (second_buffer[j] != 0)
                    {
                        fs->set_block_status(second_buffer[j], false);
                        second_buffer[j] = 0;
                    }
                }
                fs->write_block(buffer[i], reinterpret_cast<char *>(second_buffer), fs->super_block_.block_size);

                fs->set_block_status(buffer[i], false);
                buffer[i] = 0;
                j = 0;
            }
        }

        fs->set_block_status(inode.double_indirect_block, false);
        inode.double_indirect_block = 0;
        delete[] second_buffer;
    }

    return 0;
}

int file::seek(std::size_t pos)
{
    return curr_pos = pos;
}

int file::read_unaligned(uint32_t start_block, std::size_t offset, std::size_t obj_size, void * buffer)
{
    uint32_t block_size_bytes = fs->super_block_.block_size * SECTOR_SIZE;
    int ret;
    std::size_t obj_pos = 0;
    std::size_t copy_size;
    uint32_t i = start_block;
    uint32_t curr_sector;

    while (obj_pos < obj_size)
    {
        ret = get_sector(i, &curr_sector);
        if (ret < 0)
            return ret;
        if (curr_sector == 0)
            return EFIL_INVALID_SECTOR;

        ret = fs->read_block(curr_sector, fs->data_buffer_, fs->super_block_.block_size);
        if (ret < 0)
            return ret;

        copy_size = ((obj_size - obj_pos < block_size_bytes - offset) ? obj_size - obj_pos : block_size_bytes - offset); 
        memcpy(reinterpret_cast<char *>(buffer) + obj_pos, fs->data_buffer_ + offset, copy_size);
        obj_pos += copy_size;
        offset = 0;
        ++i;
    }
    return 0;
}

int file::write_unaligned(uint32_t start_block, std::size_t offset, std::size_t obj_size, const void * buffer)
{
    uint32_t block_size_bytes = fs->super_block_.block_size * SECTOR_SIZE;
    int ret;
    std::size_t obj_pos = 0;
    std::size_t copy_size;
    uint32_t i = start_block;
    uint32_t curr_sector;

    while (obj_pos < obj_size)
    {
        ret = get_sector(i, &curr_sector, true);
        if (ret < 0)
            return ret;
        if (curr_sector == 0)
            return EFIL_INVALID_SECTOR;

        copy_size = ((obj_size - obj_pos < block_size_bytes - offset) ? obj_size - obj_pos : block_size_bytes - offset); 

        if (copy_size == block_size_bytes)
        {
            ret = fs->write_block(curr_sector, reinterpret_cast<const char *>(buffer) + obj_pos, fs->super_block_.block_size);

            if (ret < 0)
                return ret;

            obj_pos += copy_size;
            ++i;
            continue;
        }

        ret = fs->read_block(curr_sector, fs->data_buffer_, fs->super_block_.block_size);
        if (ret < 0)
            return ret;
        
        memcpy(fs->data_buffer_ + offset, reinterpret_cast<const char *>(buffer) + obj_pos, copy_size);
        obj_pos += copy_size;
        offset = 0;
        ++i;
    }
    return 0;
}

int file::get_sector(uint32_t i, uint32_t * sector_out, bool do_allocate)
{
    int ret;
    uint32_t block_size_bytes = fs->super_block_.block_size * SECTOR_SIZE;
    auto indirect_max = block_size_bytes / sizeof(uint32_t);
    auto double_indirect_max = indirect_max * indirect_max;

    if (do_allocate)
        allocate_block(i);

    fs->read_inode(inode_n_, &inode);
    // direct
    if (i < INODE_BLOCKS_MAX)
    {
        (*sector_out) = inode.blocks[i];
        if (inode.blocks[i] == 0)
            return EFIL_INVALID_SECTOR;
    }
    // indirect
    else if (i < INODE_BLOCKS_MAX + indirect_max)
    {
        if (inode.indirect_block == 0)
            return EFIL_INVALID_SECTOR;
        ret = fs->read_object(inode.indirect_block, (i - INODE_BLOCKS_MAX) * sizeof(uint32_t), sizeof(uint32_t), sector_out);
        return ret;
    }
    // double indirect
    else if (i < INODE_BLOCKS_MAX + indirect_max + double_indirect_max)
    {
        uint32_t index_level_1 = (i - INODE_BLOCKS_MAX - indirect_max) / block_size_bytes;
        uint32_t index_level_2 = (i - INODE_BLOCKS_MAX - indirect_max) % block_size_bytes;
        uint32_t pointer;

        if (inode.double_indirect_block == 0)
            return EFIL_INVALID_SECTOR;

        ret = fs->read_object(inode.double_indirect_block, index_level_1 * sizeof(uint32_t), sizeof(uint32_t), &pointer);
        if (ret < 0)
            return ret;
        if (pointer == 0)
            return EFIL_INVALID_SECTOR;
        
        ret = fs->read_object(pointer, index_level_2 * sizeof(uint32_t), sizeof(uint32_t), sector_out);
        return ret;
    }
    // out of bounds for sure
    else
        return EFIL_INVALID_POS;
    return 0;
}

void file::allocate_block(uint32_t block_index)
{
    int ret;
    uint32_t block_size_bytes = fs->super_block_.block_size * SECTOR_SIZE;
    auto indirect_max = block_size_bytes / sizeof(uint32_t);
    auto double_indirect_max = indirect_max * indirect_max;
    uint32_t temp;

    uint32_t free_block = fs->get_free_block();

    fs->read_inode(inode_n_, &inode);
    if (block_index < INODE_BLOCKS_MAX)
    {
        if (inode.blocks[block_index] == 0)
        {
            fs->set_block_status(free_block, true);
            inode.blocks[block_index] = free_block;
            return;
        }
    }
    // indirect
    else if (block_index < INODE_BLOCKS_MAX + indirect_max)
    {
        if (inode.indirect_block == 0)
        {
            fs->set_block_status(free_block, true);
            inode.indirect_block = free_block;
            free_block = fs->get_free_block();
        }
        ret = fs->read_object(inode.indirect_block, (block_index - INODE_BLOCKS_MAX) * sizeof(uint32_t), sizeof(uint32_t), &temp);
        if (ret < 0)
            return;
        if (temp == 0)
        {
            fs->set_block_status(free_block, true);
            fs->write_object(inode.indirect_block, (block_index - INODE_BLOCKS_MAX) * sizeof(uint32_t), sizeof(uint32_t), &free_block);
            return;
        }
    }
    // double indirect
    else if (block_index < INODE_BLOCKS_MAX + indirect_max + double_indirect_max)
    {
        uint32_t index_level_1 = (block_index - INODE_BLOCKS_MAX - indirect_max) / block_size_bytes;
        uint32_t index_level_2 = (block_index - INODE_BLOCKS_MAX - indirect_max) % block_size_bytes;
        uint32_t pointer;

        if (inode.double_indirect_block == 0)
        {
            fs->set_block_status(free_block, true);
            inode.double_indirect_block = free_block;
            free_block = fs->get_free_block();
        }

        ret = fs->read_object(inode.double_indirect_block, index_level_1 * sizeof(uint32_t), sizeof(uint32_t), &pointer);
        if (ret < 0)
            return;
        if (pointer == 0)
        {
            fs->set_block_status(free_block, true);
            ret = fs->write_object(inode.double_indirect_block, index_level_1 * sizeof(uint32_t), sizeof(uint32_t), &free_block);
            if (ret < 0)
                return;
            pointer = free_block;
            free_block = fs->get_free_block();
        }

        ret = fs->read_object(pointer, index_level_2 * sizeof(uint32_t), sizeof(uint32_t), &temp);
        if (ret < 0)
            return;
        if (temp == 0)
        {
            fs->set_block_status(free_block, true);
            ret = fs->write_object(pointer, index_level_2 * sizeof(uint32_t), sizeof(uint32_t), &free_block);
            return;
        }
    }
    else
        return;
}