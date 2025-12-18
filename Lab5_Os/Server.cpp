#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <set>
#include <cstring>

using namespace std;

#pragma pack(push,1)
struct employee {
    int num;
    char name[10];
    double hours;
};
#pragma pack(pop)

struct Request {
    int type; // 1=READ,2=WRITE,3=RELEASE,4=EXIT
    int id;
    employee emp;
};

struct Response {
    int status; // 0 OK, <0 error
    employee emp;
};

class RWLock {
    mutex m;
    condition_variable cv;
    int readers = 0;
    bool writer = false;
public:
    void lock_read() {
        unique_lock<mutex> lk(m);
        cv.wait(lk, [&]() { return !writer; });
        ++readers;
    }
    void unlock_read() {
        unique_lock<mutex> lk(m);
        if (--readers == 0) cv.notify_all();
    }
    void lock_write() {
        unique_lock<mutex> lk(m);
        cv.wait(lk, [&]() { return !writer && readers == 0; });
        writer = true;
    }
    void unlock_write() {
        unique_lock<mutex> lk(m);
        writer = false;
        cv.notify_all();
    }
};

int find_record_index(const vector<employee>& vec, int id) {
    for (size_t i = 0; i < vec.size(); ++i) if (vec[i].num == id) return (int)i;
    return -1;
}

void print_file(const vector<employee>& vec) {
    cout << "Current file contents:\n";
    for (const auto& e : vec) {
        size_t len = strnlen_s(e.name, sizeof(e.name));
        cout << "ID=" << e.num << " Name=" << string(e.name, len)
            << " Hours=" << e.hours << "\n";
    }
}

// Обработчик одного клиента 
void ClientHandler(HANDLE hPipe, vector<employee>& employees, vector<RWLock>& locks, const string& filename) {
    set<int> readLocksHeld;
    set<int> writeLocksHeld;

    auto unlock_all = [&]() {
        for (int idx : readLocksHeld) {
            if (idx >= 0 && idx < (int)locks.size()) locks[idx].unlock_read();
        }
        for (int idx : writeLocksHeld) {
            if (idx >= 0 && idx < (int)locks.size()) locks[idx].unlock_write();
        }
        readLocksHeld.clear();
        writeLocksHeld.clear();
        };

    bool running = true;
    while (running) {
        Request req;
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(hPipe, &req, sizeof(req), &bytesRead, NULL);
        if (!ok || bytesRead == 0) {
            cerr << "ReadFile failed or client disconnected, err=" << GetLastError() << "\n";
            break;
        }

        Response resp;
        resp.status = 0;
        int idx = find_record_index(employees, req.id);

        if (req.type == 1) { // READ
            if (idx < 0) {
                resp.status = -1;
            }
            else {
                locks[idx].lock_read();
                readLocksHeld.insert(idx);
                resp.emp = employees[idx];
                resp.status = 0;
            }
            DWORD bytesWritten = 0;
            WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
        }
        else if (req.type == 2) { // WRITE
            if (idx < 0) {
                resp.status = -1;
                DWORD bytesWritten = 0;
                WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
            }
            else {
                locks[idx].lock_write();
                writeLocksHeld.insert(idx);
                resp.emp = employees[idx];
                resp.status = 0;
                DWORD bytesWritten = 0;
                WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);

                // Ожидаем новые данные от клиента (Request с emp)
                Request req2;
                DWORD br2 = 0;
                BOOL ok2 = ReadFile(hPipe, &req2, sizeof(req2), &br2, NULL);
                if (!ok2 || br2 == 0) {
                    cerr << "ReadFile failed while waiting for new data, err=" << GetLastError() << "\n";
                    // освободим write lock
                    if (writeLocksHeld.count(idx)) {
                        locks[idx].unlock_write();
                        writeLocksHeld.erase(idx);
                    }
                }
                else {
                    employees[idx] = req2.emp;
                    fstream fs(filename, ios::in | ios::out | ios::binary);
                    if (fs) {
                        fs.seekp(idx * sizeof(employee), ios::beg);
                        fs.write(reinterpret_cast<const char*>(&employees[idx]), sizeof(employee));
                        fs.flush();
                    }
                    else {
                        cerr << "Failed to open file for update\n";
                    }
                    Response r2; r2.status = 0; r2.emp = employees[idx];
                    DWORD bw3 = 0;
                    WriteFile(hPipe, &r2, sizeof(r2), &bw3, NULL);

                    // Ожидаем RELEASE от клиента
                    Request relReq;
                    DWORD br3 = 0;
                    BOOL ok3 = ReadFile(hPipe, &relReq, sizeof(relReq), &br3, NULL);
                    if (ok3 && br3 > 0 && relReq.type == 3 && relReq.id == req.id) {
                        if (writeLocksHeld.count(idx)) {
                            locks[idx].unlock_write();
                            writeLocksHeld.erase(idx);
                        }
                        Response r3; r3.status = 0;
                        DWORD bw4 = 0;
                        WriteFile(hPipe, &r3, sizeof(r3), &bw4, NULL);
                    }
                    else {
                        if (writeLocksHeld.count(idx)) {
                            locks[idx].unlock_write();
                            writeLocksHeld.erase(idx);
                        }
                    }
                }
            }
        }
        else if (req.type == 3) { // RELEASE (для чтения)
            if (idx >= 0 && readLocksHeld.count(idx)) {
                locks[idx].unlock_read();
                readLocksHeld.erase(idx);
                resp.status = 0;
            }
            else if (idx >= 0) {
                resp.status = -2; // release not held by this client
            }
            else resp.status = -1;
            DWORD bytesWritten = 0;
            WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
        }
        else if (req.type == 4) { // EXIT
            resp.status = 0;
            DWORD bytesWritten = 0;
            WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
            running = false;
            cout << "Client requested exit.\n";
        }
        else {
            resp.status = -1;
            DWORD bytesWritten = 0;
            WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
        }
    }

    // Очистка: освободить все локи, которые держал этот клиент
    unlock_all();

    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    cout << "Client handler finished.\n";
}

int main() {
    cout << "=== Server ===\n";
    string filename;
    cout << "Enter binary filename to create (e.g., employees.dat): ";
    getline(cin, filename);

    int n;
    cout << "Enter number of employees: ";
    cin >> n;
    vector<employee> employees;
    employees.reserve(n);
    for (int i = 0; i < n; ++i) {
        employee e{};
        cout << "Employee " << i + 1 << " ID: ";
        cin >> e.num;
        string name;
        cout << "Name (max 9 chars): ";
        cin >> name;
        strncpy_s(e.name, name.c_str(), _TRUNCATE);
        cout << "Hours: ";
        cin >> e.hours;
        employees.push_back(e);
    }

    {
        ofstream ofs(filename, ios::binary | ios::trunc);
        if (!ofs) {
            cerr << "Cannot create file\n";
            return 1;
        }
        for (const auto& e : employees) ofs.write(reinterpret_cast<const char*>(&e), sizeof(e));
    }

    print_file(employees);
    vector<RWLock> locks(employees.size());

    const char* pipeName = R"(\\.\pipe\EmpPipe)";
    cout << "Server ready. Waiting for clients on " << pipeName << "\n";

    // Принимаем клиентов в цикле, для каждого создаём новый экземпляр канала и поток
    while (true) {
        HANDLE hPipe = CreateNamedPipeA(
            pipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096, 4096,
            0,
            NULL
        );
        if (hPipe == INVALID_HANDLE_VALUE) {
            cerr << "CreateNamedPipe failed, err=" << GetLastError() << "\n";
            return 1;
        }

        BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected) {
            cerr << "Client connection failed, err=" << GetLastError() << "\n";
            CloseHandle(hPipe);
            continue;
        }

        // Запускаем обработчик в отдельном потоке (минимальное изменение)
        thread t(ClientHandler, hPipe, ref(employees), ref(locks), filename);
        t.detach();

        // Сервер продолжает принимать новых клиентов
    }

    return 0;
}
