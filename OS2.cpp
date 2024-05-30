#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <thread>
#include <condition_variable>
#include <queue>
#include <list>
#include <vector>
#include <memory>
#include <chrono>
#include <cstring>
#include <algorithm>

// Process 구조체
struct Process {
    int pid;
    char type; // 'F' for Foreground, 'B' for Background
    int wakeUpTime; // 남은 대기 시간
    bool promoted; // 프로모션 여부
};

// StackNode 구조체
struct StackNode {
    std::list<Process> processList;
    std::shared_ptr<StackNode> next;
};

// DynamicQueue 클래스
class DynamicQueue {
private:
    std::shared_ptr<StackNode> top;
    std::mutex mtx;
    std::condition_variable cv;

public:
    DynamicQueue() : top(std::make_shared<StackNode>()) {}

    void enqueue(Process p);
    Process dequeue();
    void promote();
    void split_n_merge();
    void printQueue();
};

void DynamicQueue::enqueue(Process p) {
    std::lock_guard<std::mutex> lock(mtx);
    if (p.type == 'F') {
        top->processList.push_front(p);
    }
    else {
        if (top->processList.empty()) {
            top->processList.push_back(p);
        }
        else {
            auto current = top;
            while (current->next != nullptr) {
                current = current->next;
            }
            current->next = std::make_shared<StackNode>();
            current->next->processList.push_back(p);
            current->next->next = nullptr;
        }
    }
    cv.notify_all();
}


Process DynamicQueue::dequeue() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] { return !top->processList.empty(); });

    Process p = top->processList.front();
    top->processList.pop_front();

    if (top->processList.empty() && top->next) {
        top = top->next;
    }

    return p;
}

void DynamicQueue::promote() {
    std::lock_guard<std::mutex> lock(mtx);

    if (!top->next) return;

    auto& currentList = top->processList;
    auto& nextList = top->next->processList;

    nextList.splice(nextList.end(), currentList, currentList.begin());

    if (currentList.empty() && top->next) {
        top = top->next;
    }
}

void DynamicQueue::split_n_merge() {
    std::lock_guard<std::mutex> lock(mtx);

    const size_t threshold = 5;
    std::shared_ptr<StackNode> current = top;

    while (current) {
        if (current->processList.size() > threshold) {
            auto newNode = std::make_shared<StackNode>();
            auto it = current->processList.begin();
            std::advance(it, current->processList.size() / 2);
            newNode->processList.splice(newNode->processList.begin(), current->processList, current->processList.begin(), it);
            newNode->next = current->next;
            current->next = newNode;
        }
        current = current->next;
    }
}

void DynamicQueue::printQueue() {
    std::lock_guard<std::mutex> lock(mtx);
    std::shared_ptr<StackNode> current = top;
    std::cout << "DQ: ";
    bool isFirstNode = true;
    bool isLastNode = false;
    while (current) {
        if (!current->processList.empty()) {
            std::cout << (isFirstNode ? "P => " : "") << "[";
            for (const auto& proc : current->processList) {
                std::cout << proc.pid << proc.type << (proc.promoted ? "*" : "") << " ";
            }
            std::cout << "]";
            if (isFirstNode && current->next == nullptr)
                std::cout << " (bottom/top)";
            else if (isFirstNode)
                std::cout << " (bottom)";
            else if (current->next == nullptr)
                std::cout << " (top)";
            std::cout << " ";
            isFirstNode = false;
            if (current->next == nullptr)
                isLastNode = true;
        }
        current = current->next;
    }
    std::cout << std::endl;
}

// WaitQueue 클래스
class WaitQueue {
private:
    std::list<Process> waitQueue;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void enqueue(Process p);
    void update(int elapsedTime);
    void wakeUpProcesses(DynamicQueue& dq);
    void printQueue();
};

void WaitQueue::enqueue(Process p) {
    std::lock_guard<std::mutex> lock(mtx);
    waitQueue.push_back(p);
    waitQueue.sort([](const Process& a, const Process& b) {
        return a.wakeUpTime < b.wakeUpTime;
        });
    cv.notify_all();
}

void WaitQueue::update(int elapsedTime) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& p : waitQueue) {
        p.wakeUpTime -= elapsedTime;
    }
}

void WaitQueue::wakeUpProcesses(DynamicQueue& dq) {
    std::lock_guard<std::mutex> lock(mtx);
    while (!waitQueue.empty() && waitQueue.front().wakeUpTime <= 0) {
        Process p = waitQueue.front();
        waitQueue.pop_front();
        dq.enqueue(p);
    }
}

void WaitQueue::printQueue() {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "WQ: ";
    for (const auto& p : waitQueue) {
        std::cout << "[" << p.pid << p.type << ":" << p.wakeUpTime << "] ";
    }
    std::cout << std::endl;
}

// Foreground Process (Shell)
void foregroundProcess(DynamicQueue& dq, WaitQueue& wq) {
    while (true) {
        Process p = dq.dequeue();
        std::cout << "Running: [" << p.pid << p.type << "]" << std::endl;
        std::cout << "-----------------------------" << std::endl;
        dq.printQueue();
        std::cout << "-----------------------------" << std::endl;
        wq.printQueue();
        std::cout << "-----------------------------" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3)); // Simulate command processing time
        p.wakeUpTime = 5; // For example, 5 seconds to wake up
        wq.enqueue(p);
    }
}

// Background Process (Monitor)
void backgroundProcess(DynamicQueue& dq, WaitQueue& wq) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Running: [1B]" << std::endl;
        std::cout << "-----------------------------" << std::endl;
        dq.printQueue();
        std::cout << "-----------------------------" << std::endl;
        wq.printQueue();
        std::cout << "-----------------------------" << std::endl;
        dq.promote();
    }
}

// Scheduler function
void scheduler(DynamicQueue& dq, WaitQueue& wq) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate 1-second scheduling interval
        wq.update(1);
        wq.wakeUpProcesses(dq);
    }
}

// Command parsing
char** parse(const char* command) {
    std::vector<char*> tokens;
    char* cmd = _strdup(command);
    char* token = strtok(cmd, " ");
    while (token != nullptr) {
        tokens.push_back(_strdup(token));
        token = strtok(nullptr, " ");
    }
    tokens.push_back(_strdup("")); // End of tokens
    char** result = new char* [tokens.size()];
    std::copy(tokens.begin(), tokens.end(), result);
    free(cmd);
    return result;
}

// Execute parsed command
void exec(char** args) {
    // For this example, we'll just print the arguments
    std::cout << "Executing command:";
    for (int i = 0; args[i][0] != '\0'; ++i) {
        std::cout << " " << args[i];
    }
    std::cout << std::endl;

    // Clean up
    for (int i = 0; args[i][0] != '\0'; ++i) {
        free(args[i]);
    }
    delete[] args;
}

int main() {
    DynamicQueue dq;
    WaitQueue wq;

    // 프로세스 생성 및 enqueue
    Process shell = { 0, 'F', 0, false };
    Process monitor = { 1, 'B', 0, false };
    dq.enqueue(shell);
    dq.enqueue(monitor);

    // Create threads for processes
    std::thread shellThread(foregroundProcess, std::ref(dq), std::ref(wq));
    std::thread monitorThread(backgroundProcess, std::ref(dq), std::ref(wq));

    // Start the scheduler
    std::thread schedulerThread(scheduler, std::ref(dq), std::ref(wq));

    // Join the threads
    shellThread.join();
    monitorThread.join();
    schedulerThread.join();

    return 0;
}