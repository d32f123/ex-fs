#include <iostream>
#include <string>

#include "./fs/fs.h"

using namespace std;

int main()
{
    file_system fs;

    cout << "Hey world!" << endl;

    // 1. Create a file system
    string filename = "hey";

    fs.init(filename, 10, 32, 2);
    fs.load(filename);

    cout << fs.get_super_block();

    cout << "-----\n";
    cout << "Inode map:\n";
    cout << (*fs.get_inode_map());
    cout << "Space map:\n";
    cout << (*fs.get_space_map());

    return 0; 
}