#pragma once

#include <misc/fwd_uWS.h>
#include <misc/explints.hpp>
#include <misc/Ipv4.hpp>

#include <Player.hpp>

class PrepMsg;
class Session;
class User;

class Client { // 95 b
	uWS::WebSocket<true> * const ws;
	Session& session;
	i64 lastActionOn;
	Ipv4 ip;
	Player pl;

public:
	Client(uWS::WebSocket<true> *, Session&, Ipv4, Player::Builder&);
	~Client();

	void updateLastActionTime();

	bool inactiveKickEnabled() const;
	i64 getLastActionTime() const;
	Ipv4 getIp() const;
	uWS::WebSocket<true> * getWs();
	Session& getSession();
	Player& getPlayer();
	User& getUser();

	void send(const PrepMsg&);
	void close();

	bool operator ==(const Client&) const;
};
