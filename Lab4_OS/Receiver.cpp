#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <limits>

using namespace std;

#pragma pack(push,1)
struct BufferHeader {
    int capacity;
    int head;
    int tail;
    int count;
};
#pragma pack(pop)

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

    string sendersCountStr;
    int sendersCount;
    cout << "Enter amn of processes Sender: ";
    cin >> sendersCount;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (sendersCount <= 0) {
        cerr << "Incorrect amn of Sender\n";
        return 1;
    }

    // Prepare file
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

    SIZE_T recordSize = 20;
    SIZE_T fileSize = sizeof(BufferHeader) + capacity * recordSize;
    LARGE_INTEGER liSize; liSize.QuadPart = (LONGLONG)fileSize;
    if (!SetFilePointerEx(hFile, liSize, NULL, FILE_BEGIN) || !SetEndOfFile(hFile)) {
        // extend file
    }
    // Create file mapping
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hMap) {
        cerr << "CreateFileMapping failed: " << GetLastError() << "\n";
        CloseHandle(hFile);
        return 1;
    }

    LPVOID view = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view) {
        cerr << "MapViewOfFile failed: " << GetLastError() << "\n";
        CloseHandle(hMap); 
        CloseHandle(hFile);
        return 1;
    }

    BufferHeader* hdr = (BufferHeader*)view;
    hdr->capacity = capacity;
    hdr->head = 0;
    hdr->tail = 0;
    hdr->count = 0;

    // Create synchronization objects (unique names based on filename)
    string safeName;
    {
        safeName = filename;
        for (char& c : safeName) 
            if (c == '\\' || c == '/' || c == ':' || c == ' ') 
                c = '_';
    }
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
        UnmapViewOfFile(view); 
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 1;
    }

    // Launch Sender processes
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

    // Wait for all Senders to be ready
    cout << "Waiting for ready signals from Senders...\n";
    for (int i = 0; i < sendersCount; ++i) {
        DWORD res = WaitForSingleObject(hReadySem, INFINITE);
        if (res != WAIT_OBJECT_0) {
            cerr << "WaitForSingleObject on readySem failed\n";
        }
    }
    cout << "All Senders ready.\n";

    while (true) {
        cout << "Enter command (read / exit): ";
        string cmd;
        getline(cin, cmd);
        if (cmd == "exit") break;
        if (cmd == "read") {
            // Wait until there is a filled slot (blocks if empty)
            DWORD r = WaitForSingleObject(hSemFull, INFINITE);
            if (r != WAIT_OBJECT_0) {
                cerr << "WaitForSingleObject on semFull failed\n";
                continue;
            }
            // Enter critical section
            WaitForSingleObject(hMutex, INFINITE);
            // Re-check header state (not strictly necessary because semFull ensured count>0)
            BufferHeader* h = (BufferHeader*)view;
            if (h->count <= 0) {
                ReleaseMutex(hMutex);
                ReleaseSemaphore(hSemFull, 1, NULL); 
                cout << "Buffer is empty\n";
                continue;
            }
            // Read message at head
            char* base = (char*)view + sizeof(BufferHeader);
            char msg[21]; ZeroMemory(msg, sizeof(msg));
            memcpy(msg, base + h->head * recordSize, recordSize);
            h->head = (h->head + 1) % h->capacity;
            h->count -= 1;
            ReleaseMutex(hMutex);
            ReleaseSemaphore(hSemEmpty, 1, NULL);

            cout << "Message read: \"" << msg << "\"\n";
        }
        else {
            cout << "Unknown team\n";
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
