#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <cstdlib>

using namespace std;

struct ThreadParam {
    int id;                // 1-based id
    int n;                 // array size
    int* arr;
    CRITICAL_SECTION* pcs;
    HANDLE hStartEvent;
    HANDLE hBlockedEvent;  
    HANDLE hControlEvent;  
    char* terminateFlag;   
    HANDLE hThreadExit;    
};

DWORD WINAPI MarkerThread(LPVOID lpParam) {
    ThreadParam* p = reinterpret_cast<ThreadParam*>(lpParam);
    int id = p->id;
    int n = p->n;
    int* arr = p->arr;
    CRITICAL_SECTION* pcs = p->pcs;

    // Wait for start signal from main
    WaitForSingleObject(p->hStartEvent, INFINITE);
    srand(id);

    while (true) {
        int r = rand();
        int idx = r % n;

        // Try to mark
        EnterCriticalSection(pcs);
        if (arr[idx] == 0) {
            Sleep(5);
            arr[idx] = id;
            Sleep(5);
            LeaveCriticalSection(pcs);
            continue;
        }
        else {
            // Can't mark: count how many elements marked by this thread
            int count = 0;
            for (int i = 0; i < n; ++i) 
                if (arr[i] == id) 
                    ++count;

            LeaveCriticalSection(pcs);
            cout << "Marker " << id
                << " cannot continue. Marked count: " << count
                << " first blocked index: " << idx << endl;

            // Signal main about impossibility to continue
            SetEvent(p->hBlockedEvent);
            WaitForSingleObject(p->hControlEvent, INFINITE);
            EnterCriticalSection(pcs);
            bool shouldTerminate = (*(p->terminateFlag) != 0);
            LeaveCriticalSection(pcs);

            if (shouldTerminate) {
                EnterCriticalSection(pcs);
                for (int i = 0; i < n; ++i) {
                    if (arr[i] == id) arr[i] = 0;
                }
                LeaveCriticalSection(pcs);
                break;
            }
            else {
                continue;
            }
        }
    }

    return 0;
}

void PrintArray(int* arr, int n) {
    cout << "Array: ";
    for (int i = 0; i < n; ++i) {
        cout << arr[i] << (i + 1 == n ? '\n' : ' ');
    }
}

int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    int n = 0;
    cout << "Enter array size: ";
    if (!(cin >> n) || n <= 0)
        return 0;
    vector<int> arrVec(n, 0);
    int* arr = arrVec.data();

    int m = 0;
    cout << "Enter number of marker threads: ";
    if (!(cin >> m) || m <= 0)
        return 0;

    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    HANDLE hStartEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr); 

    vector<HANDLE> hBlockedEvents(m);
    vector<HANDLE> hControlEvents(m);
    vector<char> terminateFlags(m, 0); // 0 == running, 1 == terminate
    vector<HANDLE> threadHandles(m);
    vector<ThreadParam> params(m);

    for (int i = 0; i < m; ++i) {
        hBlockedEvents[i] = CreateEvent(nullptr, TRUE, FALSE, nullptr); // manual-reset
        hControlEvents[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr); // auto-reset
    }

    // Create threads
    for (int i = 0; i < m; ++i) {
        params[i].id = i + 1; 
        params[i].n = n;
        params[i].arr = arr;
        params[i].pcs = &cs;
        params[i].hStartEvent = hStartEvent;
        params[i].hBlockedEvent = hBlockedEvents[i];
        params[i].hControlEvent = hControlEvents[i];
        params[i].terminateFlag = &terminateFlags[i];

        threadHandles[i] = CreateThread(nullptr, 0, MarkerThread, &params[i], 0, nullptr);
        if (threadHandles[i] == nullptr) {
            cerr << "Failed to create thread " << (i + 1) << endl;
        }
    }

    SetEvent(hStartEvent);
    int remaining = m;
    while (remaining > 0) {
        if (m > MAXIMUM_WAIT_OBJECTS) {
            cerr << "Too many marker threads for this implementation." << endl;
            break;
        }

        vector<HANDLE> activeBlockedHandles;
        activeBlockedHandles.reserve(m);
        for (int i = 0; i < m; ++i) {
            EnterCriticalSection(&cs);
            bool isAlive = (terminateFlags[i] == 0); // 0 == running
            LeaveCriticalSection(&cs);
            if (isAlive) {
                activeBlockedHandles.push_back(hBlockedEvents[i]);
            }
        }
        if (activeBlockedHandles.empty()) 
            break;

        WaitForMultipleObjects((DWORD)activeBlockedHandles.size(), activeBlockedHandles.data(), TRUE, INFINITE);
        EnterCriticalSection(&cs);
        PrintArray(arr, n);
        LeaveCriticalSection(&cs);

        int termId = 0;
        while (true) {
            cout << "Enter marker id to terminate (1.." << m << "): ";
            if (!(cin >> termId)) {
                cin.clear();
                string tmp;
                getline(cin, tmp);
                continue;
            }
            if (termId < 1 || termId > m) {
                cout << "Invalid id. Try again." << endl;
                continue;
            }
            EnterCriticalSection(&cs);
            bool already = (terminateFlags[termId - 1] != 0);
            LeaveCriticalSection(&cs);
            if (already) {
                cout << "This marker is already marked for termination or terminated. Choose another." << endl;
                continue;
            }
            break;
        }

        // Signal chosen marker to terminate (set flag under CS then wake it)
        EnterCriticalSection(&cs);
        terminateFlags[termId - 1] = 1;
        LeaveCriticalSection(&cs);
        SetEvent(hControlEvents[termId - 1]); // wake that marker

        // Wait for that thread to exit
        WaitForSingleObject(threadHandles[termId - 1], INFINITE);
        CloseHandle(threadHandles[termId - 1]);
        --remaining;
        EnterCriticalSection(&cs);
        PrintArray(arr, n);
        LeaveCriticalSection(&cs);
        ResetEvent(hBlockedEvents[termId - 1]);
        for (int i = 0; i < m; ++i) {
            if (i == termId - 1) continue;
            EnterCriticalSection(&cs);
            bool isTerminated = (terminateFlags[i] != 0);
            LeaveCriticalSection(&cs);
            if (!isTerminated) {
                ResetEvent(hBlockedEvents[i]);
                SetEvent(hControlEvents[i]);
            }
        }
    }

    cout << "All markers finished. Main exiting." << endl;

    for (int i = 0; i < m; ++i) {
        if (hBlockedEvents[i]) CloseHandle(hBlockedEvents[i]);
        if (hControlEvents[i]) CloseHandle(hControlEvents[i]);
    }
    if (hStartEvent) CloseHandle(hStartEvent);

    DeleteCriticalSection(&cs);

    return 0;
}
