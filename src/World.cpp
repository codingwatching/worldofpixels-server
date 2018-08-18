#include "World.hpp"

#include <uWS.h>

#include <config.hpp>
#include <Client.hpp>

#include <misc/TaskBuffer.hpp>

#include <iostream>
#include <utility>
#include <algorithm>

// two 32 bit signed ints to one unsiged 64 bit int
u64 key(i32 x, i32 y) {
	union {
		struct {
			i32 x;
			i32 y;
		} p;
		u64 pos;
	} s;
	s.p.x = x;
	s.p.y = y;
	return s.pos;
}

/* World class functions */

World::World(WorldStorage ws, TaskBuffer& tb)
: WorldStorage(std::move(ws)),
  tb(tb),
  updateRequired(false),
  drawRestricted(false) {
	convertProtectionData([this](i32 x, i32 y) {
		setChunkProtection(x, y, true);
	});
}

World::~World() {
	std::cout << "World unloaded: " << getWorldName() << std::endl;
}

void World::setUnloadFunc(std::function<void()> unloadFunc) {
	unload = std::move(unloadFunc);
}

sz_t World::unloadOldChunks(bool force) {
	sz_t unloadCount = 0;

	//auto oldest = chunks.end();

	for (auto it = chunks.begin(); it != chunks.end();) {
		if (it->second.shouldUnload(force)) {
			/*if (force && (oldest == chunks.end() || it->second.getLastActionTime() < oldest->second.getLastActionTime())) {
				oldest = it;
			} else {*/
				it = chunks.erase(it);
				++unloadCount;
			//}
		} else {
			++it;
		}
	}
/*
	if (oldest != chunks.end()) {
		chunks.erase(oldest);
		return 1;
	}
*/
	return unloadCount;
}

void World::update_all_clients() {
	for (auto cli : clients) {
		plupdates.emplace(cli);
	}
}

void World::add_cli(Client * const cl) {
	u16 paintrate = getPixelRate();

	if (hasMotd()) {
		cl->tell(getMotd());
	}

	if (!hasPassword()) {
		cl->promote(drawRestricted ? Client::NONE : Client::USER, paintrate);
	} else {
		cl->tell("[Server] This world has a password set. Use '/pass PASSWORD' to unlock drawing.");
		cl->promote(Client::NONE, paintrate);
	}
	clients.emplace(cl);
	cl->set_id(ids.getId());
	update_all_clients();
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
	ids.freeId(cl->get_id());
	sched_updates();
	if (!clients.size()) {
		tryUnload();
	}
}

Client * World::get_cli(const u32 id) const {
	for(const auto client : clients){
		if(id == client->get_id()){
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

	u8 buf[1 + 1 + plupdates.size() * (sizeof(u32) + sizeof(pinfo_t))
		+ sizeof(u16) + pxupdates.size() * sizeof(pixupd_t)
		+ 1 + sizeof(u32) * plleft.size()];
	u8 * const upd = &buf[0];

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
		u32 id = client->get_id();
		memcpy((void *)(upd + offs), (void *)&id, sizeof(u32));
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
	if (pendingUpdates) {
		sched_updates();
	}
}

const std::unordered_map<u64, Chunk>::iterator World::get_chunk(const i32 x, const i32 y, bool create) {
	if (x > 0x7FFF || y > 0x7FFF
	 || x < ~0x7FFF || y < ~0x7FFF) {
		return chunks.end();
	}
	auto search = chunks.find(key(x, y));
	if (search == chunks.end()) {
		if (create) {
			WorldStorage& ws = *this;
			search = chunks.emplace(std::piecewise_construct,
				std::forward_as_tuple(key(x, y)),
				std::forward_as_tuple(x, y, ws)).first;
			if (chunks.size() > 64) {
				search->second.preventUnloading(true);
				unloadOldChunks(true);
				search->second.preventUnloading(false);
			}
		}
	}

	return search;
}

void World::send_chunk(uWS::HttpResponse * res, i32 x, i32 y) {
	// will load the chunk if unloaded... possible race with nginx if not
	auto chunk = get_chunk(x, y);
	if (chunk == chunks.end()) {
		res->end();
		return;
	}

	if (!chunk->second.isPngCacheOutdated()) {
		const auto& d = chunk->second.getPngData();
		res->end(reinterpret_cast<const char *>(d.data()), d.size());
		return;
	}

	u64 k = key(x, y);
	auto search = ongoingChunkRequests.find(k);
	if (search == ongoingChunkRequests.end()) {
		Chunk& c = chunk->second;
		c.preventUnloading(true);

		search = ongoingChunkRequests.emplace(std::piecewise_construct,
			std::forward_as_tuple(k),
			std::forward_as_tuple(std::initializer_list<uWS::HttpResponse *>{res})).first;

		auto end = [this, search, &c](TaskBuffer& tb) {
			const auto& d = c.getPngData();
			for (uWS::HttpResponse * res : search->second) {
				res->end(reinterpret_cast<const char *>(d.data()), d.size());
			}

			ongoingChunkRequests.erase(search);
			c.preventUnloading(false);
			c.unsetCacheOutdatedFlag();
			tryUnload();
		};

		tb.queue([&c, end](TaskBuffer& tb) {
			c.updatePngCache();
			tb.runInMainThread(std::move(end));
		});
	} else {
		// add this request to the list, if a png is already being encoded
		search->second.emplace_back(res);
	}
}

void World::del_chunk(const i32 x, const i32 y, const RGB clr){
	#warning "FIXME"
	/*Chunk * const c = get_chunk(x, y);
	if(c){
		c->clear(clr);
		uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = c->get_prepd_data_msg();
		for(auto client : clients){
			client->get_ws()->sendPrepared(prep);
		}
		uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
	}*/
}

void World::paste_chunk(const i32 x, const i32 y, char const * const data){
	#warning "FIXME"
	/*Chunk * const c = get_chunk(x, y);
	if(c){
		c->set_data(data, 16 * 16 * 3);

		uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = c->get_prepd_data_msg();
		for(auto client : clients){
			client->get_ws()->sendPrepared(prep);
		}
		uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
	}*/
}

bool World::put_px(const i32 x, const i32 y, const RGB clr, u8 placerRank, u32 id) {
	auto chunk = get_chunk(x >> 9, y >> 9);
	if (chunk == chunks.end()) {
		return false;
	}

	if (isActionPaintAllowed(chunk->second, x, y, placerRank) && chunk->second.setPixel(x & 0x1FF, y & 0x1FF, clr)){
		pxupdates.push_back({id, x, y, clr.r, clr.g, clr.b});
		sched_updates();
		return true;
	}

	return false;
}

void World::setChunkProtection(i32 x, i32 y, bool state) {
	auto chunk = get_chunk(x >> 5, y >> 5); // div by 32
	if (chunk == chunks.end()) {
		return;
	}

	// x and y are 16x16 aligned
	chunk->second.setProtectionGid(x & 0x1F, y & 0x1F, state ? 1 : 0);

	if (clients.size() != 0) {
		u8 msg[10] = {CHUNK_PROTECTED};
		memcpy(&msg[1], (char *)&x, 4);
		memcpy(&msg[5], (char *)&y, 4);
		memcpy(&msg[9], (char *)&state, 1);
		uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = uWS::WebSocket<uWS::SERVER>::prepareMessage(
			(char *)&msg[0], sizeof(msg), uWS::BINARY, false);
		for (auto client : clients) {
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

bool World::save() {
	bool didStuff = false;
	for (auto& chunk : chunks) {
		didStuff |= chunk.second.save();
	}

	didStuff |= WorldStorage::save();
	return didStuff;
}

bool World::is_empty() const {
	return !clients.size();
}

bool World::is_pass(std::string const& p) {
	return hasPassword() && p == getPassword();
}

bool World::mods_enabled() {
	return getGlobalModeratorsAllowed();
}

u8 World::get_default_rank() {
	return drawRestricted ? Client::NONE : Client::USER;
}

u16 World::get_paintrate() {
	return getPixelRate();
}

void World::restrictDrawing(bool s) {
	drawRestricted = s;
}

std::set<Client *> * World::get_pl() {
	return &clients;
}

bool World::isActionPaintAllowed(const Chunk& c, i32 x, i32 y, u8 rank) {
	x >>= 4; x &= 0xF; // divide by 16 and mod 16
	y >>= 4; y &= 0xF;
	return c.getProtectionGid(x, y) == 0 || rank >= Client::MODERATOR;
}

bool World::tryUnloadAllChunks() {
	for (auto it = chunks.begin(); it != chunks.end();) {
		it = it->second.shouldUnload(true) ? chunks.erase(it) : std::next(it);
	}
	return chunks.size() == 0;
}

void World::tryUnload() {
	if (!clients.size() && tryUnloadAllChunks()) {
		unload();
	}
}
