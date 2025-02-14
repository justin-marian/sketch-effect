#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>

#include <queue>
#include <string>
#include <vector>

/// References: used for the implementation of the ThreadPool class
/// https://stackoverflow.com/questions/75912758/condition-variables-in-c-how-do-i-use-them-properly
/// https://github.com/progschj/ThreadPool/blob/master/ThreadPool.h
/// https://matgomes.com/thread-pools-cpp-with-queues/?utm_source=chatgpt.com

// Pipeline stages
enum class TaskState
{
    Pending,
    Running,
    Completed
};


// Task structure (function, state, name)
struct Task
{
	std::function<void()> func; // function to be executed
	TaskState state;            // state of the task
	std::string name;           // name of the task (from a set of tasks)

    Task(std::function<void()> f, const std::string& n = "") :
        func(move(f)),
        state(TaskState::Pending),
        name(n) {
    }
};


class ThreadPool
{
public:
    ThreadPool(size_t P);
    ~ThreadPool();

	// Add a task to the queue with a name (MUST be from a set of tasks)
    void Add_Task(const std::function<void()>& task, const std::string& name);
	// Wait for all tasks to complete
    void Free_Resource();

private:
	// Worker function, execute each task with no concurrency issues
    void Schedule_Workers();

public:
    std::vector<std::thread> workers;

private:
    bool stop = false;
    size_t P = 0;

    std::mutex mutexT;
    std::condition_variable notify;
    std::condition_variable complete;

    std::queue<Task> tasks;
};
