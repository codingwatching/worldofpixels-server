#pragma once

#include <string>
#include <functional>

#include <misc/fwd_uWS.h>
#include <misc/explints.hpp>

#include <Player.hpp>

struct UserInfo {
	const u64 uid;
	std::string username;
};

class Client {
	uWS::WebSocket<true> * const ws;
	i64 lastActionOn;
	Player pl;
	UserInfo ui;
	std::string ip;

public:
	Client(uWS::WebSocket<true> *, World&, UserInfo, std::string ip);

	void updateLastActionTime();
	i64 getLastActionTime() const;
	const std::string& getIp() const;
	uWS::WebSocket<true> * getWs();

	void send(u8 *, sz_t, bool text);
	void close();

	bool operator ==(const Client&) const;
};
