#include "AuthManager.hpp"

#include <iostream>

#include <misc/base64.hpp>
#include <misc/utils.hpp>

AuthManager::AuthManager(TimedCallbacks& tc)
: tc(tc),
  sessionLife(defaultInactiveSessionLifespan),
  guestSessionLife(defaultInactiveGuestSessionLifespan),
  guestIdCounter(0) { }

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

#pragma message("md5hash salted ip for guest tokens")
void AuthManager::createGuestSession(Ipv4 ip, std::string ua, std::string lang,
		std::function<void(std::array<u8, 16>, Session&)> cb) {

	auto it = sessions.emplace(createRandomToken(),
		Session(ll::make_shared<User>(generateGuestId()), ip,
			std::move(ua), std::move(lang), guestSessionLife)
	);

	if (!it.second) {
		std::cerr << "Token generated already exists lol?" << std::endl;
	}

	cb(it.first->first, it.first->second);
}

std::array<u8, 16> AuthManager::createRandomToken() {
	std::array<u8, 16> arr;
	std::generate(arr.begin(), arr.end(), randByte);

	return arr;
}

u64 AuthManager::generateGuestId() {
	return 0xFFFFFFFF00000000ull | ++guestIdCounter;
}
