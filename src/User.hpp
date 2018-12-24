#pragma once

#include <string>
#include <functional>
#include <misc/explints.hpp>

#include <nlohmann/json_fwd.hpp>

class Session;

class User {
public:
	using Id = u64;
	using Xp = u64;
	using Tokens = u32;

private:
	std::vector<std::reference_wrapper<Session>> linkedSessions;
	const Id uid;
	Xp xpCount;
	Tokens tokenCount;
	u32 followersCount;
	u32 followingCount;
	u32 friendCount;
	std::string username;
	std::string lastWorld;

public:
	User(Id); // Guest (Temp) account
	User(Id, Xp, Tokens, u32, u32, u32, std::string, std::string);

	void addXp(Xp);
	void addTokens(Tokens);
	bool remTokens(Tokens);
	void setName(std::string);
	void setLastWorld(std::string);

	Id getId() const;
	Xp getXp() const;
	Tokens getTokens() const;
	u32 getFollowers() const;
	u32 getFollowing() const;
	u32 getFriends() const;
	const std::string& getUsername() const;
	const std::string& getLastWorld() const;
	bool isGuest() const;

	void addSession(Session&);
	void delSession(Session&);

	void forEachLinkedSession(std::function<void(Session&)>);
	
private:
	void notifyUserUpdate();
};

void to_json(nlohmann::json&, const User&);
