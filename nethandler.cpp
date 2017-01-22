#include "server.hpp"

NetStage::NetStage(std::vector<std::function<void(Client * const, const char *, const size_t)>> funcs,
                   NetStage * const next_stage)
	: netmsgs(funcs),
	  next_stage(next_stage) { }

void NetStage::process_msg(Client * const cl, const char * msg, const size_t len) {
	uint8_t type = msg[0];
	if(type < netmsgs.size()){
		netmsgs[type](cl, msg + 1, len - 1);
	}
}

void NetStage::upgrade(Client * const cl) {
	if(next_stage){
		cl->set_msg_handler(next_stage);
	}
}

/* Network packet handlers */

NetStageLogin::NetStageLogin(NetStage * const next_stage, LoginManager * const lm)
	: NetStage({
	    std::bind(NetStageLogin::login_user, (NetStage *)this, lm,
	              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),

	    std::bind(NetStageLogin::register_user, (NetStage *)this, lm,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),

	    std::bind(NetStageLogin::login_guest, (NetStage *)this, lm,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
	  }, next_stage) { }

void NetStageLogin::login_user(NetStage * const s, LoginManager * const lm,
                               Client * const cl, const char * msg, const size_t len) {
	std::cout << "Login!" << std::endl;
	uint8_t namelength = msg[0];
	if(namelength <= 2 || len <= namelength + 1U){
		return;
	}
	uint8_t passlength = msg[namelength];
	if(passlength <= 4 || len != namelength + passlength + 2U){
		return;
	}
	std::string user(msg + 1, namelength);
	std::string pass(msg + 2 + namelength, passlength);
	std::cout << user << ", " << pass << std::endl;
}

void NetStageLogin::register_user(NetStage * const s, LoginManager * const lm,
                                  Client * const cl, const char * msg, const size_t len) {
	std::cout << "Register!" << std::endl;
}

void NetStageLogin::login_guest(NetStage * const s, LoginManager * const lm,
                                Client * const cl, const char * msg, const size_t len) {
	std::cout << "Guest!" << std::endl;
	if(cl->get_id() == 0){ /* probably not needed */
		logininfo_t linfo = lm->guest_login();
		std::cout << linfo.name << std::endl;
		cl->set_id(linfo.id);
		cl->set_name(linfo.name);
		cl->updated();
		s->upgrade(cl);
	}
}

NetStagePlay::NetStagePlay(WorldManager * const worlds)
	: NetStage({
	    std::bind(NetStagePlay::req_chunk, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
	    std::bind(NetStagePlay::put_pixel, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
	    std::bind(NetStagePlay::player_upd, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
	    std::bind(NetStagePlay::fill_chunk, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
	    std::bind(NetStagePlay::send_chat, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
	    std::bind(NetStagePlay::change_world, worlds, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
	  }, nullptr) { }

void NetStagePlay::req_chunk(Client * const cl, const char * msg, const size_t len) {
	std::cout << "Request!" << std::endl;
	if(len >= 8){ 
		chunkpos_t pos = *((chunkpos_t *)msg);
		cl->get_chunk(pos.x, pos.y);
	}
}

void NetStagePlay::put_pixel(Client * const cl, const char * msg, const size_t len) {
	std::cout << "Paint!" << std::endl;
	if(len >= 11){
		pixupd_t pos = *((pixupd_t *)msg);
		cl->put_px(pos.x, pos.y, {pos.r, pos.g, pos.b});
	}
}

void NetStagePlay::player_upd(Client * const cl, const char * msg, const size_t len) {
	std::cout << "Moved!" << std::endl;
	if(len == 12){
		pinfo_t pos = *((pinfo_t *)msg);
		if(pos.tool <= 3 || ((pos.tool & 128) && pos.tool <= 131 && cl->is_admin())){
			cl->move(pos);
		}
	}
}

void NetStagePlay::fill_chunk(Client * const cl, const char * msg, const size_t len) {
	std::cout << "Fill!" << std::endl; // not really
	if(!cl->is_admin()){
		/* No hacks for you */
		cl->safedelete(true);
		return;
	}
	chunkpos_t pos = *((chunkpos_t *)msg);
	World * w = cl->get_world();
	if(w){
		w->del_chunk(pos.x, pos.y);
	}
}

void NetStagePlay::send_chat(Client * const cl, const char * msg, const size_t len) {
	std::string mstr(msg, len);
	std::cout << "Chat! " << mstr << std::endl;
	if(cl->can_chat() && getUTF8strlen(mstr) <= 80){
		cl->updated();
		if(!(msg[0] == '/' /*&& cmds.exec(cl, std::string(msg + 1, len - 2))*/)){
			cl->chat(mstr);
		}
	} else {
		cl->warn();
	}
}

void NetStagePlay::change_world(WorldManager * const s, Client * const cl, const char * msg, const size_t len) {
	std::cout << "Change world!" << std::endl;
	std::string worldname(msg, len);
	if(!s->join_world(cl, worldname)){
		cl->safedelete(true);
	}
}
