#pragma once

#include <string>
#include <functional>

#include <misc/fwd_uWS.h>
#include <misc/explints.hpp>

#include <nlohmann/json.hpp>

#include <Player.hpp>

struct UserInfo {
	const u64 uid;
	std::string username;
	bool isGuest;

	UserInfo();
	UserInfo(u64, std::string);
};

void to_json(nlohmann::json&, const UserInfo&);

class Client {
	uWS::WebSocket<true> * const ws;
	i64 lastActionOn;
	UserInfo ui;
	std::string ip;
	Player pl;

public:
	Client(uWS::WebSocket<true> *, World&, Player::Builder&, UserInfo, std::string ip);

	void updateLastActionTime();

	i64 getLastActionTime() const;
	const std::string& getIp() const;
	uWS::WebSocket<true> * getWs();
	Player& getPlayer();
	UserInfo& getUserInfo();

	void send(const u8 *, sz_t, bool text);
	void close();

	bool operator ==(const Client&) const;
};
