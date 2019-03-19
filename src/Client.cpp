#include "Client.hpp"

#include <uWS.h>

#include <utils.hpp>
#include <PrepMsg.hpp>

#include <World.hpp>
#include <Session.hpp>

Client::Client(uWS::WebSocket<uWS::SERVER> * ws, Session& s, Ip ip, Player::Builder& pb)
: ws(ws),
  session(s),
  lastAction(std::chrono::steady_clock::now()),
  ip(ip),
  pl(pb.setClient(*this)) {
	session.addClient(*this);
}

Client::~Client() {
	session.delClient(*this);
}

void Client::updateLastActionTime() {
	lastAction = std::chrono::steady_clock::now();
}

bool Client::inactiveKickEnabled() const {
	return true;
}

std::chrono::steady_clock::time_point Client::getLastActionTime() const {
	return lastAction;
}

Ip Client::getIp() const {
	return ip;
}

uWS::WebSocket<true> * Client::getWs() {
	return ws;
}

Player& Client::getPlayer() {
	return pl;
}

Session& Client::getSession() {
	return session;
}

User& Client::getUser() {
	return session.getUser();
}

void Client::send(const PrepMsg& p) {
	ws->sendPrepared(static_cast<uWS::WebSocket<uWS::SERVER>::PreparedMessage *>(p.getPrepared()));
}

void Client::close() {
	ws->close();
}

bool Client::operator ==(const Client& c) const {
	// Client objects are NOT meant to be copied
	return this == std::addressof(c);
}
