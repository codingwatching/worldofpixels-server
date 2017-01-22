#include "server.hpp"

/* Chunk class functions */

Chunk::Chunk(const int32_t cx, const int32_t cy, Database * const db)
	: db(db),
	  cx(cx),
	  cy(cy),
	  changed(false) {
	if(!db->get_chunk(cx, cy, (char *)&data)){
		memset(data, 255, sizeof(data));
	}
}

Chunk::~Chunk() {
	save();
}

bool Chunk::set_data(const uint8_t x, const uint8_t y, const RGB clr) {
	const uint16_t pos = (y * 16 + x) * 3;
	if(data[pos] == clr.r && data[pos + 1] == clr.g && data[pos + 2] == clr.b){
		return false;
	}
	data[pos] = clr.r;
	data[pos + 1] = clr.g;
	data[pos + 2] = clr.b;
	changed = true;
	return true;
}
	
void Chunk::send_data(uWS::WebSocket<uWS::SERVER> ws) {
	uint8_t msg[16 * 16 * 3 + 9];
	msg[0] = CHUNK_DATA;
	memcpy(&msg[1], &cx, 4);
	memcpy(&msg[5], &cy, 4);
	memcpy(&msg[9], &data[0], sizeof(data));
	ws.send((const char *)&msg[0], sizeof(msg), uWS::BINARY);
}

void Chunk::save() {
	if(changed){
		changed = false;
		db->set_chunk(cx, cy, (char *)&data);
		/* std::cout << "Chunk saved at X: " << cx << ", Y: " << cy << std::endl; */
	}
}

void Chunk::clear(){
	memset(data, 255, sizeof(data));
	changed = true;
}

/* World class functions */

World::World(const std::string& path, const std::string& name)
	: db(path + name + "/"),
	  name(name) {
	uv_timer_init(uv_default_loop(), &upd_hdl);
	upd_hdl.data = this;
}

World::~World() {
	for(const auto& chunk : chunks){
		delete chunk.second;
	}
	std::cout << "World unloaded: " << name << std::endl;
}

void World::add_cli(Client * const cl) {
	clients.emplace(cl);
	plupdates.emplace(cl);
	sched_updates();
}

void World::upd_cli(Client * const cl) {
	plupdates.emplace(cl);
	sched_updates();
}

void World::rm_cli(Client * const cl) {
	plleft.emplace(cl->get_id());
	clients.erase(cl);
	plupdates.erase(cl);
	if(!clients.size()){
		return;
	}
	sched_updates();
}

Client * World::get_cli(const uint32_t id) const {
	for(const auto client : clients){
		if(id == client->get_id()){
			return client;
		}
	}
	return nullptr;
}

void World::sched_updates() {
	if(!uv_is_active((uv_handle_t *)&upd_hdl)){
		uv_timer_start(&upd_hdl, (uv_timer_cb)&send_updates, WORLD_UPDATE_RATE_MSEC, 0);
	}
}

void World::send_updates(uv_timer_t * const t) {
	World * const wrld = (World *) t->data;
	size_t offs = 2;
	uint32_t tmp = 0;
	uint8_t * const upd = (uint8_t *) malloc(1 + 1 + wrld->plupdates.size() * (sizeof(uint32_t) + sizeof(pinfo_t))
	                                   + sizeof(uint16_t) + wrld->pxupdates.size() * sizeof(pixupd_t)
	                                   + 1 + sizeof(uint32_t) * wrld->plleft.size());
	upd[0] = WORLD_UPDATE;
	for(auto client : wrld->plupdates){
		uint32_t id = client->get_id();
		memcpy((void *)(upd + offs), (void *)&id, sizeof(uint32_t));
		offs += sizeof(uint32_t);
		memcpy((void *)(upd + offs), (void *)client->get_pos(), sizeof(pinfo_t));
		offs += sizeof(pinfo_t);
		if(++tmp >= WORLD_MAX_PLAYER_UPDATES){
			break;
		}
	}
	wrld->plupdates.clear();
	upd[1] = tmp;
	tmp = wrld->pxupdates.size();
	tmp = tmp >= WORLD_MAX_PIXEL_UPDATES ? WORLD_MAX_PIXEL_UPDATES : tmp;
	memcpy((void *)(upd + offs), &tmp, sizeof(uint16_t));
	tmp = 0;
	offs += sizeof(uint16_t);
	for(auto& px : wrld->pxupdates){
		memcpy((void *)(upd + offs), &px, sizeof(pixupd_t));
		offs += sizeof(pixupd_t);
		if(++tmp >= WORLD_MAX_PIXEL_UPDATES){
			break;
		}
	}
	wrld->pxupdates.clear();
	
	tmp = wrld->plleft.size();
	tmp = tmp >= WORLD_MAX_PLAYER_LEFT_UPDATES ? WORLD_MAX_PLAYER_LEFT_UPDATES : tmp;
	memcpy((void *)(upd + offs), &tmp, sizeof(uint8_t));
	tmp = 0;
	offs += sizeof(uint8_t);
	for(auto pl : wrld->plleft){
		memcpy((void *)(upd + offs), &pl, sizeof(uint32_t));
		offs += sizeof(uint32_t);
		if(++tmp >= WORLD_MAX_PLAYER_LEFT_UPDATES){
			break;
		}
	}
	wrld->plleft.clear();
	
	uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = uWS::WebSocket<uWS::SERVER>::prepareMessage(
		(char *)upd, offs, uWS::BINARY, false);
	for(auto client : wrld->clients){
		client->get_ws().sendPrepared(prep);
	}
	uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
	free(upd);
}

Chunk * World::get_chunk(const int32_t x, const int32_t y) {
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
		chunk = chunks[key(x, y)] = new Chunk(x, y, &db);
	} else {
		chunk = search->second;
	}
	return chunk;
}

void World::send_chunk(uWS::WebSocket<uWS::SERVER> ws, const int32_t x, const int32_t y) {
	Chunk * const c = get_chunk(x, y);
	if(c){ c->send_data(ws); }
}

void World::del_chunk(const int32_t x, const int32_t y){
	Chunk * const c = get_chunk(x, y);
	if(c){
		c->clear();
		/* Could be optimized a lot */
		for(auto client : clients){
			c->send_data(client->get_ws());
		}
	}
}

bool World::put_px(const int32_t x, const int32_t y, const RGB clr) {
	Chunk * const chunk = get_chunk(x >> 4, y >> 4);
	if(chunk && chunk->set_data(x & 0xF, y & 0xF, clr)){
		pxupdates.push_back({x, y, clr.r, clr.g, clr.b});
		sched_updates();
		return true;
	}
	return false;
}

void World::safedelete() {
	uv_timer_stop(&upd_hdl);
	uv_close((uv_handle_t *)&upd_hdl, (uv_close_cb)([](uv_handle_t * const t){
		delete (World *)t->data;
	}));
}

void World::broadcast(const std::string& msg) const {
	uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = uWS::WebSocket<uWS::SERVER>::prepareMessage(
		(char *)msg.c_str(), msg.size(), uWS::TEXT, false);
	for(auto client : clients){
		client->get_ws().sendPrepared(prep);
	}
	uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
}

void World::save() const {
	for(const auto& chunk : chunks){
		chunk.second->save();
	}
}

bool World::is_empty() const {
	return !clients.size();
}

/* WorldManager class functions */

WorldManager::WorldManager(const std::string& path)
	: path(path) { }

WorldManager::~WorldManager() {
	for(auto& world : worlds){
		delete world.second;
	}
}

bool WorldManager::join_world(Client * const cl, const std::string& name) {
	World * const lastworld = cl->get_world();
	if(lastworld){
		lastworld->rm_cli(cl);
		if(lastworld->is_empty()){
			lastworld->safedelete();
		}
	}
	if(!validate_name(name)){
		return true;
	}
	World * const w = get_world(name);
	if(w){
		cl->set_world(w); // <-- !!
		return false;
	}
	return true;
}

World * WorldManager::get_world(const std::string& name) {
	const auto search = worlds.find(name);
	World * w = nullptr;
	if(search == worlds.end()){
		worlds[name] = w = new World(path, name);
	} else {
		w = search->second;
	}
	return w;
}

void WorldManager::rm_world(World * const world) {
	worlds.erase(world->name);
	world->safedelete();
}

void WorldManager::save() const {
	for(const auto& world : worlds){
		world.second->save();
	}
}
