#pragma once

#include <chrono>

#include <misc/explints.hpp>

class Bucket {
	u16 rate;
	u16 per;
	float allowance;
	std::chrono::steady_clock::time_point last_check;

public:
	Bucket(const u16 rate, const u16 per);
	void set(u16 rate, u16 per);
	bool can_spend(const u16 = 1);
};
