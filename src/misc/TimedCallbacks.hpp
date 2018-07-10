#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <misc/explints.hpp>

namespace uS {
	struct Timer;
	struct Loop;
}

class TimedCallbacks {
public:
	struct TimerInfo {
		TimerInfo(TimedCallbacks&, std::function<bool(void)> &&,
			std::unique_ptr<uS::Timer, void (*)(uS::Timer *)>,
			u32, int); // dumb

		TimedCallbacks & tc;
		std::function<bool(void)> cb;
		std::unique_ptr<uS::Timer, void (*)(uS::Timer *)> timer;
		u32 id;
		int timeout;
	};

private:
	uS::Loop * const timerLoop;
	u32 nextId;
	std::unordered_map<u32, std::unique_ptr<TimerInfo>> runningTimers;
	
public:
	TimedCallbacks(uS::Loop * loop);
	
	void clearTimers();
	
	bool resetTimer(u32);
	bool clearTimer(u32);

	u32 startTimer(std::function<bool(void)> && func, int timeout);
};
