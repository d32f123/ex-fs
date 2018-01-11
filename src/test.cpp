#include <iostream>
#include <string>
#include <vector>
#include <limits>

#include "./fs/fs.h"

#define PROMPT_STR      ("> ")

using namespace std;

extern std::vector<std::string> split(const std::string &text, char sep);

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

void do_ls(file_system * fs, const std::string & curr_dir = "/", int depth = 1)
{
    auto root_id = fs->opendir(curr_dir);
    do {
        auto dirent = fs->readdir(root_id);
        //auto file_id = fs->open(curr_dir + "/" + dirent.name);
        /// ASDASDASDSAD
        for (int i = 0; i < depth; ++i)
            cout << '\t';
        cout << dirent.name;
    } while (true);
}

int exec_command(const std::vector<std::string> & args, file_system * fs)
{
    if (args[0] == "ls")
    {
        do_ls(fs);
    }
    return 0;
}

int main()
{
    file_system fs;

    cout << "Welcome!" << endl;
    std::vector<std::string> first_options = {"Create a new fs", "Open an existing fs"};

    int choice;
    do {
        choice = prompt_user(first_options);
        switch (choice) {
            case 1: create_fs(&fs);
            break;
            case 2: load_fs(&fs);
            break;
        }
        
        while (true)
        {
            std::string full_command;
            std::getline(cin, full_command);

            auto args = split(full_command, ' ');
            if (args[0] == "exit")
                break;
            exec_command(args, &fs);
        }
    } while(choice != 0);


    return 0; 
}

int create_fs(file_system * fs)
{
    cout << "Enter filename: ";
    std::string filename;
    cin >> filename;

    auto inode_count = input_user<int>("How many inodes: ");
    auto disk_size = input_user<int>("Disk size: ");
    auto block_size = input_user<int>("Block size: ");

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