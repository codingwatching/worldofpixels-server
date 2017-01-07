#include "server.hpp"

#include <cmath>

/* Client class functions */

Client::Client(const uint32_t id, uWS::WebSocket<uWS::SERVER> ws, World * const wrld, const std::string& ip)
		: pixupdlimit(CLIENT_PIXEL_UPD_RATELIMIT),
		  chatlimit(CLIENT_CHAT_RATELIMIT),
		  ws(ws),
		  wrld(wrld),
		  penalty(0),
		  handledelete(true),
		  admin(false),
		  pos({0, 0, 0, 0, 0, 0}),
		  lastclr({0, 0, 0}),
		  id(id),
		  ip(ip) {
	std::cout << "(" << wrld->name << ") New client! ID: " << id << std::endl;
	uv_timer_init(uv_default_loop(), &idletimeout_hdl);
	idletimeout_hdl.data = this;
	uv_timer_start(&idletimeout_hdl, (uv_timer_cb) &Client::idle_timeout, 300000, 1200000);
	uint8_t msg[5] = {SET_ID};
	memcpy(&msg[1], (char *)&id, sizeof(id));
	ws.send((const char *)&msg, sizeof(msg), uWS::BINARY);
}

Client::~Client() {
	/* std::cout << "Client deleted! ID: " << id << std::endl; */
}

bool Client::can_edit() {
	return pixupdlimit.can_spend();
}

void Client::get_chunk(const int32_t x, const int32_t y) const {
	wrld->send_chunk(ws, x, y);
}

void Client::put_px(const int32_t x, const int32_t y, const RGB clr) {
	if(can_edit()){
		uint32_t distx = (x >> 4) - (pos.x >> 8); distx *= distx;
		uint32_t disty = (y >> 4) - (pos.y >> 8); disty *= disty;
		const uint32_t dist = sqrt(distx + disty);
		const uint32_t clrdist = ColourDistance(lastclr, clr);
		if(dist > 3 || (clrdist != 0 && clrdist < 8)){
			if(warn() || dist > 3){
				return;
			}
		}
		lastclr = clr;
		wrld->put_px(x, y, clr);
		updated();
	} else {
		warn();
	}
}

void Client::teleport(const int32_t x, const int32_t y) {
	uint8_t msg[9] = {TELEPORT};
	memcpy(&msg[1], (char *)&x, sizeof(x));
	memcpy(&msg[5], (char *)&y, sizeof(y));
	ws.send((const char *)&msg, sizeof(msg), uWS::BINARY);
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
	return chatlimit.can_spend();
}

void Client::chat(const std::string& msg) {
	wrld->broadcast(get_nick() + ": " + msg);
}

void Client::tell(const std::string& msg) {
	ws.send(msg.c_str(), msg.size(), uWS::TEXT);
}

void Client::updated() {
	uv_timer_again(&idletimeout_hdl);
}

void Client::idle_timeout(uv_timer_t * const t) {
	Client * const cl = (Client *)t->data;
	/* Maybe there should be a proper kick function */
	cl->tell("Server: Kicked for inactivity.");
	cl->safedelete(true);
}

void Client::safedelete(const bool close) {
	if(handledelete){
		handledelete = false;
		wrld->rm_cli(this);
		uv_timer_stop(&idletimeout_hdl);
		uv_close((uv_handle_t *)&idletimeout_hdl, (uv_close_cb)([](uv_handle_t * const t){
			delete (Client *)t->data;
		}));
		if(close){
			ws.close();
		}
	}
}

void Client::promote() {
	admin = true;
	tell("Server: You are now an admin. Do /help for a list of commands.");
	uint8_t msg[2] = {PERMISSIONS, 0}; /* Second byte doesn't do anything for now */
	ws.send((const char *)&msg, sizeof(msg), uWS::BINARY);
}

bool Client::warn() {
	if(++penalty > CLIENT_MAX_WARN_LEVEL){
		safedelete(true);
		return true;
	}
	return false;
}

bool Client::is_admin() const {
	return admin;
}

uWS::WebSocket<uWS::SERVER> Client::get_ws() const {
	return ws;
}

std::string Client::get_nick() const {
	return (admin ? "(A) " + std::to_string(id) : std::to_string(id));
}

World * Client::get_world() const {
	return wrld;
}
