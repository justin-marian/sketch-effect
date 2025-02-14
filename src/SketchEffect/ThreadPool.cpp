#include "ThreadPool.h"

#include <iostream>
#include <unordered_set>

using namespace std;


////////////////////////////////////////////////////////////////////////////////////////
// Function: ThreadPool
// Description: Constructor that initializes the thread pool with P threads.
// Parameters:
//   - P: Number of threads in the pool.
////////////////////////////////////////////////////////////////////////////////////////
ThreadPool::ThreadPool(size_t P)
{
    for (size_t t_idx = 0; t_idx < P; ++t_idx)
    {
        workers.emplace_back([this] {
			Schedule_Workers(); // lambda function
        });
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// Function: ThreadPool
// Description: Destructor that joins all threads in the pool.
////////////////////////////////////////////////////////////////////////////////////////
ThreadPool::~ThreadPool()
{
    {
        unique_lock<mutex> lock(mutexT);
        stop = true;
    }
    notify.notify_all();
    for (thread& worker : workers)
    {
        worker.join();
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// Function: Add_Task
// Description: Adds a task to the thread pool.
// Parameters:
//   - task: Task to be added to the pool (function<void()>).
//   - name: Optional name for the task to identify it.
////////////////////////////////////////////////////////////////////////////////////////
void ThreadPool::Add_Task(const function<void()>& task, const string& name)
{
    {
        unique_lock<mutex> lock(mutexT);
        tasks.emplace(task, name); // initial state pending
    }
    notify.notify_one();
}


////////////////////////////////////////////////////////////////////////////////////////
// Function: Free_Resource
// Description: Waits for all tasks to be completed and all threads to be free.
////////////////////////////////////////////////////////////////////////////////////////
void ThreadPool::Free_Resource()
{
    unique_lock<mutex> lock(mutexT);
    complete.wait(lock, [this] {
        // all tasks are completed
        return tasks.empty() && P == 0;
    });
}


////////////////////////////////////////////////////////////////////////////////////////
// Function: Schedule_Workers
// Description: Schedules the workers to execute tasks.
////////////////////////////////////////////////////////////////////////////////////////
void ThreadPool::Schedule_Workers()
{
    while (true)
    {
        Task task([] {}, ""); //  DUMMY TASK == NO TASK
        {
            unique_lock<mutex> lock(mutexT);
            notify.wait(lock, [this] {
                return stop || !tasks.empty();
            });

            if (stop && tasks.empty())
            {
                return;
            }

            task = move(tasks.front());
            tasks.pop();
            task.state = TaskState::Running;

            ++P;
        }

        static const unordered_set<string> set_tasks_names = 
        { 
            "COMBINE_IMAGES", 
            "SOBEL_BINARY_EDGE", 
            "HATCHING", 
            "HORIZONTAL_BLUR", 
            "VERTICAL_BLUR" 
        };
        
        if (!task.name.empty() && set_tasks_names.find(task.name) == set_tasks_names.end())
        {
            cerr << "[Error]: Task '" << task.name << "' is not recognized." << endl;
            {
                unique_lock<mutex> lock(mutexT);
                --P;
                complete.notify_one();
            }
            continue;
        }

        task.func();

        {
            unique_lock<mutex> lock(mutexT);
            --P;
            task.state = TaskState::Completed;
            complete.notify_one();
        }
    }
}
