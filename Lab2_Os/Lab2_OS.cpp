#include <windows.h>
#include <iostream>
#include <vector>

using namespace std;

vector<int> arr;          
int        arrSize = 0;        
int        minIndex = 0; 
int        maxIndex = 0; 
double     avg = 0.0;    

DWORD WINAPI MinMaxThread(LPVOID)
{
    int minVal = arr[0];
    int maxVal = arr[0];
    minIndex = maxIndex = 0;

    for (int i = 1; i < arrSize; ++i) {
        if (arr[i] < minVal) {
            minVal = arr[i];
            minIndex = i;
        }
        if (arr[i] > maxVal) {
            maxVal = arr[i];
            maxIndex = i;
        }
        Sleep(7);  
    }

    cout << "Min = " << minVal << " at index " << minIndex << endl;
    cout << "Max = " << maxVal << " at index " << maxIndex << endl;
    return 0;
}

DWORD WINAPI AverageThread(LPVOID)
{
    long long sum = 0;
    for (int i = 0; i < arrSize; ++i) {
        sum += arr[i];
        Sleep(12); 
    }
    avg = static_cast<double>(sum) / arrSize;
    cout << "Average = " << avg << endl;
    return 0;
}

int main()
{
    cout << "Enter array size: ";
    cin >> arrSize;
    arr.resize(arrSize);

    cout << "Enter " << arrSize << " integers:\n";
    for (int i = 0; i < arrSize; ++i) {
        cin >> arr[i];
    }

    HANDLE hMinMax = CreateThread(
        nullptr, 0, MinMaxThread, nullptr, 0, nullptr
    );
    if (!hMinMax) {
        cerr << "Failed to create MinMax thread\n";
        return 1;
    }

    HANDLE hAverage = CreateThread(
        nullptr, 0, AverageThread, nullptr, 0, nullptr
    );
    if (!hAverage) {
        cerr << "Failed to create Average thread\n";
        CloseHandle(hMinMax);
        return 1;
    }

    WaitForSingleObject(hMinMax, INFINITE);
    WaitForSingleObject(hAverage, INFINITE);

    CloseHandle(hMinMax);
    CloseHandle(hAverage);

    int avgInt = static_cast<int>(avg);
    arr[minIndex] = avgInt;
    arr[maxIndex] = avgInt;

    cout << "Array after replacement:\n";
    for (int x : arr) {
        cout << x << " ";
    }
    cout << endl;

    return 0;
}
