#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <iomanip>

#include "./fs/fs.h"

#define PROMPT_STR      ("> ")

using namespace std;

extern std::vector<std::string> split(const std::string &text, char sep);

struct hex_char_struct
{
	unsigned char c;
	hex_char_struct(unsigned char _c) : c(_c) { }
};

inline std::ostream& operator<<(std::ostream& o, const hex_char_struct& hs)
{
	return (o << setw(2) << setfill('0') << hex << static_cast<int>(hs.c));
}

inline hex_char_struct hex(unsigned char _c)
{
	return { _c };
}

int create_fs(file_system * fs);
int load_fs(file_system * fs);

template<typename T>
T input_user(const std::string & prompt = "Enter: ")
{
    T ret;
    while (true)
    {
        cout << prompt;
        if (cin >> ret)
            return ret;
        cout << "Invalid input. Try again" << endl;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

int prompt_user(const std::vector<std::string> & options)
{
    for (std::size_t i = 0; i < options.size(); ++i)
    {
        cout << i + 1 << ") " << options[i] << endl;
    }
    cout << "0) Exit" << endl;
    
    int ret;
    while (true) 
    {
        cout << PROMPT_STR;
        if (cin >> ret && ret >= 0 && static_cast<std::size_t>(ret) <= options.size())
            break;
        else
        {
            cout << "Please enter an integer in range [0; " << options.size() << "]\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }
    return ret;
}

void do_ls(file_system * fs, const std::string & curr_dir, int depth = 0)
{
    const auto root_id = fs->opendir(curr_dir);
	dirent_t dirent;
    while ((dirent = fs->readdir(root_id)).inode_n != INODE_INVALID) 
	{
        for (auto i = 0; i < depth; ++i)
            cout << '\t';
        cout << dirent.name << ":" << static_cast<int>(dirent.f_type) << ":" << dirent.inode_n << endl;
		if (dirent.f_type == file_type::dir
			&& strncmp(dirent.name, ".", DIRENT_NAME_MAX) != 0 && strncmp(dirent.name, "..", DIRENT_NAME_MAX) != 0)
		{
			do_ls(fs, file_system::concat_paths(curr_dir, dirent.name), depth + 1);
		}
    }
	fs->closedir(root_id);
}

void do_create(file_system * fs, const std::string & filename)
{
	cout << fs->create(filename) << endl;
}

void do_mkdir(file_system * fs, const std::string & dirname)
{
	cout << fs->mkdir(dirname) << endl;
}

void do_cd(file_system * fs, const std::string & dirname)
{
	cout << fs->cd(dirname) << endl;
}

void do_link(file_system * fs, const std::string & orig, const std::string & new_file)
{
	cout << fs->link(orig, new_file) << endl;
}

void do_unlink(file_system * fs, const std::string & file)
{
	cout << fs->unlink(file) << endl;
}

void do_rmdir(file_system * fs, const std::string & file)
{
	cout << fs->rmdir(file) << endl;
}

void do_opendir(file_system * fs, const std::string & file)
{
	cout << fs->opendir(file) << endl;
}

void do_closedir(file_system * fs, did_t did)
{
	cout << fs->closedir(did) << endl;
}

void do_readdir(file_system * fs, did_t did)
{
	cout << fs->readdir(did) << endl;
}

void do_open(file_system * fs, const std::string & file)
{
	cout << fs->open(file) << endl;
}

void do_close(file_system * fs, fid_t fid)
{
	cout << fs->close(fid) << endl;
}

void do_read(file_system * fs, fid_t fid, const std::size_t size)
{
	char * buffer = new char[size];
	cout << fs->read(fid, buffer, size) << endl;
	for (std::size_t i = 0; i < size; ++i)
	{
		if (i % 16 == 0)
			cout << endl << setw(4) << setfill('0') << hex << static_cast<int>(i << 4) << dec << ": ";
		cout << buffer[i];
	}
}

void do_write(file_system * fs, fid_t fid, const std::string & input)
{
	cout << fs->write(fid, input.c_str(), input.size()) << endl;
}

void do_trunc(file_system * fs, fid_t fid, std::size_t new_size)
{
	cout << fs->trunc(fid, new_size) << endl;
}

void do_seek(file_system * fs, fid_t fid, std::size_t new_pos)
{
	cout << fs->seek(fid, new_pos) << endl;
}

void do_rewind(file_system * fs, did_t did)
{
	cout << fs->rewind_dir(did) << endl;
}

int exec_command(const std::vector<std::string> & args, file_system * fs)
{
    if (args[0] == "ls")
    {
		if (args.size() > 1)
			do_ls(fs, args[1]);
		else
			do_ls(fs, "");
    }
	if (args[0] == "create" && args.size() > 1)
	{
		do_create(fs, args[1]);
	}
	if (args[0] == "mkdir" && args.size() > 1)
	{
		do_mkdir(fs, args[1]);
	}
	if (args[0] == "cd" && args.size() > 1)
	{
		do_cd(fs, args[1]);
	}
	if (args[0] == "link" && args.size() > 2)
	{
		do_link(fs, args[1], args[2]);
	}
	if (args[0] == "unlink" && args.size() > 1)
	{
		do_unlink(fs, args[1]);
	}
	if (args[0] == "rmdir" && args.size() > 1)
	{
		do_rmdir(fs, args[1]);
	}
	if (args[0] == "opendir" && args.size() > 1)
	{
		do_opendir(fs, args[1]);
	}
	if (args[0] == "closedir" && args.size() > 1)
	{
		do_closedir(fs, stoul(args[1]));
	}
	if (args[0] == "readdir" && args.size() > 1)
	{
		do_readdir(fs, stoul(args[1]));
	}
	if (args[0] == "rewind" && args.size() > 1)
	{
		do_rewind(fs, stoul(args[1]));
	}
	if (args[0] == "open" && args.size() > 1)
	{
		do_open(fs, args[1]);
	}
	if (args[0] == "close" && args.size() > 1)
	{
		do_close(fs, stoul(args[1]));
	}
	if (args[0] == "read" && args.size() > 2)
	{
		do_read(fs, stoul(args[1]), stoul(args[2]));
	}
	if (args[0] == "write" && args.size() > 2)
	{
		do_write(fs, stoul(args[1]), args[2]);
	}
	if (args[0] == "trunc" && args.size() > 2)
	{
		do_trunc(fs, stoul(args[1]), stoul(args[2]));
	}
	if (args[0] == "seek" && args.size() > 2)
	{
		do_seek(fs, stoul(args[1]), stoul(args[2]));
	}
    return 0;
}

int main()
{
    file_system fs;

    cout << "Welcome!" << endl;
    const std::vector<std::string> first_options = {"Create a new fs", "Open an existing fs"};

	do {
        const auto choice = prompt_user(first_options);
		if (choice == 0)
			break;
        switch (choice) {
            case 1: create_fs(&fs);
            break;
            case 2: load_fs(&fs);
            break;
			default: continue;
        }
		cin.ignore(numeric_limits<std::streamsize>::max(), '\n');
        while (true)
        {
            std::string full_command;
			cout << PROMPT_STR;
            std::getline(cin, full_command);

            auto args = split(full_command, ' ');
            if (args[0] == "exit")
                break;
            exec_command(args, &fs);
        }
    } while(true);


    return 0; 
}

int create_fs(file_system * fs)
{
    cout << "Enter filename: ";
    std::string filename;
    cin >> filename;

    const auto inode_count = input_user<int>("How many inodes: ");
    const auto disk_size = input_user<int>("Disk size: ");
    const auto block_size = input_user<int>("Block size: ");

    fs->init(filename, inode_count, disk_size, block_size);

    return 0;
}

int load_fs(file_system * fs)
{
    cout << "Enter filename: ";
    std::string filename;
    cin >> filename;

    fs->load(filename);

    return 0;
}