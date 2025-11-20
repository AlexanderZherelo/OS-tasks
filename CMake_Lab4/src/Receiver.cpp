#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <limits>
#include "header.h"

using namespace std;

string MakeObjName(const string& base, const string& file) {
    string name = base + "_" + file;
    for (char& c : name) if (c == '\\' || c == '/' || c == ':' || c == ' ') c = '_';
    return name;
}

int main() {
    string filename;
    int capacity;
    cout << "Enter bin name: ";
    getline(cin, filename);
    cout << "Enter amn of records in bin: ";
    cin >> capacity;
    if (capacity <= 0) {
        cerr << "Incorrect capacity\n";
        return 1;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    int sendersCount;
    cout << "Enter amn of processes Sender: ";
    cin >> sendersCount;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (sendersCount <= 0) {
        cerr << "Incorrect amn of Sender\n";
        return 1;
    }

    HANDLE hFile = CreateFileA(filename.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        cerr << "CreateFile failed: " << GetLastError() << "\n";
        return 1;
    }

    SIZE_T fileSize = sizeof(BufferHeader) + capacity * RECORD_SIZE;
    LARGE_INTEGER liSize; liSize.QuadPart = (LONGLONG)fileSize;
    SetFilePointerEx(hFile, liSize, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);

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

    // Initialize header
    BufferHeader* hdr = GetHeader(view);
    hdr->capacity = capacity;
    hdr->head = hdr->tail = hdr->count = 0;

    string safeName = MakeSafeName(filename);
    string mutexName = MakeObjName("Global\\MsgMutex", safeName);
    string semEmptyName = MakeObjName("Global\\MsgEmpty", safeName);
    string semFullName = MakeObjName("Global\\MsgFull", safeName);
    string readySemName = MakeObjName("Global\\ReadySem", safeName);

    HANDLE hMutex = CreateMutexA(NULL, FALSE, mutexName.c_str());
    HANDLE hSemEmpty = CreateSemaphoreA(NULL, capacity, capacity, semEmptyName.c_str());
    HANDLE hSemFull = CreateSemaphoreA(NULL, 0, capacity, semFullName.c_str());
    HANDLE hReadySem = CreateSemaphoreA(NULL, 0, sendersCount, readySemName.c_str());

    if (!hMutex || !hSemEmpty || !hSemFull || !hReadySem) {
        cerr << "Create sync objects failed: " << GetLastError() << "\n";
        UnmapViewOfFile(view); CloseHandle(hMap); CloseHandle(hFile);
        return 1;
    }

    // Launch Sender processes (create new consoles to avoid mixed stdin)
    vector<HANDLE> procHandles;
    for (int i = 0; i < sendersCount; ++i) {
        string cmd = "Sender.exe \"" + filename + "\"";
        STARTUPINFOA si = { 0 }; si.cb = sizeof(si);
        PROCESS_INFORMATION pi = { 0 };
        BOOL ok = CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
        if (!ok) {
            cerr << "CreateProcess failed: " << GetLastError() << "\n";
        }
        else {
            procHandles.push_back(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    cout << "Waiting for ready signals from Senders...\n";
    for (int i = 0; i < sendersCount; ++i) {
        WaitForSingleObject(hReadySem, INFINITE);
    }
    cout << "All Senders ready.\n";

    while (true) {
        cout << "Enter command (read / exit): ";
        string cmd;
        getline(cin, cmd);
        if (cmd == "exit") break;
        if (cmd == "read") {
            WaitForSingleObject(hSemFull, INFINITE);
            WaitForSingleObject(hMutex, INFINITE);

            if (hdr->count <= 0) {
                ReleaseMutex(hMutex);
                ReleaseSemaphore(hSemFull, 1, NULL);
                cout << "Buffer is empty\n";
                continue;
            }

            char out[RECORD_SIZE + 1];
            RB_Read(view, out, RECORD_SIZE + 1);
            ReleaseMutex(hMutex);
            ReleaseSemaphore(hSemEmpty, 1, NULL);

            cout << "Message read: \"" << out << "\"\n";
        }
        else {
            cout << "Unknown command\n";
        }
    }

    cout << "Receiver is terminating. Waiting for senders to complete....\n";
    for (HANDLE ph : procHandles) {
        WaitForSingleObject(ph, 2000);
        CloseHandle(ph);
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
