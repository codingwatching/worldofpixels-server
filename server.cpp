#include "server.hpp"

/* Server class functions */

size_t getUTF8strlen(const std::string& str) {
	size_t j = 0, i = 0, x = 1;
	while (i < str.size()) {
		if (x > 4) { /* Invalid unicode */
			return SIZE_MAX;
		}
		
		if ((str[i] & 0xC0) != 0x80) {
			j += x == 4 ? 2 : 1;
			x = 1;
		} else {
			x++;
		}
		i++;
	}
	if (x == 4) {
		j++;
	}
	return (j);
}

Server::Server(const uint16_t port, const std::string& adminpw, const std::string& path)
	: port(port),
	  adminpw(adminpw),
	  path(path + "/"),
	  cmds(this),
	  h(uWS::NO_DELAY, true) {
	std::cout << "Admin password set to: " << adminpw << "." << std::endl;

	h.onConnection([this](uWS::WebSocket<uWS::SERVER> ws, uWS::UpgradeInfo ui) {
		ws.setUserData(nullptr);
	});
	
	h.onMessage([this](uWS::WebSocket<uWS::SERVER> ws, const char * msg, size_t len, uWS::OpCode oc) {
		Client * const player = (Client *)ws.getUserData();
		if(player && oc == uWS::BINARY){
			switch(len){
				case 8: {
					chunkpos_t pos = *((chunkpos_t *)msg);
					player->get_chunk(pos.x, pos.y);
				} break;

				case 9: {
					if(!player->is_admin()){
						/* No hacks for you */
						player->safedelete(true);
						break;
					}
					chunkpos_t pos = *((chunkpos_t *)msg);
					player->get_world()->del_chunk(pos.x, pos.y);
				} break;
				
				case 11: {
					pixupd_t pos = *((pixupd_t *)msg);
					player->put_px(pos.x, pos.y, {pos.r, pos.g, pos.b});
				} break;
				
				case 12: {
					pinfo_t pos = *((pinfo_t *)msg);
					if(pos.tool <= 2 || (player->is_admin() && pos.tool <= 3)){
						player->move(pos);
					}
				} break;
			};
		} else if(player && oc == uWS::TEXT && len > 1 && msg[len-1] == '\12'){
			std::string mstr(msg, len - 1);
			if(player->can_chat() && getUTF8strlen(mstr) <= 80){
				player->updated();
				if(!(msg[0] == '/' && cmds.exec(player, std::string(msg + 1, len - 2)))){
					player->chat(mstr);
				}
			} else {
				player->warn();
			}
		} else if(!player && oc == uWS::BINARY && len > 2 && len - 2 <= 24
			  && *((uint16_t *)(msg + len - 2)) == 1337){
			join_world(ws, std::string(msg, len - 2));
		} else if(!player){
			ws.close();
		}
	});

	h.onDisconnection([this](uWS::WebSocket<uWS::SERVER> ws, int c, const char * msg, size_t len) {
		Client * const player = (Client *)ws.getUserData();
		if(player){
			World * const w = player->get_world();
			player->safedelete(false);
			if(w && w->is_empty()){
				worlds.erase(w->name);
				w->safedelete();
			}
		}
	});
}

Server::~Server() {
	for(const auto& world : worlds){
		delete world.second;
	}
}

void Server::broadcastmsg(const std::string& msg) {
	h.getDefaultGroup<uWS::SERVER>().broadcast(msg.c_str(), msg.size(), uWS::TEXT);
}

void Server::run() {
	mkdir(path.c_str(), 0700);
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
	for(const auto& world : worlds){
		world.second->save();
	}
	std::cout << "Worlds saved." << std::endl;
}

void Server::save_chunks(uv_timer_t * const t) {
	Server * const srv = (Server *)t->data;
	srv->save_now();
}

void Server::join_world(uWS::WebSocket<uWS::SERVER> ws, const std::string& worldname) {
	/* Validate world name, allowed chars are a..z, 0..9, '_' and '.' */
	for(size_t i = worldname.size(); i--;){
		if(!((worldname[i] > 96 && worldname[i] < 123) ||
		     (worldname[i] > 47 && worldname[i] < 58) ||
		      worldname[i] == 95 || worldname[i] == 46)){
			ws.close();
			return;
		}
	}
	const auto search = worlds.find(worldname);
	World * w = nullptr;
	if(search == worlds.end()){
		worlds[worldname] = w = new World(path, worldname);
	} else {
		w = search->second;
	}
	if(w){
		Client * const cl = new Client(w->get_id(), ws, w, ws.getAddress().address);
		ws.setUserData(cl);
		w->add_cli(cl);
	}
}

bool Server::is_adminpw(const std::string& pw) {
	return pw == adminpw;
}

