#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include "header.h"

using namespace std;

string MakeObjName(const string& base, const string& file) {
    string name = base + "_" + file;
    for (char& c : name) if (c == '\\' || c == '/' || c == ':' || c == ' ') c = '_';
    return name;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: Sender.exe <filename>\n";
        return 1;
    }
    string filename = argv[1];

    HANDLE hFile = CreateFileA(filename.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        cerr << "Cannot open file: " << GetLastError() << "\n";
        return 1;
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hMap) {
        cerr << "CreateFileMapping failed: " << GetLastError() << "\n";
        CloseHandle(hFile);
        return 1;
    }

    LPVOID view = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view) {
        cerr << "MapViewOfFile failed: " << GetLastError() << "\n";
        CloseHandle(hMap); CloseHandle(hFile);
        return 1;
    }

    string safeName = MakeSafeName(filename);
    string mutexName = MakeObjName("Global\\MsgMutex", safeName);
    string semEmptyName = MakeObjName("Global\\MsgEmpty", safeName);
    string semFullName = MakeObjName("Global\\MsgFull", safeName);
    string readySemName = MakeObjName("Global\\ReadySem", safeName);

    HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, mutexName.c_str());
    HANDLE hSemEmpty = OpenSemaphoreA(SEMAPHORE_MODIFY_STATE | SYNCHRONIZE, FALSE, semEmptyName.c_str());
    HANDLE hSemFull = OpenSemaphoreA(SEMAPHORE_MODIFY_STATE | SYNCHRONIZE, FALSE, semFullName.c_str());
    HANDLE hReadySem = OpenSemaphoreA(SEMAPHORE_MODIFY_STATE | SYNCHRONIZE, FALSE, readySemName.c_str());

    if (!hMutex || !hSemEmpty || !hSemFull || !hReadySem) {
        cerr << "Open sync objects failed: " << GetLastError() << "\n";
        UnmapViewOfFile(view); CloseHandle(hMap); CloseHandle(hFile);
        return 1;
    }

    ReleaseSemaphore(hReadySem, 1, NULL);

    while (true) {
        cout << "Enter command (send / exit): ";
        string cmd;
        getline(cin, cmd);
        if (cmd == "exit") break;
        if (cmd == "send") {
            cout << "Enter your message text (max 20 characters): ";
            string text;
            getline(cin, text);
            if ((int)text.size() >= (int)RECORD_SIZE) {
                cout << "The message must be less than 20 characters.\n";
                continue;
            }

            WaitForSingleObject(hSemEmpty, INFINITE);
            WaitForSingleObject(hMutex, INFINITE);

            RB_Write(view, text.c_str(), text.size());

            ReleaseMutex(hMutex);
            ReleaseSemaphore(hSemFull, 1, NULL);

            cout << "Message sent\n";
        }
        else {
            cout << "Unknown command\n";
        }
    }

    CloseHandle(hMutex);
    CloseHandle(hSemEmpty);
    CloseHandle(hSemFull);
    CloseHandle(hReadySem);

    UnmapViewOfFile(view);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return 0;
}
