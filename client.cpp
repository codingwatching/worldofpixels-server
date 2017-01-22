#include "server.hpp"

#include <cmath>

/* Client class functions */

Client::Client(NetStage * msg_hdlr, uWS::WebSocket<uWS::SERVER> ws, const std::string& ip)
		: msghandler(msg_hdlr),
		  pixupdlimit(CLIENT_PIXEL_UPD_RATELIMIT),
		  chatlimit(CLIENT_CHAT_RATELIMIT),
		  ws(ws),
		  wrld(nullptr),
		  penalty(0),
		  handledelete(true),
		  rank(0),
		  permissions(0),
		  pos({0, 0, 0, 0, 0, 0}),
		  lastclr({0, 0, 0}),
		  id(0),
		  ip(ip) {
	uv_timer_init(uv_default_loop(), &idletimeout_hdl);
	idletimeout_hdl.data = this;
	uv_timer_start(&idletimeout_hdl, (uv_timer_cb) &Client::idle_timeout, 300000, 1200000);
}

Client::~Client() {
	/* std::cout << "Client deleted! ID: " << id << std::endl; */
}

void Client::msg(const char * m, const size_t len) {
	msghandler->process_msg(this, m, len);
}

bool Client::can_edit() {
	return pixupdlimit.can_spend();
}

void Client::get_chunk(const int32_t x, const int32_t y) const {
	if(wrld){
		wrld->send_chunk(ws, x, y);
	}
}

void Client::put_px(const int32_t x, const int32_t y, const RGB clr) {
	if(can_edit() && wrld){
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
	if(wrld){
		uint8_t msg[9] = {TELEPORT};
		memcpy(&msg[1], (char *)&x, sizeof(x));
		memcpy(&msg[5], (char *)&y, sizeof(y));
		ws.send((const char *)&msg, sizeof(msg), uWS::BINARY);
		pos.x = (x << 4) + 8;
		pos.y = (y << 4) + 8;
		wrld->upd_cli(this);
	}
}

void Client::move(const pinfo_t& newpos) {
	if(wrld){
		pos = newpos;
		wrld->upd_cli(this);
		updated();
	}
}

const pinfo_t * Client::get_pos() { /* Hmmm... */
	return &pos;
}

bool Client::can_chat() {
	return chatlimit.can_spend();
}

void Client::chat(const std::string& msg) {
	if(wrld){
		wrld->broadcast(get_nick() + ": " + msg);
	}
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
		if(wrld){
			wrld->rm_cli(this);
		}
		uv_timer_stop(&idletimeout_hdl);
		uv_close((uv_handle_t *)&idletimeout_hdl, (uv_close_cb)([](uv_handle_t * const t){
			delete (Client *)t->data;
		}));
		if(close){
			ws.close();
		}
	}
}

void Client::send_cli_perms() {
	uint8_t msg[10] = {PERMISSIONS, rank};
	const uint16_t chatrate = chatlimit.get_rate();
	const uint16_t pixlrate = pixupdlimit.get_rate();
	memcpy((char *)&msg[2], (char *)&permissions, sizeof(uint32_t));
	memcpy((char *)&msg[6], (char *)&pixlrate, sizeof(uint16_t));
	memcpy((char *)&msg[8], (char *)&chatrate, sizeof(uint16_t));
	ws.send((const char *)&msg, sizeof(msg), uWS::BINARY);
}

void Client::change_limits(const uint16_t rate, bool chat) {
	if(chat){
		chatlimit.change_rate(rate);
	} else {
		pixupdlimit.change_rate(rate);
	}
}

void Client::promote(const uint8_t newrank) {
	if(rank != newrank){
		rank = newrank;
		if(rank == ADMIN){
			tell("Server: You are now an admin. Do /help for a list of commands.");
		}
		send_cli_perms();
	}
}

bool Client::warn() {
	if(++penalty > CLIENT_MAX_WARN_LEVEL){
		safedelete(true);
		return true;
	}
	return false;
}

bool Client::is_admin() const {
	return rank == ADMIN;
}

uWS::WebSocket<uWS::SERVER> Client::get_ws() const {
	return ws;
}

std::string Client::get_nick() const {
	return (is_admin() ? "(A) " + name : name);
}

World * Client::get_world() const {
	return wrld;
}

uint32_t Client::get_id() const {
	return id;
}

void Client::set_id(const uint32_t newid) {
	id = newid;
}

void Client::set_world(World * const newworld) {
	wrld = newworld;
	if(wrld){
		size_t len = wrld->name.size();
		uint8_t lenu8 = len >= 255 ? 255 : len;
		char * msg = (char *)malloc(sizeof(uint8_t) * 2 + len);
		const char * name = wrld->name.c_str();
		msg[0] = JOIN_WORLD;
		msg[1] = lenu8;
		memcpy(msg + 2, name, len);
		wrld->add_cli(this);
		ws.send((const char *)msg, len + sizeof(uint8_t) * 2, uWS::BINARY);
		free(msg);
	}
}

void Client::set_name(const std::string& newname) {
	name = newname;
}

void Client::set_msg_handler(NetStage * const newhandler) {
	msghandler = newhandler;
}
