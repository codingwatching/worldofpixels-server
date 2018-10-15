#include "Client.hpp"

#include <uWS.h>

#include <misc/utils.hpp>

#include <World.hpp>
#include <Packet.cpp>

Client::Client(uWS::WebSocket<uWS::SERVER> * ws, World& w, Player::Builder& pb, UserInfo u, std::string ip)
: ws(ws),
  lastActionOn(jsDateNow()),
  ui(std::move(u)),
  ip(std::move(ip)),
  pl(pb.setClient(*this)) { }

void Client::updateLastActionTime() {
	lastActionOn = jsDateNow();
}

bool Client::inactiveKickEnabled() const {
	return true;
}

i64 Client::getLastActionTime() const {
	return lastActionOn;
}

const std::string& Client::getIp() const {
	return ip;
}

uWS::WebSocket<true> * Client::getWs() {
	return ws;
}

Player& Client::getPlayer() {
	return pl;
}

UserInfo& Client::getUserInfo() {
	return ui;
}

void Client::send(Packet& p) {
	ws->sendPrepared(static_cast<uWS::WebSocket<uWS::SERVER>::PreparedMessage *>(p.getPrepared()));
}

void Client::close() {
	ws->close();
}

bool Client::operator ==(const Client& c) const {
	return ws == c.ws;
}
