#include "Session.hpp"

#include <algorithm>

#include <User.hpp>
#include <Client.hpp>

#pragma message("TODO: Think if HTTP requests need to prevent session expires")

Session::Session(ll::shared_ptr<User> usr, Ipv4 ip, std::string ua, std::string lang, std::chrono::minutes maxInactivity)
: user(std::move(usr)),
  maxInactivity(std::move(maxInactivity)),
  creatorIp(ip),
  creatorUa(std::move(ua)),
  creatorLang(std::move(lang)),
  created(std::chrono::system_clock::now()),
  expires(created + this->maxInactivity) {
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

void Session::updateExpiryTime() {
	expires = std::chrono::system_clock::now() + maxInactivity;
}

bool Session::isExpired() const { // always false if !activeClients.empty()
	return activeClients.empty() && std::chrono::system_clock::now() >= expires;
}

User& Session::getUser() {
	return *user.get();
}

std::chrono::system_clock::time_point Session::getCreationTime() const {
	return created;
}

std::chrono::system_clock::time_point Session::getExpiryTime() const {
	return expires;
}

Ipv4 Session::getCreatorIp() const {
	return creatorIp;
}

const std::string& Session::getCreatorUserAgent() const {
	return creatorUa;
}

const std::string& Session::getPreferredLanguage() const {
	return creatorLang;
}
