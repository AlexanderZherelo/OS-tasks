//Четный вариант

//ответы на теорию:

//Объектно ориентированное программирование — это принцип програмирования
//где основной единицей является объект. Программа строится как набор объектов, 
//каждый из которых сочетает данные и действия над этими данными.

//Процедурная декомпозиция — это метод структурирования кода,
//при котором сложная задача разбивается на более простые подзадачи,
//каждая из которых реализуется в виде отдельной процедуры или функции.
//Этот подход помогает сделать код более понятным и удобным 

//Архитектура ПО — структура программной системы, она определяет, как устроена программа,
//какие компоненты в ней есть,как они взаимодействуют, и какие принципы лежат в основе её построени

//Энтропия программного обеспечения описываtn рост беспорядка,
//сложности и непредсказуемости в ПО по мере их развития и изменения.
//То есть, чем больше изменений вносится в систему,тем выше вероятность появления ошибок и багов
//Примеры: 1)правки без рефакторинга; 2) сложные зависимости; 3)много костылей 

//Закон гласит, что свобода и разнообразие на одном уровне иерархии требуют усиления ограничений на других уровнях, и наоборот
//Примеры: 1)Архитектура ПО; 2)Система управления доступом 3)Разработка командой

#include <iostream>
#include <vector>
#include <string>
#include <limits>

using namespace std;

vector<long long> firstNFib(size_t n) {
    vector<long long> res;
    if (n == 0) return res;
    res.push_back(0);
    if (n == 1) return res;
    res.push_back(1);
    for (size_t i = 2; i < n; ++i) {
        long long next = res[i - 1] + res[i - 2];
        res.push_back(next);
    }
    return res;
}

bool isPalindrome(const string& s) {
    if (s.empty()) return true;
    if (s[0] == '-') return false;
    size_t i = 0, j = s.size() - 1;
    while (i < j) {
        if (s[i] != s[j]) return false;
        ++i;
        --j;
    }
    return true;
}

struct Node {
    long long value;
    Node* next;
    Node(long long v) : value(v), next(nullptr) {}
};

void pushBack(Node*& head, Node*& tail, long long v) {
    Node* node = new Node(v);
    if (!head) {
        head = tail = node;
    }
    else {
        tail->next = node;
        tail = node;
    }
}

void printList(Node* head) {
    Node* cur = head;
    bool first = true;
    while (cur) {
        if (!first) cout << ' ';
        cout << cur->value;
        first = false;
        cur = cur->next;
    }
    cout << '\n';
}

void deleteList(Node* head) {
    while (head) {
        Node* tmp = head;
        head = head->next;
        delete tmp;
    }
}

Node* reverseIterative(Node* head) {
    Node* prev = nullptr;
    Node* curr = head;
    while (curr) {
        Node* next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}

int main() {
    while (true) {
        cout << "\nChoose operation:\n"
            << "1) Out first n numberf Fib\n"
            << "2) Check if number palindrome\n"
            << "3) Reverse singly linked list iteratively\n"
            << "0) Exit\n"
            << "Enter number: ";
        int choice;
        if (!(cin >> choice)) {
            cout << "Incorrect number. Exit.\n";
            return 0;
        }

        if (choice == 0) {
            return 0;
        }

        if (choice == 1) {
            cout << "Enter nat n (>=0): ";
            long long n;
            if (!(cin >> n) || n < 0) {
                cout << "incorect numb\n";
                continue;
            }
            vector<long long> fib = firstNFib(static_cast<size_t>(n));
            cout << "First " << fib.size() << " numbers Fib\n";
            for (size_t i = 0; i < fib.size(); ++i) {
                if (i) cout << ' ';
                cout << fib[i];
            }
            cout << '\n';
        }
        else if (choice == 2) {
            cout << "Enter number: ";
            string s;
            if (!(cin >> s)) {
                cout << "Incorrect numb\n";
                continue;
            }
            cout << (isPalindrome(s) ? "YES\n" : "NO\n");
        }
        else if (choice == 3) {
            cout << "Enter number of elements (n >= 0): ";
            long long n;
            if (!(cin >> n) || n < 0) {
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }
            Node* head = nullptr;
            Node* tail = nullptr;
            if (n > 0) {
                cout << "Enter " << n << " integer values separated by spaces or newlines:\n";
                for (long long i = 0; i < n; ++i) {
                    long long v;
                    if (!(cin >> v)) {
                        cout << "Incorrect input while reading list\n";
                        deleteList(head);
                        head = tail = nullptr;
                        break;
                    }
                    pushBack(head, tail, v);
                }
            }

            if (!head && n == 0) {
                cout << "Input list is empty\n";
                continue;
            }
            if (!head && n > 0) {
                continue;
            }

            cout << "Original list:\n";
            printList(head);

            Node* newHead = reverseIterative(head);
            cout << "Reversed list:\n";
            printList(newHead);

            deleteList(newHead);
        }
        else {
            cout << "incorrect choice\n";
        }
    }
    return 0;
}

