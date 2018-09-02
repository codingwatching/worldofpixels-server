#include "TimedCallbacks.hpp"

#include <uWS.h>
#include <iostream>

using uS::Timer;
using uS::Loop;
using TimerInfo = TimedCallbacks::TimerInfo;

constexpr auto timerDestructor = [] (Timer * t) {
	t->stop(); // perf hit if not running!
	t->close();
};

constexpr auto timerCb = [] (Timer * t) {
	TimerInfo * ti = static_cast<TimerInfo*>(t->getData());
	if (!ti->cb()) {
		ti->tc.clearTimer(ti->id);
	}
};

TimerInfo::TimerInfo(TimedCallbacks & tc, std::function<bool(void)> && cb,
			std::unique_ptr<uS::Timer, void (*)(uS::Timer *)> t,
			u32 id, int tmo)
: tc(tc),
  cb(std::move(cb)),
  timer(std::move(t)),
  id(id),
  timeout(tmo) { }

TimedCallbacks::TimedCallbacks(Loop * loop)
: timerLoop(loop),
  nextId(0) { }

void TimedCallbacks::clearTimers() {
	runningTimers.clear();
}

bool TimedCallbacks::resetTimer(u32 id) {
	auto s = runningTimers.find(id);
	if (s != runningTimers.end()) {
		s->second->timer->stop();
		s->second->timer->start(timerCb, s->second->timeout, s->second->timeout);
		return true;
	}
	return false;
}

bool TimedCallbacks::clearTimer(u32 id) {
	return runningTimers.erase(id) != 0;
}

u32 TimedCallbacks::startTimer(std::function<bool(void)> && func, int timeout) {
	u32 id = ++nextId;

	std::unique_ptr<uS::Timer, void (*)(uS::Timer *)> t(new Timer(timerLoop), timerDestructor);
	auto p = std::make_unique<TimerInfo>(*this, std::move(func), std::move(t), id, timeout);

	p->timer->setData(p.get());
	p->timer->start(timerCb, timeout, timeout);

	runningTimers.emplace(id, std::move(p));
	return id;
}
