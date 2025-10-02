#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <string>

struct employee {
    int num;
    char name[10];
    double hours;
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: Reporter <binary_file> <report_file> <hourly_rate>\n";
        return 1;
    }

    std::string binName = argv[1];
    constexpr char extBin[] = ".bin";
    if (binName.size() < sizeof(extBin) - 1 ||
        binName.substr(binName.size() - (sizeof(extBin) - 1)) != extBin)
    {
        binName += extBin;
    }

    std::string reportName = argv[2];
    if (reportName.size() < sizeof(extBin) - 1 ||
        reportName.substr(reportName.size() - (sizeof(extBin) - 1)) != extBin)
    {
        reportName += extBin;
    }

    double rate = std::atof(argv[3]);
    if (rate <= 0) {
        std::cerr << "Invalid hourly rate.\n";
        return 1;
    }

    std::ifstream in(binName, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open binary file: " << binName << "\n";
        return 1;
    }

    in.seekg(0, std::ios::end);
    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    if (size % sizeof(employee) != 0) {
        std::cerr << "Corrupt binary file.\n";
        return 1;
    }

    int count = static_cast<int>(size / sizeof(employee));
    std::vector<employee> list(count);
    in.read(reinterpret_cast<char*>(list.data()), size);
    in.close();

    std::sort(list.begin(), list.end(),
        [rate](auto const& a, auto const& b) {
            return (a.hours * rate) > (b.hours * rate);
        });

    std::ofstream out(reportName);
    if (!out) {
        std::cerr << "Cannot open report file: " << reportName << "\n";
        return 1;
    }

    out << "Report using file \"" << binName << "\"\n";
    out << "Num, name, hrs, salary\n";
    out << std::fixed << std::setprecision(2);

    for (auto const& e : list) {
        double salary = e.hours * rate;
        out << e.num << ", "
            << e.name << ", "
            << e.hours << ", "
            << salary << "\n";
    }

    out.close();
    std::cout << "Report \"" << reportName << "\" generated.\n";
    return 0;
}
