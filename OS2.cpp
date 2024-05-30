#include <iostream>
#include <list>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>

using namespace std;

class Process {
public:
    int pid;
    bool isForeground;
    bool promoted;
    int remainingTime;  // Time remaining in Wait Queue
    Process(int id, bool fg) : pid(id), isForeground(fg), promoted(false), remainingTime(0) {}
};

class DynamicQueue {
    list<Process> fgQueue;
    list<Process> bgQueue;
    mutex mtx;
public:
    void enqueue(Process p) {
        lock_guard<mutex> lock(mtx);
        if (p.isForeground) {
            fgQueue.push_back(p);
        }
        else {
            bgQueue.push_back(p);
        }
    }

    Process dequeue() {
        lock_guard<mutex> lock(mtx);
        if (!fgQueue.empty()) {
            Process p = fgQueue.front();
            fgQueue.pop_front();
            return p;
        }
        else if (!bgQueue.empty()) {
            Process p = bgQueue.front();
            bgQueue.pop_front();
            return p;
        }
        return Process(-1, false);  // Invalid process
    }

    void promote() {
        lock_guard<mutex> lock(mtx);
        if (!bgQueue.empty()) {
            Process p = bgQueue.back();
            bgQueue.pop_back();
            p.promoted = true;
            fgQueue.push_back(p);
        }
    }

    void printQueue() {
        lock_guard<mutex> lock(mtx);
        cout << "DQ: ";
        cout << "P => ";
        for (auto& p : bgQueue) {
            cout << "[" << p.pid << "B" << (p.promoted ? "*" : "") << "] ";
        }
        if (!bgQueue.empty() && !fgQueue.empty()) {
            cout << endl << "P => ";
        }
        for (auto& p : fgQueue) {
            cout << "[" << p.pid << "F" << (p.promoted ? "*" : "") << "] ";
        }
        cout << "(bottom/top)" << endl;
    }
};

class AlarmClock {
    DynamicQueue dq;
    list<Process> waitQueue;
    int currentTime;
    mutex mtx;
    condition_variable cv;

public:
    AlarmClock() : currentTime(0) {}

    void startShell(int Y) {
        thread([&, Y]() {
            while (true) {
                this_thread::sleep_for(chrono::seconds(Y));
                unique_lock<mutex> lock(mtx);
                Process p = dq.dequeue();
                if (p.pid != -1) {
                    cout << "Running: [" << p.pid << (p.isForeground ? "F" : "B") << "]" << endl;
                    // Execute the process
                    // For simulation, we just print the process id
                    cout << "Process [" << p.pid << "] executed." << endl;
                    if (p.isForeground) {
                        dq.enqueue(p);  // Re-enqueue for continuous execution
                    }
                    else {
                        p.remainingTime = rand() % 10 + 1;  // Random wait time for simulation
                        waitQueue.push_back(p);  // Move to wait queue for BG processes
                    }
                }
                cv.notify_all();
            }
            }).detach();
    }

    void startMonitor(int X) {
        thread([&, X]() {
            while (true) {
                this_thread::sleep_for(chrono::seconds(X));
                unique_lock<mutex> lock(mtx);
                cout << "---------------------------" << endl;
                dq.printQueue();
                cout << "---------------------------" << endl;
                cout << "WQ: ";
                for (auto& p : waitQueue) {
                    cout << "[" << p.pid << (p.isForeground ? "F" : "B") << ":" << p.remainingTime << "] ";
                }
                cout << endl;
                cout << "---------------------------" << endl;
                cv.notify_all();
            }
            }).detach();
    }

    void promotePeriodically(int interval) {
        thread([&, interval]() {
            while (true) {
                this_thread::sleep_for(chrono::seconds(interval));
                dq.promote();
                cv.notify_all();
            }
            }).detach();
    }

    void addProcess(Process p) {
        unique_lock<mutex> lock(mtx);
        dq.enqueue(p);
        cv.notify_all();
    }

    void executeCommand(const string& command) {
        stringstream ss(command);
        string cmd;
        ss >> cmd;

        if (cmd == "echo") {
            string arg;
            ss >> arg;
            cout << arg << endl;
        }
        else if (cmd == "dummy") {
            // Dummy process does nothing
        }
        else if (cmd == "gcd") {
            int x, y;
            ss >> x >> y;
            while (y != 0) {
                int temp = y;
                y = x % y;
                x = temp;
            }
            cout << "GCD: " << x << endl;
        }
        else if (cmd == "prime") {
            int x;
            ss >> x;
            vector<bool> isPrime(x + 1, true);
            isPrime[0] = isPrime[1] = false;
            for (int i = 2; i * i <= x; ++i) {
                if (isPrime[i]) {
                    for (int j = i * i; j <= x; j += i) {
                        isPrime[j] = false;
                    }
                }
            }
            int primeCount = count(isPrime.begin(), isPrime.end(), true);
            cout << "Primes count: " << primeCount << endl;
        }
        else if (cmd == "sum") {
            int x;
            ss >> x;
            int sum = (x * (x + 1)) / 2 % 1000000;
            cout << "Sum: " << sum << endl;
        }
    }

    void simulateCommandExecution(const string& commandLine, int& pidCounter) {
        stringstream ss(commandLine);
        string cmd;

        while (getline(ss, cmd, ';')) {
            stringstream cmdStream(cmd);
            string token;
            vector<string> tokens;
            bool isBackground = false;

            while (cmdStream >> token) {
                if (token[0] == '&') {
                    isBackground = true;
                    token = token.substr(1);
                }
                tokens.push_back(token);
            }

            if (!tokens.empty()) {
                string joinedCommand = tokens[0];
                for (size_t i = 1; i < tokens.size(); ++i) {
                    joinedCommand += " " + tokens[i];
                }
                cout << "prompt > " << joinedCommand << endl;
                executeCommand(joinedCommand);
                addProcess(Process(pidCounter++, !isBackground));
            }
        }
    }

    void readCommandsFromFile(const string& filename, int& pidCounter) {
        ifstream file(filename);
        string line;
        while (getline(file, line)) {
            simulateCommandExecution(line, pidCounter);
        }
    }
};

void promptLoop(AlarmClock& alarm, int& pidCounter) {
    string commandLine;
    while (true) {
        getline(cin, commandLine);
        if (commandLine == "exit") break;
        alarm.simulateCommandExecution(commandLine, pidCounter);
    }
}

int main() {
    AlarmClock alarm;

    // Start background tasks in separate threads
    thread shellThread(&AlarmClock::startShell, &alarm, 2);
    thread monitorThread(&AlarmClock::startMonitor, &alarm, 5);
    thread promoteThread(&AlarmClock::promotePeriodically, &alarm, 10);

    // Adding some initial processes for simulation
    alarm.addProcess(Process(0, true));
    alarm.addProcess(Process(1, false));
    alarm.addProcess(Process(2, true));
    alarm.addProcess(Process(3, false));

    int pidCounter = 4;  // Continue PID counter from 4

    // Read commands from file
    alarm.readCommandsFromFile("commands.txt", pidCounter);

    // Command input loop
    promptLoop(alarm, pidCounter);

    // Join background threads before exiting
    shellThread.join();
    monitorThread.join();
    promoteThread.join();

    return 0;
}