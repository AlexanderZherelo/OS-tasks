#include <windows.h>
#include <iostream>
#include <string>

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
    int status;
    employee emp;
};

int main() {
    cout << "=== Client ===\n";
    const char* pipeName = R"(\\.\pipe\EmpPipe)";

    // Подключаемся к именованному каналу
    HANDLE hPipe = CreateFileA(
        pipeName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (hPipe == INVALID_HANDLE_VALUE) {
        cerr << "Failed to connect to pipe, err=" << GetLastError() << "\n";
        return 1;
    }
    cout << "Connected to server pipe.\n";

    bool running = true;
    while (running) {
        cout << "\nChoose operation:\n1 - Read record\n2 - Modify record\n3 - Exit\nEnter choice: ";
        int choice;
        cin >> choice;
        if (choice == 1) {
            Request req{};
            req.type = 1;
            cout << "Enter employee ID to read: ";
            cin >> req.id;
            DWORD bw = 0;
            WriteFile(hPipe, &req, sizeof(req), &bw, NULL);

            Response resp;
            DWORD br = 0;
            BOOL ok = ReadFile(hPipe, &resp, sizeof(resp), &br, NULL);
            if (!ok || br == 0 || resp.status != 0) {
                cout << "Read failed or record not found.\n";
            }
            else {
                cout << "Record received: ID=" << resp.emp.num
                    << " Name=" << string(resp.emp.name, strnlen_s(resp.emp.name, 10))
                    << " Hours=" << resp.emp.hours << "\n";
                cout << "Press Enter to release read lock...";
                cin.ignore(); cin.get();

                // Send RELEASE
                Request rel{};
                rel.type = 3;
                rel.id = req.id;
                DWORD bw2 = 0;
                WriteFile(hPipe, &rel, sizeof(rel), &bw2, NULL);
                Response r2;
                DWORD br2 = 0;
                ReadFile(hPipe, &r2, sizeof(r2), &br2, NULL);
                cout << "Released.\n";
            }
        }
        else if (choice == 2) {
            Request req{};
            req.type = 2;
            cout << "Enter employee ID to modify: ";
            cin >> req.id;
            DWORD bw = 0;
            WriteFile(hPipe, &req, sizeof(req), &bw, NULL);

            Response resp;
            DWORD br = 0;
            BOOL ok = ReadFile(hPipe, &resp, sizeof(resp), &br, NULL);
            if (!ok || br == 0 || resp.status != 0) {
                cout << "Record not found or error.\n";
            }
            else {
                cout << "Current record: ID=" << resp.emp.num
                    << " Name=" << string(resp.emp.name, strnlen_s(resp.emp.name, 10))
                    << " Hours=" << resp.emp.hours << "\n";
                // Ввод новых значений
                employee newe = resp.emp;
                cout << "Enter new name (or '.' to keep): ";
                string newname;
                cin >> newname;
                if (newname != ".") strncpy_s(newe.name, newname.c_str(), _TRUNCATE);
                cout << "Enter new hours (or -1 to keep): ";
                double nh; cin >> nh;
                if (nh >= 0) newe.hours = nh;

                // Отправляем новые данные (Request type=2 с emp=newe)
                Request req2{};
                req2.type = 2;
                req2.id = req.id;
                req2.emp = newe;
                DWORD bw2 = 0;
                WriteFile(hPipe, &req2, sizeof(req2), &bw2, NULL);

                // Получаем подтверждение
                Response r2;
                DWORD br2 = 0;
                ReadFile(hPipe, &r2, sizeof(r2), &br2, NULL);
                if (r2.status == 0) {
                    cout << "Server updated record: ID=" << r2.emp.num
                        << " Name=" << string(r2.emp.name, strnlen_s(r2.emp.name, 10))
                        << " Hours=" << r2.emp.hours << "\n";
                }
                else {
                    cout << "Update failed on server.\n";
                }

                cout << "Press Enter to release write lock...";
                cin.ignore(); cin.get();

                // Send RELEASE
                Request rel{};
                rel.type = 3;
                rel.id = req.id;
                DWORD bw3 = 0;
                WriteFile(hPipe, &rel, sizeof(rel), &bw3, NULL);
                Response r3;
                DWORD br3 = 0;
                ReadFile(hPipe, &r3, sizeof(r3), &br3, NULL);
                cout << "Released.\n";
            }
        }
        else if (choice == 3) {
            Request req{};
            req.type = 4;
            DWORD bw = 0;
            WriteFile(hPipe, &req, sizeof(req), &bw, NULL);
            Response resp;
            DWORD br = 0;
            ReadFile(hPipe, &resp, sizeof(resp), &br, NULL);
            cout << "Exit acknowledged by server.\n";
            running = false;
        }
        else {
            cout << "Invalid choice.\n";
        }
    }

    CloseHandle(hPipe);
    cout << "Client finished. Press Enter to exit.\n";
    cin.ignore(); cin.get();
    return 0;
}
