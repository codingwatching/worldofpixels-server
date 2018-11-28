#pragma once

#include <string>
#include <array>
#include <chrono>
#include <vector>
#include <functional>
#include <unordered_map>

#include <Session.hpp>
#include <User.hpp>

#include <misc/explints.hpp>
#include <misc/shared_ptr_ll.hpp>
#include <misc/fwd_uWS.h>

namespace std {
	template <>
	struct hash<std::array<u8, 16>> {
		std::size_t operator()(const std::array<u8, 16>& k) const {
			// very lazy hash, tokens are random so it's ok,
			// assumes sizeof(std::size_t) <= 16
            return *(reinterpret_cast<const std::size_t *>(k.data()));
        }
    };
}

class TimedCallbacks;

// todo: user class must have session ref list
class AuthManager {
	static constexpr auto defaultInactiveSessionLifespan = std::chrono::hours(48); // 2 days
	static constexpr auto defaultInactiveGuestSessionLifespan = defaultInactiveSessionLifespan / 2;

private:
	TimedCallbacks& tc;
	std::unordered_map<std::array<u8, 16>, Session> sessions;
	std::unordered_map<User::Id, ll::weak_ptr<User>> userCache;
	std::chrono::minutes sessionLife;
	std::chrono::minutes guestSessionLife;
	u32 guestIdCounter;

public:
	AuthManager(TimedCallbacks&);

	// Returns nullptr if there is no session by this token
	Session * getSession(std::array<u8, 16>);
	Session * getSession(const char *, sz_t);
	Session * getSession(const std::string&);

	void createGuestSession(Ipv4, std::string ua, std::string lang,
		std::function<void(std::array<u8, 16>, Session&)>);

	void createGoogleSession(Ipv4, std::string ua, std::string lang,
		std::string gtoken, std::function<void(std::array<u8, 16>, Session&)>);

private:
	static std::array<u8, 16> createRandomToken();
	u64 generateGuestId();
};
