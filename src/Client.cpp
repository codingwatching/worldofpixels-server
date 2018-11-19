#include "Client.hpp"

#include <uWS.h>

#include <misc/utils.hpp>
#include <misc/PrepMsg.hpp>

#include <World.hpp>
#include <Session.hpp>

Client::Client(uWS::WebSocket<uWS::SERVER> * ws, Session& s, Ipv4 ip, Player::Builder& pb)
: ws(ws),
  session(s),
  lastActionOn(jsDateNow()),
  ip(ip),
  pl(pb.setClient(*this)) {
	session.addClient(*this);
}

Client::~Client() {
	session.delClient(*this);
}

void Client::updateLastActionTime() {
	lastActionOn = jsDateNow();
}

bool Client::inactiveKickEnabled() const {
	return true;
}

i64 Client::getLastActionTime() const {
	return lastActionOn;
}

Ipv4 Client::getIp() const {
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
	return ws == c.ws;
}
