#include "World.hpp"

#include <Client.hpp>
#include <User.hpp>
#pragma message("remove config.hpp")
#include <config.hpp>
#include <PacketDefinitions.hpp>
#include <ApiProcessor.hpp>

#include <misc/TaskBuffer.hpp>

#include <iostream>
#include <utility>
#include <algorithm>

#include <uWS.h>
#include <nlohmann/json.hpp>

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
	// solution: move player lefts at the beginning of the network update packet
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

bool World::verifyChunkPos(Chunk::Pos x, Chunk::Pos y) {
	return x <= border && y <= border
		&& x >= ~border && y >= ~border;
}

Chunk& World::getChunk(Chunk::Pos x, Chunk::Pos y) {
	auto search = chunks.find(key(x, y));
	if (search == chunks.end()) {
		WorldStorage::maybeConvertChunk(x, y);

		search = chunks.emplace(std::piecewise_construct,
			std::forward_as_tuple(key(x, y)),
			std::forward_as_tuple(x, y, *this)).first;

		if (chunks.size() > 64) {
			search->second.preventUnloading(true);
			unloadOldChunks(true);
			search->second.preventUnloading(false);
		}
	}

	return search->second;
}

// returns true if this function ended the request before returning
bool World::sendChunk(Chunk::Pos x, Chunk::Pos y, ll::shared_ptr<Request> req) {
	if (!verifyChunkPos(x, y)) {
		req->writeStatus("400 Bad Request");
		req->end();
		return true;
	}

	// will load the chunk if unloaded... possible race with nginx if not
	// TODO: Stream file from the server, instead of relying on nginx
	Chunk& chunk = getChunk(x, y);

	if (!chunk.isPngCacheOutdated()) {
		const auto& d = chunk.getPngData();
		req->end(reinterpret_cast<const char *>(d.data()), d.size());
		return true;
	}

	u64 k = key(x, y);
	auto search = ongoingChunkRequests.find(k);
	if (search == ongoingChunkRequests.end()) {
		chunk.preventUnloading(true);

		search = ongoingChunkRequests.emplace(std::piecewise_construct,
			std::forward_as_tuple(k),
			std::forward_as_tuple(std::initializer_list<ll::shared_ptr<Request>>{std::move(req)})).first;

		auto end = [this, search, &chunk] (TaskBuffer& tb) {
			const auto& d = chunk.getPngData();
			for (auto& req : search->second) {
				if (!req->isCancelled()) { // TODO: Prepared HTTP response?
					//req->writeHeader("Content-Type", "image/png");
					req->end(reinterpret_cast<const char *>(d.data()), d.size());
				} else {
					std::cout << "Didn't send, request was cancelled" << std::endl;
				}
			}

			ongoingChunkRequests.erase(search);
			chunk.preventUnloading(false);
			chunk.unsetCacheOutdatedFlag();
			tryUnloadWorld();
		};

		tb.queue([&chunk, end{std::move(end)}] (TaskBuffer& tb) {
			chunk.updatePngCache();
			tb.runInMainThread(std::move(end));
		});
	} else {
		// add this request to the list, if a png is already being encoded
		search->second.emplace_back(std::move(req));
	}

	return false;
}

/*void World::cancelChunkRequest(Chunk::Pos x, Chunk::Pos y, uWS::HttpResponse * res) {
	auto search = ongoingChunkRequests.find(key(x, y));
	if (search != ongoingChunkRequests.end()) {
		search->second.erase(res);
		// can't erase the map entry, even if the set is empty
		// because when the encoding finishes it will be accessed
	}
}*/

void World::chat(Player& p, const std::string& s) {
	broadcast(ChatMessage(p.getUser().getId(), s));
}

// returns false when you were not allowed to paint, or position is out of range
bool World::paint(Player& p, World::Pos x, World::Pos y, RGB_u clr) {
	Chunk::Pos cx = x >> Chunk::posShift;
	Chunk::Pos cy = y >> Chunk::posShift;

	if (!verifyChunkPos(cx, cy)) {
		return false;
	}

	Chunk& chunk = getChunk(cx, cy);

	if (isActionPaintAllowed(chunk, x, y, p)) {
		if (chunk.setPixel(x, y, clr)) {
			pixelUpdates.push_back({p.getPid(), x, y, clr.r, clr.g, clr.b});
			schedUpdates();
		}

		return true;
	}

	return false;
}

void World::setAreaProtection(Chunk::ProtPos x, Chunk::ProtPos y, bool state) {
	Chunk& chunk = getChunk(x >> Chunk::pcShift, y >> Chunk::pcShift);
	u32 newState = state ? 1 : 0; // these numbers should have a special meaning

	// x and y are 16x16 aligned
	chunk.setProtectionGid(x, y, newState);

	if (players.size() != 0) {
		broadcast(ProtectionUpdate(x, y, newState));
	}
}

void World::broadcast(const PrepMsg& prep) {
	for (Player& pl : players) {
		pl.send(prep);
	}
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

bool World::isActionPaintAllowed(const Chunk& c, World::Pos x, World::Pos y, Player& p) {
	x >>= Chunk::pSizeShift;
	y >>= Chunk::pSizeShift;

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
