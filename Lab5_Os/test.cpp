#include <pch.h>
// Server_tests.cpp
#include <gtest/gtest.h>
#include <windows.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include <cstdio>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <set>
#include <cstring>

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

// find_record_index 
int find_record_index(const std::vector<employee>& vec, int id) {
    for (size_t i = 0; i < vec.size(); ++i) if (vec[i].num == id) return (int)i;
    return -1;
}

// Совместимая реализация RWLock (копия логики)
class RWLock {
    std::mutex m;
    std::condition_variable cv;
    int readers = 0;
    bool writer = false;
public:
    void lock_read() {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&]() { return !writer; });
        ++readers;
    }
    void unlock_read() {
        std::unique_lock<std::mutex> lk(m);
        if (--readers == 0) cv.notify_all();
    }
    void lock_write() {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&]() { return !writer && readers == 0; });
        writer = true;
    }
    void unlock_write() {
        std::unique_lock<std::mutex> lk(m);
        writer = false;
        cv.notify_all();
    }
};

// Локальная реализация ClientHandler в тестовом файле
// Эта реализация следует тому же протоколу, что и оригинал:
void ClientHandler(HANDLE hPipe, std::vector<employee>& employees, std::vector<RWLock>& locks, const std::string& filename) {
    std::set<int> readLocksHeld;
    std::set<int> writeLocksHeld;

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
        Request req{};
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(hPipe, &req, sizeof(req), &bytesRead, NULL);
        if (!ok || bytesRead == 0) {
            // клиент отключился или ошибка — завершаем
            break;
        }

        Response resp{};
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

                // Ожидаем новые данные (Request с emp)
                Request req2{};
                DWORD br2 = 0;
                BOOL ok2 = ReadFile(hPipe, &req2, sizeof(req2), &br2, NULL);
                if (!ok2 || br2 == 0) {
                    if (writeLocksHeld.count(idx)) {
                        locks[idx].unlock_write();
                        writeLocksHeld.erase(idx);
                    }
                }
                else {
                    // обновляем в памяти и в файле
                    employees[idx] = req2.emp;
                    std::fstream fs(filename, std::ios::in | std::ios::out | std::ios::binary);
                    if (fs) {
                        fs.seekp(idx * sizeof(employee), std::ios::beg);
                        fs.write(reinterpret_cast<const char*>(&employees[idx]), sizeof(employee));
                        fs.flush();
                    }
                    Response r2; r2.status = 0; r2.emp = employees[idx];
                    DWORD bw3 = 0;
                    WriteFile(hPipe, &r2, sizeof(r2), &bw3, NULL);

                    // Ожидаем RELEASE
                    Request relReq{};
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
        else if (req.type == 3) { 
            if (idx >= 0 && readLocksHeld.count(idx)) {
                locks[idx].unlock_read();
                readLocksHeld.erase(idx);
                resp.status = 0;
            }
            else if (idx >= 0) {
                resp.status = -2; 
            }
            else resp.status = -1;
            DWORD bytesWritten = 0;
            WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
        }
        else if (req.type == 4) { 
            resp.status = 0;
            DWORD bytesWritten = 0;
            WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
            running = false;
        }
        else {
            resp.status = -1;
            DWORD bytesWritten = 0;
            WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
        }
    }

    unlock_all();
    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}

// ---------------------------
// Тесты
// ---------------------------

TEST(FindRecordIndex, FoundAndNotFound) {
    std::vector<employee> v;
    employee e1{}; e1.num = 10;
    employee e2{}; e2.num = 20;
    v.push_back(e1); v.push_back(e2);
    EXPECT_EQ(find_record_index(v, 10), 0);
    EXPECT_EQ(find_record_index(v, 20), 1);
    EXPECT_EQ(find_record_index(v, 30), -1);
}

TEST(RWLock, MultipleReadersAndExclusiveWriter) {
    RWLock lock;
    std::atomic<int> readersInside{ 0 };
    std::atomic<bool> writerEntered{ false };
    std::atomic<int> maxReaders{ 0 };

    std::thread r1([&]() {
        lock.lock_read();
        ++readersInside;
        maxReaders = std::max<int>(maxReaders, readersInside.load());
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        --readersInside;
        lock.unlock_read();
        });
    std::thread r2([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        lock.lock_read();
        ++readersInside;
        maxReaders = std::max<int>(maxReaders, readersInside.load());
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        --readersInside;
        lock.unlock_read();
        });

    std::thread w([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        lock.lock_write();
        writerEntered = true;
        EXPECT_EQ(readersInside.load(), 0);
        lock.unlock_write();
        });

    r1.join(); r2.join(); w.join();
    EXPECT_GE(maxReaders.load(), 2);
    EXPECT_TRUE(writerEntered.load());
}

TEST(FileIO, WriteAndReadEmployeeBinary) {
    const char* fname = "test_employees.bin";
    std::vector<employee> v;
    employee e{}; e.num = 42; strncpy_s(e.name, "Ivan", _TRUNCATE); e.hours = 12.5;
    v.push_back(e);

    {
        std::ofstream ofs(fname, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(ofs.is_open());
        ofs.write(reinterpret_cast<const char*>(&v[0]), sizeof(employee));
    }

    employee r{};
    {
        std::ifstream ifs(fname, std::ios::binary);
        ASSERT_TRUE(ifs.is_open());
        ifs.read(reinterpret_cast<char*>(&r), sizeof(employee));
    }

    EXPECT_EQ(r.num, e.num);
    EXPECT_STREQ(r.name, e.name);
    EXPECT_DOUBLE_EQ(r.hours, e.hours);

    std::remove(fname);
}

// Интеграционный тест с локальной реализацией ClientHandler
TEST(Integration, ClientHandler_ReadReleaseAndExit_LocalHandler) {
    const char* fname = "test_employees2.bin";
    std::vector<employee> employees;
    employee e{}; e.num = 1; strncpy_s(e.name, "Petya", _TRUNCATE); e.hours = 8.0;
    employees.push_back(e);
    {
        std::ofstream ofs(fname, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(ofs.is_open());
        ofs.write(reinterpret_cast<const char*>(&employees[0]), sizeof(employee));
    }
    std::vector<RWLock> locks(employees.size());

    const char* pipeName = R"(\\.\pipe\TestEmpPipeForUnitTest2)";
    HANDLE hPipeServer = CreateNamedPipeA(
        pipeName,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,
        4096, 4096,
        0,
        NULL
    );
    ASSERT_NE(hPipeServer, INVALID_HANDLE_VALUE);

    // Серверный поток: ждёт подключения и запускает локальную ClientHandler
    std::thread serverThread([&]() {
        BOOL connected = ConnectNamedPipe(hPipeServer, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected) return;
        ClientHandler(hPipeServer, employees, locks, fname);
        });

    // Небольшая пауза, затем клиент подключается
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    HANDLE hClient = CreateFileA(
        pipeName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    ASSERT_NE(hClient, INVALID_HANDLE_VALUE);

    // READ id=1
    Request reqRead{}; reqRead.type = 1; reqRead.id = 1;
    DWORD bw = 0;
    BOOL ok = WriteFile(hClient, &reqRead, sizeof(reqRead), &bw, NULL);
    ASSERT_TRUE(ok && bw == sizeof(reqRead));

    Response resp{};
    DWORD br = 0;
    ok = ReadFile(hClient, &resp, sizeof(resp), &br, NULL);
    ASSERT_TRUE(ok && br == sizeof(resp));
    EXPECT_EQ(resp.status, 0);
    EXPECT_EQ(resp.emp.num, 1);
    EXPECT_STREQ(resp.emp.name, "Petya");

    // RELEASE
    Request rel{}; rel.type = 3; rel.id = 1;
    ok = WriteFile(hClient, &rel, sizeof(rel), &bw, NULL);
    ASSERT_TRUE(ok && bw == sizeof(rel));
    Response r2{};
    ok = ReadFile(hClient, &r2, sizeof(r2), &br, NULL);
    ASSERT_TRUE(ok && br == sizeof(r2));
    EXPECT_EQ(r2.status, 0);

    // EXIT
    Request exitReq{}; exitReq.type = 4; exitReq.id = 0;
    ok = WriteFile(hClient, &exitReq, sizeof(exitReq), &bw, NULL);
    ASSERT_TRUE(ok && bw == sizeof(exitReq));
    Response r3{};
    ok = ReadFile(hClient, &r3, sizeof(r3), &br, NULL);
    ASSERT_TRUE(ok && br == sizeof(r3));
    EXPECT_EQ(r3.status, 0);

    CloseHandle(hClient);
    serverThread.join();
    std::remove(fname);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
