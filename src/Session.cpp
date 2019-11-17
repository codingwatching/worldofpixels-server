#include "Session.hpp"

#include <algorithm>
#include <unordered_set>

#include <User.hpp>
#include <World.hpp>
#include <Client.hpp>

#include <nlohmann/json.hpp>

#pragma message("TODO: Think if HTTP requests need to prevent session expires")

Session::Session(ll::shared_ptr<User> usr, Ip ip, std::chrono::system_clock::time_point created)
: user(std::move(usr)),
  creatorIp(ip),
  created(created) {
	user->addSession(*this);
}

Session::~Session() {
	user->delSession(*this);
}

void Session::addClient(Client& c) { // don't call this with the same client twice
	activeClients.emplace_back(std::ref(c));
}

void Session::delClient(Client& c) {
	auto it = std::find_if(activeClients.begin(), activeClients.end(),
		[&c] (const auto& cr) { return cr.get() == c; });

	if (it != activeClients.end()) {
		activeClients.erase(it);
	}
}

void Session::forEachClient(std::function<void(Client&)> f) {
	// copy for the same reason as the one in User.cpp
	std::vector<std::reference_wrapper<Client>> clientsCopy(activeClients);
	for (auto client : clientsCopy) {
		f(client);
	}
}

void Session::userWasUpdated() {
	std::unordered_set<World *> notifiedWorlds;
	forEachClient([this, &notifiedWorlds] (Client& c) {
		World * w = std::addressof(c.getPlayer().getWorld());
		if (notifiedWorlds.emplace(w).second) { // if the world wasn't in the set
			w->sendUserUpdate(*user);
		}
	});
}

sz_t Session::clientCount() const {
	return activeClients.size();
}

User& Session::getUser() const {
	return *user.get();
}

std::chrono::system_clock::time_point Session::getCreationTime() const {
	return created;
}

Ip Session::getCreatorIp() const {
	return creatorIp;
}

void to_json(nlohmann::json& j, const Session& s) {
	j = {
		{ "created", std::chrono::duration_cast<std::chrono::milliseconds>(s.getCreationTime().time_since_epoch()).count() },
		{ "user", s.getUser() },
		{ "clients", {
			{ "count", s.clientCount() }
		}},
		{ "creator", {
			{ "ip", s.getCreatorIp() }
		}}
	};
}
