#include "Client.hpp"

#include <uWS.h>

#include <misc/utils.hpp>

#include <World.hpp>

UserInfo::UserInfo()
: uid(1),
  username("Guest"),
  isGuest(true) { }

UserInfo::UserInfo(u64 uid, std::string s)
: uid(uid),
  username(std::move(s)),
  isGuest(false) { }

Client::Client(uWS::WebSocket<uWS::SERVER> * ws, World& w, Player::Builder& pb, UserInfo u, std::string ip)
: ws(ws),
  lastActionOn(jsDateNow()),
  ui(std::move(u)),
  ip(std::move(ip)),
  pl(pb.build()) { }

void Client::updateLastActionTime() {
	lastActionOn = jsDateNow();
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

void Client::send(const u8 * buf, sz_t len, bool text) {
	ws->send(reinterpret_cast<const char*>(buf), len, text ? uWS::TEXT : uWS::BINARY);
}

void Client::close() {
	ws->close();
}

bool Client::operator ==(const Client& c) const {
	return ws == c.ws;
}
