#include "server.hpp"

/* Server class functions */

Server::Server(const uint16_t port, const std::string& adminpw, const std::string& path)
	: port(port),
	  adminpw(adminpw),
	  cmds(this),
	  worlds(path + "/"),
	  h(uWS::NO_DELAY, true),
	  lm(path + "/"),
	  netplay(&worlds),
	  netlogin((NetStage *)&netplay, &lm) {
	mkdir(path.c_str(), 0700);
	std::cout << "Admin password set to: " << adminpw << "." << std::endl;

	h.onConnection([this](uWS::WebSocket<uWS::SERVER> ws, uWS::UpgradeInfo ui) {
		Client * const cl = new Client((NetStage *)&netlogin, ws, ws.getAddress().address);
		if(!cl){
			std::cout << "Prepare for trouble!" << std::endl;
			ws.close();
			return;
		}
		ws.setUserData(cl);
	});
	
	h.onMessage([this](uWS::WebSocket<uWS::SERVER> ws, const char * msg, size_t len, uWS::OpCode oc) {
		Client * const player = (Client *)ws.getUserData();
		if(oc == uWS::BINARY && len > 1){
			player->msg(msg, len);
		}
	});

	h.onDisconnection([this](uWS::WebSocket<uWS::SERVER> ws, int c, const char * msg, size_t len) {
		Client * const player = (Client *)ws.getUserData();
		if(player){
			World * const w = player->get_world();
			uint32_t id = player->get_id();
			if(id != 0){
				lm.logout(id);
			}
			player->safedelete(false);
			if(w && w->is_empty()){
				worlds.rm_world(w);
			}
		}
	});
}

Server::~Server() { }

void Server::broadcastmsg(const std::string& msg) {
	h.getDefaultGroup<uWS::SERVER>().broadcast(msg.c_str(), msg.size(), uWS::TEXT);
}

void Server::run() {
	uv_timer_init(uv_default_loop(), &save_hdl);
	save_hdl.data = this;
	uv_timer_start(&save_hdl, (uv_timer_cb)&save_chunks, 900000, 900000);
	h.listen(port);
	h.run();
}

void Server::quit() {
	if(!uv_is_closing((uv_handle_t *)&save_hdl)){
		broadcastmsg("Server: Shutting down! (Rejoin in ~5 minutes)");
		uv_timer_stop(&save_hdl);
		uv_close((uv_handle_t *)&save_hdl, (uv_close_cb)([](uv_handle_t * const t){
			delete (Server *)t->data;
			exit(0);
		}));
	} else { /* uv_close gets stuck? */
		delete this;
		exit(0);
	}
}

void Server::save_now() const {
	worlds.save();
	std::cout << "Worlds saved." << std::endl;
}

void Server::save_chunks(uv_timer_t * const t) {
	Server * const srv = (Server *)t->data;
	srv->save_now();
}

bool Server::is_adminpw(const std::string& pw) {
	return pw == adminpw;
}

