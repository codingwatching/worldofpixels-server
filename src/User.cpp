#include "User.hpp"

#include <algorithm>
#include <memory>

#include <Session.hpp>

#include <misc/utils.hpp>

#include <nlohmann/json.hpp>

User::User(User::Id uid)
: uid(uid),
  xpCount(0),
  tokenCount(0),
  followersCount(0),
  followingCount(0),
  friendCount(0),
  username("Guest") { }

User::User(User::Id uid, Xp xp, Tokens tok, u32 fers, u32 fing, u32 frnds, std::string u, std::string lw)
: uid(uid),
  xpCount(xp),
  tokenCount(tok),
  followersCount(fers),
  followingCount(fing),
  friendCount(frnds),
  username(std::move(u)),
  lastWorld(std::move(lw)) { }

void User::addXp(Xp xp) {
	xpCount += xp;
	notifyUserUpdate();
}

void User::addTokens(Tokens toks) {
	tokenCount += toks;
	notifyUserUpdate();
}

bool User::remTokens(Tokens toks) {
	if (tokenCount < toks) {
		return false;
	}

	tokenCount -= toks;
	notifyUserUpdate();

	return true;
}

void User::setName(std::string name) {
	username = std::move(name);
	notifyUserUpdate();
}

void User::setLastWorld(std::string worldId) {
	lastWorld = std::move(worldId);
	notifyUserUpdate();
}

User::Id User::getId()         const { return uid; }
User::Xp User::getXp()         const { return xpCount; }
User::Tokens User::getTokens() const { return tokenCount; }
u32 User::getFollowers()       const { return followersCount; }
u32 User::getFollowing()       const { return followingCount; }
u32 User::getFriends()         const { return friendCount; }

const std::string& User::getUsername()  const { return username; }
const std::string& User::getLastWorld() const { return lastWorld; }

bool User::isGuest() const {
	return (uid & 0xFFFFFFFF00000000) == 0xFFFFFFFF00000000;
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

void User::forEachLinkedSession(std::function<void(Session&)> f) {
	// copy the vector because the caller could invalidate the session or kick all of its users
	// and that would remove an element from the linkedSessions vector, causing the iterator
	// of the loop to become invalid, there's probably a faster way, but this is the safest.
	std::vector<std::reference_wrapper<Session>> linkedSessionsCopy(linkedSessions);
	for (auto session : linkedSessionsCopy) {
		f(session);
	}
}

void User::notifyUserUpdate() {
	forEachLinkedSession([] (Session& s) {
		s.userWasUpdated();
	});
}

void to_json(nlohmann::json& j, const User& u) {
	j = {
		{ "uid", n2hexstr(u.getId()) },
		{ "xp", u.getXp() },
		{ "tokens", u.getTokens() },
		{ "followers", u.getFollowers() },
		{ "following", u.getFollowing() },
		{ "friends", u.getFriends() },
		{ "username", u.getUsername() },
		{ "guest",  u.isGuest() }
	};
}
