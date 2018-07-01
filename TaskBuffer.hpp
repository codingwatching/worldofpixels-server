#pragma once

#ifdef USE_LIBUV
#include <uv.h>
#endif

#include <functional>
#include <vector>
#include <mutex>

class TaskBuffer {
	std::mutex buflock;
	std::vector<std::function<void(void)>> taskbuf;
#ifdef USE_LIBUV
	uv_async_t * async_hdl;
#endif
	
public:
	TaskBuffer();
	~TaskBuffer();
	
#ifdef USE_LIBUV
	/* This will make the tasks execute in the main thread */
	static void asyncExecute(uv_async_t * const);
#endif
	void runTasks();
	void queueTask(const std::function<void(void)> &); /* Thread safe */
	
private:
	void executeTasks();
};
