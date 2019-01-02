#pragma once

#include <chrono>

#include <fwd_uWS.h>
#include <explints.hpp>
#include <Ipv4.hpp>

#include <Player.hpp>

class PrepMsg;
class Session;
class User;

class Client { // 95 b
	uWS::WebSocket<true> * const ws;
	Session& session;
	std::chrono::steady_clock::time_point lastAction;
	Ipv4 ip;
	Player pl;

public:
	Client(uWS::WebSocket<true> *, Session&, Ipv4, Player::Builder&);
	~Client();

	void updateLastActionTime();

	bool inactiveKickEnabled() const;
	std::chrono::steady_clock::time_point getLastActionTime() const;
	Ipv4 getIp() const;
	uWS::WebSocket<true> * getWs();
	Session& getSession();
	Player& getPlayer();
	User& getUser();

	void send(const PrepMsg&);
	void close();

	bool operator ==(const Client&) const;
};
