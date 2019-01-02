#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <functional>

#include <explints.hpp>
#include <shared_ptr_ll.hpp>
#include <fwd_uWS.h>
#include <Ipv4.hpp>

#include <nlohmann/json_fwd.hpp>

class Client;
class User;

// each client will hold a reference to their session
class Session {
	ll::shared_ptr<User> user;
	std::vector<std::reference_wrapper<Client>> activeClients;

	std::chrono::minutes maxInactivity;

	// This could be stored somewhere else, where RAM won't be wasted
	Ipv4 creatorIp;
	std::string creatorUa;
	std::string creatorLang;

	std::chrono::system_clock::time_point created;
	std::chrono::system_clock::time_point expires;

public:
	Session(ll::shared_ptr<User>, Ipv4, std::string ua, std::string lang, std::chrono::minutes maxInactivity);
	~Session();

	void addClient(Client&);
	void delClient(Client&);
	
	void forEachClient(std::function<void(Client&)>);

	void userWasUpdated(); // changed name, or any other property
	void updateExpiryTime();
	bool isExpired() const; // always false if !activeClients.empty()

	User& getUser() const;
	std::chrono::system_clock::time_point getCreationTime() const;
	std::chrono::system_clock::time_point getExpiryTime() const;
	Ipv4 getCreatorIp() const;
	const std::string& getCreatorUserAgent() const;
	const std::string& getPreferredLanguage() const;
};

void to_json(nlohmann::json&, const Session&);
