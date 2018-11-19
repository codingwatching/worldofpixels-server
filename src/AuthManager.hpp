#pragma once

#include <string>
#include <array>
#include <vector>
#include <functional>

#include <Session.hpp>

#include <misc/explints.hpp>
#include <misc/shared_ptr_ll.hpp>
#include <misc/fwd_uWS.h>

class TimedCallbacks;

// todo: user class must have session ref list
class AuthManager {
	static constexpr u64 inactiveSessionLifespanMs = 48 * 60 * 60 * 1000; // 2 days

private:
	TimedCallbacks& tc;
	std::unordered_map<std::array<u8, 16>, Session> sessions;
	std::unordered_map<User::Id, ll::weak_ptr<User>> userCache;

public:
	AuthManager(TimedCallbacks&);

	// Returns nullptr if there is no session by this token
	Session * getSession(std::array<u8, 16>);

	void createGuestSession(uWS::HttpRequest&, std::function<void(std::array<u8, 16>, Session&)>);
	void createGoogleSession(uWS::HttpRequest&, std::string gtoken, std::function<void(std::array<u8, 16>, Session&)>);
};


