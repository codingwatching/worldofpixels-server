#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <functional>

#include <explints.hpp>
#include <shared_ptr_ll.hpp>
#include <Ip.hpp>

#include <nlohmann/json_fwd.hpp>

class Client;
class User;

// each client will hold a shared_ptr to their session
class Session {
	ll::shared_ptr<User> user;
	std::vector<std::reference_wrapper<Client>> activeClients;

	Ip creatorIp;
	std::chrono::system_clock::time_point created;

public:
	Session(ll::shared_ptr<User>, Ip, std::chrono::system_clock::time_point created);
	~Session();

	// should be private
	void addClient(Client&);
	void delClient(Client&);
	void userWasUpdated(); // changed name, or any other property

	void forEachClient(std::function<void(Client&)>);
	sz_t clientCount() const;

	User& getUser() const;
	std::chrono::system_clock::time_point getCreationTime() const;
	Ip getCreatorIp() const;
};

void to_json(nlohmann::json&, const Session&);
