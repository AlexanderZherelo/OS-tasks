#include <windows.h>
#include <iostream>
#include <string>

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
    for (char& c : name) if (c == '\\' || c == '/' || c == ':' || c == ' ') 
        c = '_';
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

    // Prepare sync object names same as Receiver
    string safeName = filename;
    for (char& c : safeName) if (c == '\\' || c == '/' || c == ':' || c == ' ') 
        c = '_';
    string mutexName = MakeObjName("Global\\MsgMutex", safeName);
    string semEmptyName = MakeObjName("Global\\MsgEmpty", safeName);
    string semFullName = MakeObjName("Global\\MsgFull", safeName);
    string readySemName = MakeObjName("Global\\ReadySem", safeName);

    // Open existing sync objects (Receiver created them)
    HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, mutexName.c_str());
    HANDLE hSemEmpty = OpenSemaphoreA(SEMAPHORE_MODIFY_STATE | SYNCHRONIZE, FALSE, semEmptyName.c_str());
    HANDLE hSemFull = OpenSemaphoreA(SEMAPHORE_MODIFY_STATE | SYNCHRONIZE, FALSE, semFullName.c_str());
    HANDLE hReadySem = OpenSemaphoreA(SEMAPHORE_MODIFY_STATE | SYNCHRONIZE, FALSE, readySemName.c_str());

    if (!hMutex || !hSemEmpty || !hSemFull || !hReadySem) {
        cerr << "Open sync objects failed: " << GetLastError() << "\n";
        UnmapViewOfFile(view); 
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 1;
    }

    // Signal readiness: increment Receiver's ready semaphore by 1
    ReleaseSemaphore(hReadySem, 1, NULL);

    BufferHeader* hdr = (BufferHeader*)view;
    SIZE_T recordSize = 20;

    while (true) {
        cout << "Enter command (send / exit): ";
        string cmd;
        getline(cin, cmd);
        if (cmd == "exit") 
            break;
        if (cmd == "send") {
            cout << "Enter your message text (max 20 characters): ";
            string text;
            getline(cin, text);
            if ((int)text.size() >= 20) {
                cout << "The message must be less than 20 characters.\n";
                continue;
            }
            // Wait for free slot (blocks if buffer full)
            DWORD r = WaitForSingleObject(hSemEmpty, INFINITE);
            if (r != WAIT_OBJECT_0) {
                cerr << "WaitForSingleObject on semEmpty failed\n";
                continue;
            }
            WaitForSingleObject(hMutex, INFINITE);
            char* base = (char*)view + sizeof(BufferHeader);
            char buf[20]; 
            ZeroMemory(buf, sizeof(buf));
            memcpy(buf, text.c_str(), text.size());
            memcpy(base + hdr->tail * recordSize, buf, recordSize);
            hdr->tail = (hdr->tail + 1) % hdr->capacity;
            hdr->count += 1;
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
