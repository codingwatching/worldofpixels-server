#include "AuthManager.hpp"

#include <iostream>
#include <algorithm>

#include <misc/base64.hpp>
#include <misc/utils.hpp>
#include <misc/BufferHelper.hpp>
#include <misc/TimedCallbacks.hpp>

AuthManager::AuthManager(TimedCallbacks& tc)
: tc(tc),
  sessionLife(defaultInactiveSessionLifespan),
  guestSessionLife(defaultInactiveGuestSessionLifespan),
  guestIdCounter(0),
  sessionTimer(0) {
	std::generate(guestSalt.begin(), guestSalt.end(), randUint32);
}

void AuthManager::getOrLoadUser(User::Id uid, std::function<void(ll::shared_ptr<User>)> f, bool load) {
	auto it = userCache.find(uid);
	if (it == userCache.end()) {
		if (load) {
			// load the user!
			f(nullptr);
		} else {
			f(nullptr);
		}
		return;
	}

	f(it->second.lock());
}

Session * AuthManager::getSession(std::array<u8, 16> token) {
	auto it = sessions.find(token);

	if (it == sessions.end()) {
		return nullptr;
	}

	return &it->second;
}

Session * AuthManager::getSession(const char * b64buf, sz_t size) {
	std::array<u8, 16> token;
	int read = base64Decode(b64buf, size, token.data(), token.size());
	if (read == token.size()) {
		return getSession(token);
	} else {
		std::cout << "B64 decoder didn't read full token or error occurred: " << read << std::endl;
	}

	return nullptr;
}

Session * AuthManager::getSession(const std::string& b64str) {
	return getSession(b64str.data(), b64str.size());
}

void AuthManager::createGuestSession(Ipv4 ip, std::string ua, std::string lang,
		std::function<void(std::array<u8, 16>, Session&)> cb) {
	auto token(createGuestToken(ip));
	auto search = sessions.find(token);

	if (search == sessions.end()) {
		search = sessions.emplace(token,
			Session(ll::make_shared<User>(generateGuestUserId()), ip,
				std::move(ua), std::move(lang), guestSessionLife)
		).first;
	} else {
		search->second.updateExpiryTime();
	}

	cb(token, search->second);

	if (sessionTimer == 0) {
		updateTimer();
	}
}

void AuthManager::forEachSession(std::function<void(const std::array<u8, 16>&, const Session&)> f) {
	for (const auto& session : sessions) {
		f(session.first, session.second);
	}
}

bool AuthManager::updateTimer() {
	if (sessionTimer != 0) {
		tc.clearTimer(sessionTimer);
		sessionTimer = 0;
	}

	if (sessions.size() == 0) {
		sessionTimer = 0;
		return false;
	}

	auto now(std::chrono::system_clock::now());
	auto min(std::chrono::system_clock::time_point::max());

	for (const auto& session : sessions) {
		auto t(session.second.getExpiryTime());
		if (t < min) {
			min = t;
		}
	}

	sessionTimer = tc.startTimer([this] {
		for (auto it = sessions.begin(); it != sessions.end();) {
			// how slow is the call to system_clock::now()?
			if (it->second.isExpired()) {
				it = sessions.erase(it);
			} else {
				++it;
			}
		}

		sessionTimer = 0; // don't destroy the timer being fired just yet, updateTimer.
		updateTimer();
		return false;
	}, std::chrono::duration_cast<std::chrono::milliseconds>(min - now).count());

	return true;
}

std::array<u8, 16> AuthManager::createRandomToken() {
	std::array<u8, 16> arr;
	std::generate(arr.begin(), arr.end(), randUint32);

	return arr;
}

std::array<u8, 16> AuthManager::createGuestToken(Ipv4 ip) {
	static_assert(sizeof(decltype(Ipv4().get())) == 4);

	std::array<u8, 16> arr;
	auto it = std::copy(guestSalt.begin(), guestSalt.end(), arr.begin());
	buf::writeLE(&*it, ip.get());

	return arr;
}

u64 AuthManager::generateGuestUserId() {
	return 0xFFFFFFFF00000000ull | ++guestIdCounter;
}
