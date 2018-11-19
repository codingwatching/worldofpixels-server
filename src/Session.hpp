#pragma once

#include <string>
#include <vector>
#include <functional>

#include <misc/explints.hpp>
#include <misc/shared_ptr_ll.hpp>
#include <misc/fwd_uWS.h>
#include <misc/Ipv4.hpp>

class Client;
class User;

// each client will hold a reference to their session
class Session {
	ll::shared_ptr<User> user;
	std::vector<std::reference_wrapper<Client>> activeClients;

	// This could be stored somewhere else, where RAM won't be wasted
	Ipv4 creatorIp;
	std::string creatorUa;
	std::string creatorLang;

	i64 creationTime;
	i64 expiryTime;

public:
	Session(ll::shared_ptr<User>, uWS::HttpRequest);

	void addClient(Client&);
	void delClient(Client&);

	void updateExpiryTime();
	bool isExpired() const; // always false if !activeClients.empty()

	User& getUser();
	i64 getCreationTime() const;
	i64 getExpiryTime() const;
	const std::string& getCreatorIp() const;
	const std::string& getCreatorUserAgent() const;
	const std::string& getPreferredLanguage() const;
};
