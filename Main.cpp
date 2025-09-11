#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

void LaunchAndWait(const std::string& cmdLine) {
    std::vector<char> cmd(cmdLine.begin(), cmdLine.end());
    cmd.push_back('\0');
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmd.data(),
        NULL, NULL, FALSE, 0,
        NULL, NULL, &si, &pi))
    {
        std::cerr << "Failed to launch: " << cmdLine << "\n";
        ExitProcess(1);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

int main() {
    std::string binName;
    int recordCount;
    std::cout << "Enter binary file name: ";
    std::getline(std::cin, binName);

    constexpr char extBin[] = ".bin";
    if (binName.size() < sizeof(extBin) - 1 ||
        binName.substr(binName.size() - (sizeof(extBin) - 1)) != extBin)
    {
        binName += extBin;
    }

    std::cout << "Enter number of records: ";
    std::cin >> recordCount;
    std::cin.get(); 

    {
        std::ostringstream oss;
        oss << "Creator.exe \"" << binName << "\" " << recordCount;
        LaunchAndWait(oss.str());
    }

    struct employee { int num; char name[10]; double hours; };
    {
        std::ifstream in(binName, std::ios::binary);
        if (!in) {
            std::cerr << "Cannot open binary file: " << binName << "\n";
            return 1;
        }
        std::cout << "\n--- Contents of " << binName << " ---\n";
        while (in.peek() != EOF) {
            employee e;
            in.read(reinterpret_cast<char*>(&e), sizeof(e));
            if (!in) break;
            std::cout << "Num: " << e.num
                << ", Name: " << e.name
                << ", Hours: " << e.hours << "\n";
        }
    }

    std::string reportName;
    double rate;
    std::cout << "\nEnter report file name: ";
    std::getline(std::cin, reportName);

    if (reportName.size() < sizeof(extBin) - 1 ||
        reportName.substr(reportName.size() - (sizeof(extBin) - 1)) != extBin)
    {
        reportName += extBin;
    }

    std::cout << "Enter hourly rate: ";
    std::cin >> rate;
    std::cin.get();

    {
        std::ostringstream oss;
        oss << "Reporter.exe \""
            << binName << "\" \""
            << reportName << "\" "
            << std::fixed << std::setprecision(2)
            << rate;
        LaunchAndWait(oss.str());
    }

    {
        std::ifstream in(reportName);
        if (!in) {
            std::cerr << "Cannot open report file: " << reportName << "\n";
            return 1;
        }
        std::cout << "\n--- Report: " << reportName << " ---\n";
        std::string line;
        while (std::getline(in, line)) {
            std::cout << line << "\n";
        }
    }
    system("pause");
    return 0;
}
