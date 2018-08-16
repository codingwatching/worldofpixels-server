#pragma once

#include <functional>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>

namespace uS {
	struct Async;
	struct Loop;
}

class TaskBuffer {
	std::unique_ptr<uS::Async, void (*)(uS::Async *)> execCaller;
	std::thread worker;
	std::atomic_flag shouldRun;
	std::condition_variable cv;
	std::mutex cvLock;
	std::mutex mtTaskLock; /* For main thread tasks */
	std::mutex taskLock; /* For async tasks */
	std::vector<std::function<void(TaskBuffer &)>> mtTasks; /* Functions to be run in the main thread */
	std::vector<std::function<void(TaskBuffer &)>> asyncTasks; /* Expensive functions (run on another thread) */

public:
	TaskBuffer(uS::Loop *);
	~TaskBuffer();

	void prepareForDestruction();
	void setWorkerThreadSchedulingPriorityToLowestPossibleValueAllowedByTheOperatingSystem();

	/* Thread safe */
	void runInMainThread(std::function<void(TaskBuffer &)> &&);
	void queue(std::function<void(TaskBuffer &)> &&);

private:
	static void doExecuteMainThreadTasks(uS::Async *);
	void executeMainThreadTasks();
	void executeTasks();
};
