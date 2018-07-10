#include "Bucket.hpp"


Bucket::Bucket(const u16 rate, const u16 per)
	: rate(rate),
	  per(per),
	  allowance(rate) { }

void Bucket::set(u16 nrate, u16 nper) {
	rate = nrate;
	per = nper < 1 ? 1 : nper;
	if (allowance > nrate) {
		allowance = nrate;
	}
}

bool Bucket::can_spend(const u16 count) {
	const auto now = std::chrono::steady_clock::now();
	std::chrono::duration<float> passed = now - last_check;
	last_check = now;
	allowance += passed.count() * (static_cast<float>(rate) / per);
	
	if (allowance > rate) {
		allowance = rate;
	}
	
	if (allowance < count) {
		return false;
	}
	
	allowance -= count;
	return true;
}
