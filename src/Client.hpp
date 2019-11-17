#pragma once

#include <chrono>

#include <fwd_uWS.h>
#include <explints.hpp>
#include <shared_ptr_ll.hpp>
#include <Ip.hpp>

#include <Player.hpp>

class PrepMsg;
class Session;
class User;

class Client { // 95 b
	uWS::WebSocket<true> * const ws;
	ll::shared_ptr<Session> session;
	const std::chrono::steady_clock::time_point connectedOn;
	std::chrono::steady_clock::time_point lastAction;
	Ip ip;
	Player pl;

public:
	Client(uWS::WebSocket<true> *, ll::shared_ptr<Session>, Ip, Player::Builder&);
	~Client();

	void updateLastActionTime();

	bool inactiveKickEnabled() const;
	std::chrono::seconds getSecondsConnected() const;
	std::chrono::steady_clock::time_point getLastActionTime() const;
	Ip getIp() const;
	uWS::WebSocket<true> * getWs();
	Session& getSession();
	Player& getPlayer();
	User& getUser();

	void send(const PrepMsg&);
	void close();

	bool operator ==(const Client&) const;
};
