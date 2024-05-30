#include <iostream>
#include <thread>
#include <condition_variable>
#include <queue>
#include <list>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>

// Process 구조체
struct Process {
    int pid;
    char type; // 'F' for Foreground, 'B' for Background
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
        top->processList.push_back(p);
    }
    else {
        top->processList.push_front(p);
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
    std::cout << "Dynamic Queue:" << std::endl;
    while (current) {
        for (const auto& proc : current->processList) {
            std::cout << proc.pid << proc.type << " ";
        }
        std::cout << std::endl;
        current = current->next;
    }
}

// Foreground Process (Shell)
void foregroundProcess(DynamicQueue& dq) {
    while (true) {
        Process p = dq.dequeue();
        std::cout << "Shell is handling commands: Process " << p.pid << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate command processing time
        dq.enqueue(p);
    }
}

// Background Process (Monitor)
void backgroundProcess(DynamicQueue& dq) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Monitor is monitoring system state..." << std::endl;
        dq.printQueue();
        dq.promote();
    }
}

// Scheduler function
void scheduler() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate 1-second scheduling interval
    }
}

int main() {
    DynamicQueue dq;

    // Create and enqueue shell and monitor processes
    Process shell = { 0, 'F' };
    Process monitor = { 1, 'B' };
    dq.enqueue(shell);
    dq.enqueue(monitor);

    // Create threads for processes
    std::thread shellThread(foregroundProcess, std::ref(dq));
    std::thread monitorThread(backgroundProcess, std::ref(dq));

    // Start the scheduler
    std::thread schedulerThread(scheduler);

    // Join the threads
    shellThread.join();
    monitorThread.join();
    schedulerThread.join();

    return 0;
}