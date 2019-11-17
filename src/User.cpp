#include "User.hpp"

#include <algorithm>
#include <memory>

#include <Session.hpp>

#include <utils.hpp>

#include <nlohmann/json.hpp>

User::User(User::Id uid, User::Rep totalRep, UviasRank rank, std::string u)
: uid(uid),
  username(std::move(u)),
  totalRep(totalRep),
  rank(rank) { }

User::Id User::getId() const { return uid; }
User::Rep User::getTotalRep() const { return totalRep; }
const UviasRank& User::getUviasRank() const { return rank; }
const std::string& User::getUsername() const { return username; }

void User::forEachLinkedSession(std::function<void(Session&)> f) {
	// copy the vector because the caller could invalidate the session or kick all of its users
	// and that would remove an element from the linkedSessions vector, causing the iterator
	// of the loop to become invalid, there's probably a faster way, but this is the safest.
	std::vector<std::reference_wrapper<Session>> linkedSessionsCopy(linkedSessions);
	for (auto session : linkedSessionsCopy) {
		f(session);
	}
}

bool User::updateUser(std::string newName, UviasRank newRank) {
	bool changed = false;
	if (username != newName) {
		changed = true;
		username = std::move(newName);
	}

	if (updateUser(std::move(newRank))) {
		// this function already notified the change for us
		return true;
	}

	if (changed) {
		for (auto session : linkedSessions) {
			session.get().userWasUpdated();
		}
	}

	return changed;
}

bool User::updateUser(UviasRank newRank) {
	bool changed = false;

	if (!rank.deepEqual(newRank)) {
		changed = true;
		rank = std::move(newRank);
	}

	if (changed) {
		for (auto session : linkedSessions) {
			session.get().userWasUpdated();
		}
	}

	return changed;
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
	j = {
		{ "uid", n2hexstr(u.getId()) },
		{ "totalRep", u.getTotalRep() },
		{ "rank", u.getUviasRank() },
		{ "username", u.getUsername() }
	};
}
