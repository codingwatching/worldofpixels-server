#pragma once

#include <string>
#include <functional>
#include <explints.hpp>

#include <UviasRank.hpp>

#include <nlohmann/json_fwd.hpp>

class Session;
class AuthManager;

class User {
public:
	using Id = u64;
	using Rep = i32;

private:
	std::vector<std::reference_wrapper<Session>> linkedSessions;
	const Id uid;
	std::string username;
	Rep totalRep;
	UviasRank rank;

public:
	User(Id, Rep total, UviasRank, std::string);

	Id getId() const;
	Rep getTotalRep() const;
	const UviasRank& getUviasRank() const;
	const std::string& getUsername() const;

	void forEachLinkedSession(std::function<void(Session&)>);

private:
	bool updateTotalRep(Rep);
	bool updateUser(std::string newName, UviasRank newRank);
	bool updateUser(UviasRank newRank);
	void addSession(Session&);
	void delSession(Session&);

	friend AuthManager;
	friend Session;
};

void to_json(nlohmann::json&, const User&);
