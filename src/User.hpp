#pragma once

#include <string>
#include <functional>
#include <misc/explints.hpp>

#include <nlohmann/json_fwd.hpp>

class Session;

class User {
public:
	using Id = u64;

private:
	std::vector<std::reference_wrapper<Session>> linkedSessions;
	const Id uid;
	std::string username;
	bool guest;

public:
	User(Id); // Guest (Temp) account
	User(Id, std::string);

	Id getId() const;
	const std::string& getUsername() const;
	bool isGuest() const;

	void addSession(Session&);
	void delSession(Session&);
};

void to_json(nlohmann::json&, const User&);
