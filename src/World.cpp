#include "World.hpp"

#include <config.hpp>

#include <iostream>

/* World class functions */

World::World(const std::string& path, const std::string& name)
	: updateRequired(false),
	  bgclr(0xFFFFFF),
	  pids(0),
	  paintrate(32),
	  defaultRank(Client::USER),
	  db(path + name + "/"),
	  pass(),
	  name(name) {
	reload();
}

World::~World() {
	for(const auto& chunk : chunks){
		delete chunk.second;
	}
	std::cout << "World unloaded: " << name << std::endl;
}

std::string World::getProp(std::string key, std::string defval) {
	return db.getProp(key, defval);
}

void World::setProp(std::string key, std::string value) {
	db.setProp(key, value);
}

void World::reload() {
	pass = getProp("password");
	try {
		u32 r = stoul(getProp("paintrate", "32"));
		/*u32 p = stoul(getProp("paintper", "0"));*/
		r = r > 0xFFFF ? 0xFFFF : r;
		paintrate = r;
		bgclr = stoul(getProp("bgcolor", "FFFFFF"), nullptr, 16);
		bgclr = (bgclr & 0xFF) << 16 | (bgclr & 0xFF00) | (bgclr & 0xFF0000) >> 16;
	} catch(std::invalid_argument& e) {
		broadcast("DEVException while reloading world properties. (std::invalid_argument)");
	} catch(std::out_of_range& e) {
		broadcast("DEVException while reloading world properties. (std::out_of_range)");
	}
}

void World::update_all_clients() {
	for (auto cli : clients) {
		plupdates.emplace(cli);
	}
}

u32 World::get_id() {
	return ++pids;
}

void World::add_cli(Client * const cl) {
	const std::string motd(getProp("motd"));
	if (motd.size()) {
		cl->tell(motd);
	}
	if (!pass.size()) {
		cl->promote(defaultRank, paintrate);
	} else {
		cl->tell("[Server] This world has a password set. Use '/pass PASSWORD' to unlock drawing.");
		cl->promote(Client::NONE, paintrate);
	}
	clients.emplace(cl);
	update_all_clients();
	sched_updates();
}

void World::upd_cli(Client * const cl) {
	plupdates.emplace(cl);
	sched_updates();
}

void World::rm_cli(Client * const cl) {
	plleft.emplace(cl->id);
	clients.erase(cl);
	plupdates.erase(cl);
	if(!clients.size()){
		return;
	}
	sched_updates();
}

Client * World::get_cli(const u32 id) const {
	for(const auto client : clients){
		if(id == client->id){
			return client;
		}
	}
	return nullptr;
}

Client * World::get_cli(const std::string name) const {
	for(const auto client : clients){
		if(name == client->get_nick()){
			return client;
		}
	}
	return nullptr;
}

void World::sched_updates() {
	updateRequired = true;
}

void World::send_updates() {
	if (!updateRequired) {
		return;
	}
	updateRequired = false;

	sz_t offs = 2;
	u32 tmp;
	u8 * const upd = (u8 *) malloc(1 + 1 + plupdates.size() * (sizeof(u32) + sizeof(pinfo_t))
	                                   + sizeof(u16) + pxupdates.size() * sizeof(pixupd_t)
	                                   + 1 + sizeof(u32) * plleft.size());
	upd[0] = UPDATE;

	bool pendingUpdates = false;

	tmp = 0;
	for (auto it = plupdates.begin();;) {
		if (it == plupdates.end()) {
			plupdates.clear();
			break;
		}
		if(tmp >= WORLD_MAX_PLAYER_UPDATES){
			plupdates.erase(plupdates.begin(), it);
			pendingUpdates = true;
			break;
		}
		auto client = *it;
		memcpy((void *)(upd + offs), (void *)&client->id, sizeof(u32));
		offs += sizeof(u32);
		memcpy((void *)(upd + offs), (void *)client->get_pos(), sizeof(pinfo_t));
		offs += sizeof(pinfo_t);
		++it;
		++tmp;
	}
	upd[1] = tmp;
	tmp = pxupdates.size();
	tmp = tmp >= WORLD_MAX_PIXEL_UPDATES ? WORLD_MAX_PIXEL_UPDATES : tmp;
	memcpy((void *)(upd + offs), &tmp, sizeof(u16));
	tmp = 0;
	offs += sizeof(u16);
	for(auto& px : pxupdates){
		memcpy((void *)(upd + offs), &px, sizeof(pixupd_t));
		offs += sizeof(pixupd_t);
		if(++tmp >= WORLD_MAX_PIXEL_UPDATES){
			break;
		}
	}
	pxupdates.clear();

	tmp = plleft.size();
	tmp = tmp >= WORLD_MAX_PLAYER_LEFT_UPDATES ? WORLD_MAX_PLAYER_LEFT_UPDATES : tmp;
	memcpy((void *)(upd + offs), &tmp, sizeof(u8));
	tmp = 0;
	offs += sizeof(u8);
	for (auto it = plleft.begin();;) {
		if (it == plleft.end()) {
			plleft.clear();
			break;
		}
		if(tmp >= WORLD_MAX_PLAYER_LEFT_UPDATES){
			plleft.erase(plleft.begin(), it);
			pendingUpdates = true;
			break;
		}
		u32 pl = *it;
		memcpy((void *)(upd + offs), &pl, sizeof(u32));
		offs += sizeof(u32);
		++tmp;
		++it;
	}

	uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = uWS::WebSocket<uWS::SERVER>::prepareMessage(
		(char *)upd, offs, uWS::BINARY, false);
	for(auto client : clients){
		client->get_ws()->sendPrepared(prep);
	}
	uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
	free(upd);
	if (pendingUpdates) {
		sched_updates();
	}
}

Chunk * World::get_chunk(const i32 x, const i32 y, bool create) {
	if(x > WORLD_MAX_CHUNK_XY || y > WORLD_MAX_CHUNK_XY
	  || x < ~WORLD_MAX_CHUNK_XY || y < ~WORLD_MAX_CHUNK_XY){
		return nullptr;
	}
	Chunk * chunk = nullptr;
	const auto search = chunks.find(key(x, y));
	if(search == chunks.end()){
		if(chunks.size() > WORLD_MAX_CHUNKS_LOADED){
			auto it = chunks.begin();
			/* expensive, need to figure out another way of limiting loaded chunks? */
			for(auto it2 = chunks.begin(); ++it2 != chunks.end(); ++it);
			delete it->second;
			chunks.erase(it);
		}
		chunk = chunks[key(x, y)] = new Chunk(x, y, bgclr, &db);
	} else {
		chunk = search->second;
	}
	return chunk;
}

void World::send_chunk(uWS::WebSocket<uWS::SERVER> * ws, const i32 x, const i32 y, bool compressed) {
	Chunk * const c = get_chunk(x, y);
	if(c){ c->send_data(ws, compressed); }
}

void World::del_chunk(const i32 x, const i32 y, const RGB clr){
	Chunk * const c = get_chunk(x, y);
	if(c){
		c->clear(clr);
		uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = c->get_prepd_data_msg();
		for(auto client : clients){
			client->get_ws()->sendPrepared(prep);
		}
		uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
	}
}

void World::paste_chunk(const i32 x, const i32 y, char const * const data){
	Chunk * const c = get_chunk(x, y);
	if(c){
		c->set_data(data, 16 * 16 * 3);

		uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = c->get_prepd_data_msg();
		for(auto client : clients){
			client->get_ws()->sendPrepared(prep);
		}
		uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
	}
}

bool World::put_px(const i32 x, const i32 y, const RGB clr, u8 placerRank, u32 id) {
	Chunk * const chunk = get_chunk(x >> 4, y >> 4);
	if(chunk && (!chunk->ranked || placerRank > 1) && chunk->set_data(x & 0xF, y & 0xF, clr)){
		pxupdates.push_back({id, x, y, clr.r, clr.g, clr.b});
		sched_updates();
		return true;
	}
	return false;
}

void World::setChunkProtection(i32 x, i32 y, bool state) {
	db.setChunkProtection(x, y, state);
	Chunk * c = get_chunk(x, y);
	if (c) {
		c->ranked = state;
		u8 msg[10] = {CHUNK_PROTECTED};
		memcpy(&msg[1], (char *)&x, 4);
		memcpy(&msg[5], (char *)&y, 4);
		memcpy(&msg[9], (char *)&state, 1);
		uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = uWS::WebSocket<uWS::SERVER>::prepareMessage(
			(char *)&msg[0], sizeof(msg), uWS::BINARY, false);
		for(auto client : clients){
			client->get_ws()->sendPrepared(prep);
		}
		uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
	}
}

void World::broadcast(const std::string& msg) const {
	uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = uWS::WebSocket<uWS::SERVER>::prepareMessage(
		(char *)msg.c_str(), msg.size(), uWS::TEXT, false);
	for(auto client : clients){
		client->get_ws()->sendPrepared(prep);
	}
	uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
}

void World::save() {
	for(const auto& chunk : chunks){
		chunk.second->save();
	}
	db.save();
}

bool World::is_empty() const {
	return !clients.size();
}

bool World::is_pass(std::string const& p) const {
	return p == pass;
}

bool World::mods_enabled() {
	return getProp("disablemods", "false") != "true";
}

u8 World::get_default_rank() {
	return defaultRank;
}

u16 World::get_paintrate() {
	return paintrate;
}

void World::set_default_rank(u8 r) {
	defaultRank = r;
}

std::set<Client *> * World::get_pl() {
	return &clients;
}
