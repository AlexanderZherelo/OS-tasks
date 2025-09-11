#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <string>

struct employee {
    int num;            
    char name[10];      
    double hours;       
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: Creator <binary_file> <record_count>\n";
        return 1;
    }

    std::string filename = argv[1];
    constexpr char ext[] = ".bin";
    if (filename.size() < sizeof(ext) - 1 ||
        filename.substr(filename.size() - (sizeof(ext) - 1)) != ext)
    {
        filename += ext;
    }

    int count = std::atoi(argv[2]);
    if (count <= 0) {
        std::cerr << "Invalid record count.\n";
        return 1;
    }

    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open file for writing: " << filename << "\n";
        return 1;
    }

    for (int i = 0; i < count; ++i) {
        employee e;
        std::cout << "Record #" << (i + 1) << ":\n";
        std::cout << "  Num: ";
        std::cin >> e.num;
        std::cout << "  Name (max 9 chars): ";
        std::string tmp;
        std::cin >> tmp;
        std::strncpy(e.name, tmp.c_str(), sizeof(e.name) - 1);
        e.name[sizeof(e.name) - 1] = '\0';
        std::cout << "  Hours: ";
        std::cin >> e.hours;

        out.write(reinterpret_cast<const char*>(&e), sizeof(e));
        if (!out) {
            std::cerr << "Write error on record #" << (i + 1) << "\n";
            return 1;
        }
    }

    out.close();
    std::cout << "Binary file \"" << filename
        << "\" created with "
        << count << " records.\n";

    return 0;
}
