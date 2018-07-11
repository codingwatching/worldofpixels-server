#include "Client.hpp"
#include "World.hpp"

#include <cmath>
#include <iostream>

#include <misc/utils.hpp>

Client::Client(const u32 id, uWS::WebSocket<uWS::SERVER> * ws, World * const wrld, SocketInfo * si)
		: nick(),
		  dellimit(1, 1),
		  pixupdlimit(0, 1),
		  chatlimit(4, 6),
		  lastMovement(js_date_now() - 900000), /* 5 min timeout for the now-joined players */
		  ws(ws),
		  wrld(wrld),
		  penalty(0),
		  handledelete(true),
		  rank(NONE),
		  stealthadmin(false),
		  suspicious(si->origin != "http://ourworldofpixels.com"),
		  compressionEnabled(false),
		  pos({0, 0, 0, 0, 0, 0}),
		  lastclr({0, 0, 0}),
		  id(id),
		  si(si),
		  mute(false){
	std::cout << "(" << wrld->name << "/" << si->ip << ") New client! ID: " << id << std::endl;
	u8 msg[5] = {SET_ID};
	memcpy(&msg[1], (char *)&id, sizeof(id));
	ws->send((const char *)&msg, sizeof(msg), uWS::BINARY);
}

Client::~Client() {
	wrld->rm_cli(this);
	/* std::cout << "Client deleted! ID: " << id << std::endl; */
}

bool Client::can_edit() {
	return pixupdlimit.can_spend();
}

void Client::get_chunk(const i32 x, const i32 y) const {
	wrld->send_chunk(ws, x, y);
}

void Client::put_px(const i32 x, const i32 y, const RGB clr) {
	if(is_admin() || can_edit()){
		u32 distx = (x >> 4) - (pos.x >> 8); distx *= distx;
		u32 disty = (y >> 4) - (pos.y >> 8); disty *= disty;
		const u32 dist = sqrt(distx + disty);
		const u32 clrdist = ColourDistance(lastclr, clr);
		if(!is_admin() && (dist > 3 || (clrdist != 0 && clrdist < 8))){
			if(warn() || dist > 3){
				return;
			}
		}
		lastclr = clr;
		wrld->put_px(x, y, clr, rank, id);
		updated();
	} else {
		warn();
	}
}

void Client::del_chunk(const i32 x, const i32 y, const RGB clr) {
	if ((is_mod() && dellimit.can_spend()) || is_admin()) {
		wrld->del_chunk(x, y, clr);
	}
}

void Client::teleport(const i32 x, const i32 y) {
	u8 msg[9] = {TELEPORT};
	memcpy(&msg[1], (char *)&x, sizeof(x));
	memcpy(&msg[5], (char *)&y, sizeof(y));
	ws->send((const char *)&msg, sizeof(msg), uWS::BINARY);
	pos.x = (x << 4) + 8;
	pos.y = (y << 4) + 8;
	wrld->upd_cli(this);
}

void Client::move(const pinfo_t& newpos) {
	pos = newpos;
	wrld->upd_cli(this);
	updated();
}

const pinfo_t * Client::get_pos() { /* Hmmm... */
	return &pos;
}

bool Client::can_chat() {
	return is_admin() || chatlimit.can_spend();
}

void Client::chat(const std::string& msg) {
	if (!mute) {
		wrld->broadcast(get_nick() + ": " + msg);
	} else {
		tell(get_nick() + ": " + msg);
	}
}

void Client::tell(const std::string& msg) {
	ws->send(msg.c_str(), msg.size(), uWS::TEXT);
}

void Client::updated() {
	lastMovement = js_date_now();
}

void Client::disconnect() {
	if (!ws->isClosed() && !ws->isShuttingDown()) {
		ws->close();
	}
}

void Client::promote(u8 newrank, u16 prate) {
	if (newrank == rank) {
		return;
	}


	rank = newrank;
	if (rank == ADMIN) {
		tell("Server: You are now an admin. Do /help for a list of commands.");
	} else if (rank == MODERATOR) {
		tell("Server: You are now a moderator.");
		set_pbucket(prate, 2);
	} else if (rank == USER) {
		set_pbucket(prate, 4);
	} else {
		set_pbucket(0, 1);
	}
	u8 msg[2] = {PERMISSIONS, rank};
	ws->send((const char *)&msg, sizeof(msg), uWS::BINARY);
}

bool Client::warn() {
	if(!is_admin() && ++penalty > 128){
		disconnect();
		return true;
	}
	return false;
}

bool Client::is_mod() const {
	return rank == MODERATOR;
}

bool Client::is_admin() const {
	return rank == ADMIN;
}

uWS::WebSocket<uWS::SERVER> * Client::get_ws() const {
	return ws;
}

std::string Client::get_nick() const {
	if (nick.size()) {
		return nick;
	}
	std::string e(is_mod() && !stealthadmin ? "(M) " : is_admin() && !stealthadmin ? "(A) " : "");
	e += std::to_string(id);
	return e;
}

World * Client::get_world() const {
	return wrld;
}

u16 Client::get_penalty() const {
	return penalty;
}

u8 Client::get_rank() const {
	return rank;
}

i64 Client::get_last_move() const {
	return lastMovement;
}

void Client::set_stealth(bool new_state) {
	stealthadmin = new_state;
}

void Client::set_nick(const std::string & name) {
	nick = name;
}

void Client::set_pbucket(u16 rate, u16 per) {
	pixupdlimit.set(rate, per);
	u8 msg[5] = {SET_PQUOTA};
	memcpy(&msg[1], (char *)&rate, sizeof(rate));
	memcpy(&msg[3], (char *)&per, sizeof(per));
	ws->send((const char *)&msg, sizeof(msg), uWS::BINARY);
}
