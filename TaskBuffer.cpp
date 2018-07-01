#include "TaskBuffer.hpp"

#include <cstdlib>
#include <stdexcept>

TaskBuffer::TaskBuffer() {
#ifdef USE_LIBUV
	async_hdl = (uv_async_t *)std::malloc(sizeof(uv_async_t));
	if (async_hdl == nullptr) {
		throw std::bad_alloc();
	}
	uv_async_init(uv_default_loop(), async_hdl, (uv_async_cb)&asyncExecute);
	async_hdl->data = this;
#endif
}

TaskBuffer::~TaskBuffer() {
	/* Should I run remaining tasks? */
#ifdef USE_LIBUV
	uv_close((uv_handle_t *)async_hdl, (uv_close_cb)([](uv_handle_t * const hdl){
		std::free(hdl);
	}));
#endif
}

void TaskBuffer::runTasks() {
#ifdef USE_LIBUV
	uv_async_send(async_hdl);
#else
	executeTasks();
#endif
}

#ifdef USE_LIBUV
void TaskBuffer::asyncExecute(uv_async_t * const hdl) {
	TaskBuffer * const tb = (TaskBuffer *)hdl->data;
	tb->executeTasks();
}
#endif

void TaskBuffer::queueTask(const std::function<void(void)> & task) {
	buflock.lock();
	taskbuf.push_back(task);
	buflock.unlock();
}

void TaskBuffer::executeTasks() {
	for (auto & task : taskbuf) {
		task();
	}
	taskbuf.clear();
}
