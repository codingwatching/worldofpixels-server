#include "Client.hpp"

#include <uWS.h>

#include <misc/utils.hpp>

#include <World.hpp>

Client::Client(uWS::WebSocket<uWS::SERVER> * ws, World& w, UserInfo u, std::string ip)
: ws(ws),
  lastActionOn(jsDateNow()),
  pl(w.getPlayerInstanceForClient(*this)),
  ui(std::move(u)),
  ip(std::move(ip)) { }

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

void Client::send(u8 * buf, sz_t len, bool text) {
	ws->send(reinterpret_cast<const char*>(buf), len, text ? uWS::TEXT : uWS::BINARY);
}

void Client::close() {
	ws->close();
}

bool Client::operator ==(const Client& c) const {
	return ws == c.ws;
}
