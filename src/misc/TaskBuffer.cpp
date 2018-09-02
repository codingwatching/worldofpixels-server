#include "TaskBuffer.hpp"

#ifndef __WIN32
	#include <pthread.h>
#endif

#include <chrono>
#include <iostream>
#include <uWS.h>

constexpr auto asyncDeleter = [] (uS::Async * a) {
	a->close();
};

TaskBuffer::TaskBuffer(uS::Loop * loop)
: execCaller(new uS::Async(loop), asyncDeleter) {
	execCaller->setData(this);
	execCaller->start(TaskBuffer::doExecuteMainThreadTasks);

	//shouldRun = ATOMIC_FLAG_INIT;
	shouldRun.clear();
	shouldRun.test_and_set();
	worker = std::thread([this] { executeTasks(); });
}

TaskBuffer::~TaskBuffer() {
	shouldRun.clear();
	cv.notify_one();
	worker.join();
}

void TaskBuffer::prepareForDestruction() {
	// This funxtion is needed because else the event loop doesn't stop
	// when the server closes
	executeMainThreadTasks();
	execCaller = nullptr;
}

void TaskBuffer::setWorkerThreadSchedulingPriorityToLowestPossibleValueAllowedByTheOperatingSystem() {
#ifndef __WIN32
	sched_param param = { 0 };
	if (auto ret = pthread_setschedparam(worker.native_handle(), SCHED_IDLE, &param)) {
		std::cerr << "pthread_setschedparam failed (" << ret << "): " << strerror(ret) << std::endl;
	}
#else
	std::cerr << __func__ << ": not supported for this platform" << std::endl;
#endif
}

void TaskBuffer::doExecuteMainThreadTasks(uS::Async * a) {
	TaskBuffer * tb = static_cast<TaskBuffer *>(a->getData());
	tb->executeMainThreadTasks();
}

void TaskBuffer::executeMainThreadTasks() {
	/* .empty() is not thread safe, but this is the only
	 * function where items are removed from the queue.
	 */
	if (mtTasks.empty()) {
		return; /* Avoid locking the mutex */
	}

	std::vector<std::function<void(TaskBuffer &)>> tasks;
	{
		std::lock_guard<std::mutex> lk(mtTaskLock);
		std::swap(tasks, mtTasks);
	}
	for (auto & func : tasks) {
		func(*this);
	}
}

void TaskBuffer::executeTasks() {
	std::unique_lock<std::mutex> uLock(cvLock);
	do {
		if (!asyncTasks.empty()) {
			std::vector<std::function<void(TaskBuffer &)>> tasks;
			{
				std::lock_guard<std::mutex> lk(taskLock);
				std::swap(tasks, asyncTasks);
			}
			for (auto & func : tasks) {
				func(*this);
			}
		} else {
			/* Only wait if the vector is empty */
			cv.wait_for(uLock, std::chrono::seconds(1));
		}
	} while (shouldRun.test_and_set());
}

void TaskBuffer::runInMainThread(std::function<void(TaskBuffer &)> && func) {
	{
		std::lock_guard<std::mutex> lk(mtTaskLock);
		mtTasks.push_back(std::move(func));
	}
	execCaller->send();
}

void TaskBuffer::queue(std::function<void(TaskBuffer &)> && func) {
	{
		std::lock_guard<std::mutex> lk(taskLock);
		asyncTasks.push_back(std::move(func));
	}
	cv.notify_one();
}
