#include "User.hpp"

#include <algorithm>
#include <memory>

#include <Session.hpp>

#include <misc/utils.hpp>

#include <nlohmann/json.hpp>

User::User(User::Id uid)
: uid(uid),
  username("Guest"),
  guest(true) { }

User::User(User::Id uid, std::string s)
: uid(uid),
  username(std::move(s)),
  guest(false) { }

User::Id User::getId() const {
	return uid;
}

const std::string& User::getUsername() const {
	return username;
}

bool User::isGuest() const {
	return guest;
}

void User::addSession(Session& s) {
	linkedSessions.emplace_back(std::ref(s));
}

void User::delSession(Session& s) {
	auto it = std::find_if(linkedSessions.begin(), linkedSessions.end(),
		[&s] (const auto& sr) { return std::addressof(sr.get()) == std::addressof(s); });

	if (it != linkedSessions.end()) {
		linkedSessions.erase(it);
	}
}

void to_json(nlohmann::json& j, const User& u) {
	j["uid"] = n2hexstr(u.getId());
	j["username"] = u.getUsername();
	j["guest"] = u.isGuest();
}
