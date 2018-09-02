#include "World.hpp"

#include <uWS.h>

#include <Client.hpp>
#include <config.hpp>

#include <misc/TaskBuffer.hpp>

#include <iostream>
#include <utility>
#include <algorithm>

bool operator  <(std::reference_wrapper<Player> a, std::reference_wrapper<Player> b) {
	return a.get() < b.get();
}

bool operator ==(std::reference_wrapper<Player> a, std::reference_wrapper<Player> b) {
	return a.get() == b.get();
}

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

World::World(std::tuple<std::string, std::string> wsArgs, TaskBuffer& tb)
: WorldStorage(std::move(wsArgs)),
  tb(tb),
  updateRequired(false),
  drawRestricted(false) { }

World::~World() {
	std::cout << "World unloaded: " << getWorldName() << std::endl;
}

void World::setUnloadFunc(std::function<void()> unloadFunc) {
	unload = std::move(unloadFunc);
}

sz_t World::unloadOldChunks(bool force) { // TODO: handle force flag better
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

void World::configurePlayerBuilder(Player::Builder& pb) {
	pb.setWorld(*this)
	  .setSpawnPoint(0, 0)
	  .setPlayerId(ids.getId())
	  .setPaintBucket({getPixelRate(), 3})
	  .setChatBucket({4, 6})
	  .setModifyWorldAllowed(!hasPassword());
}

void World::playerJoined(Player& pl) {
	if (hasMotd()) {
		pl.tell(getMotd());
	}

	if (hasPassword()) {
		pl.tell("This world has a password set. Use '/pass PASSWORD' to unlock drawing.");
	}

	players.emplace(std::ref(pl));
	playerUpdated(pl);
}

void World::playerUpdated(Player& pl) {
	playerUpdates.emplace(std::ref(pl));
	schedUpdates();
}

void World::playerLeft(Player& pl) {
	// XXX: a client could immediately join with the same pid
	// solution: move player lefts at the beginning of the update array
	playersLeft.emplace(pl.getPid());
	ids.freeId(pl.getPid());
	players.erase(std::ref(pl));
	playerUpdates.erase(std::ref(pl));
	schedUpdates();
	if (players.size() == 0) {
		tryUnloadWorld();
	}
}

void World::schedUpdates() {
	updateRequired = true;
}

void World::sendUpdates() {
	if (!updateRequired) {
		return;
	}

	updateRequired = false;

	sz_t offs = 2;
	u32 tmp;

	u8 buf[1 + 1 + playerUpdates.size() * (sizeof(u32) + sizeof(pinfo_t))
		+ sizeof(u16) + pixelUpdates.size() * sizeof(pixupd_t)
		+ 1 + sizeof(u32) * playersLeft.size()];
	u8 * const upd = &buf[0];

	upd[0] = UPDATE;

	bool pendingUpdates = false;

	tmp = 0;
	for (auto it = playerUpdates.begin();;) {
		if (it == playerUpdates.end()) {
			playerUpdates.clear();
			break;
		}
		if(tmp >= WORLD_MAX_PLAYER_UPDATES){
			playerUpdates.erase(playerUpdates.begin(), it);
			pendingUpdates = true;
			break;
		}
		Player& pl = *it;
		u32 id = pl.getPid();
		i32 x = pl.getX();
		i32 y = pl.getY();
		memcpy((void *)(upd + offs), (void *)&id, sizeof(u32));
		offs += sizeof(u32);
		memcpy((void *)(upd + offs), (void *)&x, sizeof(i32));
		offs += sizeof(i32);
		memcpy((void *)(upd + offs), (void *)&y, sizeof(i32));
		offs += sizeof(i32);
		++it;
		++tmp;
	}
	upd[1] = tmp;
	tmp = pixelUpdates.size();
	tmp = tmp >= WORLD_MAX_PIXEL_UPDATES ? WORLD_MAX_PIXEL_UPDATES : tmp;
	memcpy((void *)(upd + offs), &tmp, sizeof(u16));
	tmp = 0;
	offs += sizeof(u16);
	for(auto& px : pixelUpdates){
		memcpy((void *)(upd + offs), &px, sizeof(pixupd_t));
		offs += sizeof(pixupd_t);
		if(++tmp >= WORLD_MAX_PIXEL_UPDATES){
			break;
		}
	}
	pixelUpdates.clear();

	tmp = playersLeft.size();
	tmp = tmp >= WORLD_MAX_PLAYER_LEFT_UPDATES ? WORLD_MAX_PLAYER_LEFT_UPDATES : tmp;
	memcpy((void *)(upd + offs), &tmp, sizeof(u8));
	tmp = 0;
	offs += sizeof(u8);
	for (auto it = playersLeft.begin();;) {
		if (it == playersLeft.end()) {
			playersLeft.clear();
			break;
		}
		if(tmp >= WORLD_MAX_PLAYER_LEFT_UPDATES){
			playersLeft.erase(playersLeft.begin(), it);
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
	for (Player& pl : players) {
		pl.getClient().getWs()->sendPrepared(prep);
	}
	uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);

	if (pendingUpdates) {
		schedUpdates();
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
			WorldStorage::maybeConvert(x, y);
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

// returns true if this function ended the request before returning
bool World::sendChunk(uWS::HttpResponse * res, i32 x, i32 y) {
	// will load the chunk if unloaded... possible race with nginx if not
	auto chunk = get_chunk(x, y);
	if (chunk == chunks.end()) {
		res->end();
		return true;
	}

	if (!chunk->second.isPngCacheOutdated()) {
		const auto& d = chunk->second.getPngData();
		res->end(reinterpret_cast<const char *>(d.data()), d.size());
		return true;
	}

	u64 k = key(x, y);
	auto search = ongoingChunkRequests.find(k);
	if (search == ongoingChunkRequests.end()) {
		Chunk& c = chunk->second;
		c.preventUnloading(true);

		search = ongoingChunkRequests.emplace(std::piecewise_construct,
			std::forward_as_tuple(k),
			std::forward_as_tuple(std::initializer_list<uWS::HttpResponse *>{res})).first;

		auto end = [this, search, &c] (TaskBuffer& tb) {
			const auto& d = c.getPngData();
			for (uWS::HttpResponse * res : search->second) {
				res->end(reinterpret_cast<const char *>(d.data()), d.size());
			}

			ongoingChunkRequests.erase(search);
			c.preventUnloading(false);
			c.unsetCacheOutdatedFlag();
			tryUnloadWorld();
		};

		tb.queue([&c, end{std::move(end)}] (TaskBuffer& tb) {
			c.updatePngCache();
			tb.runInMainThread(std::move(end));
		});
	} else {
		// add this request to the list, if a png is already being encoded
		search->second.emplace(res);
	}

	return false;
}

void World::cancelChunkRequest(uWS::HttpResponse * res, i32 x, i32 y) {
	auto search = ongoingChunkRequests.find(key(x, y));
	if (search != ongoingChunkRequests.end()) {
		search->second.erase(res);
		// can't erase the map entry, even if the set is empty
		// because when the encoding finishes it will be accessed
	}
}

void World::chat(Player& p, const std::string& s) {
	broadcast(nlohmann::json({
		{ "t", "chat" },
		{ "user", p.getUserInfo() },
		{ "msg", s }
	}).dump());
}

bool World::paint(Player& p, i32 x, i32 y, RGB_u clr) {
	auto chunk = get_chunk(x >> 9, y >> 9);
	if (chunk == chunks.end()) {
		return false;
	}

	if (isActionPaintAllowed(chunk->second, x, y, p) && chunk->second.setPixel(x & 0x1FF, y & 0x1FF, clr)){
		pixelUpdates.push_back({p.getPid(), x, y, clr.r, clr.g, clr.b});
		schedUpdates();
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

	if (players.size() != 0) {
		u8 msg[10] = {CHUNK_PROTECTED};
		memcpy(&msg[1], (char *)&x, 4);
		memcpy(&msg[5], (char *)&y, 4);
		memcpy(&msg[9], (char *)&state, 1);
		uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = uWS::WebSocket<uWS::SERVER>::prepareMessage(
			(char *)&msg[0], sizeof(msg), uWS::BINARY, false);
		for (Player& pl : players) {
			pl.getClient().getWs()->sendPrepared(prep);
		}
		uWS::WebSocket<uWS::SERVER>::finalizeMessage(prep);
	}
}

void World::broadcast(const std::string& msg) const {
	uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = uWS::WebSocket<uWS::SERVER>::prepareMessage(
		(char *)msg.c_str(), msg.size(), uWS::TEXT, false);
	for (Player& pl : players) {
		pl.getClient().getWs()->sendPrepared(prep);
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

sz_t World::getPlayerCount() const {
	return players.size();
}

void World::restrictDrawing(bool s) {
	drawRestricted = s;
}

bool World::isActionPaintAllowed(const Chunk& c, i32 x, i32 y, Player& p) {
	x >>= 4; x &= 0xF; // divide by 16 and mod 16
	y >>= 4; y &= 0xF;
	return c.getProtectionGid(x, y) == 0 /*|| rank >= Client::MODERATOR*/;
}

bool World::tryUnloadAllChunks() {
	for (auto it = chunks.begin(); it != chunks.end();) {
		it = it->second.shouldUnload(true) ? chunks.erase(it) : std::next(it);
	}
	return chunks.size() == 0;
}

void World::tryUnloadWorld() {
	if (!players.size() && tryUnloadAllChunks()) {
		unload();
	}
}
