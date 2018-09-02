#pragma once

#include <Storage.hpp>
#include <Chunk.hpp>
#include <Player.hpp>
#include <types.hpp>

#include <misc/color.hpp>
#include <misc/explints.hpp>
#include <misc/IdSys.hpp>
#include <misc/fwd_uWS.h>

#include <string>
#include <set>
#include <unordered_map>
#include <map>
#include <vector>
#include <tuple>

class TaskBuffer;
class Client;
class Player;

class World : public WorldStorage {
	IdSys ids;
	TaskBuffer& tb; // for http chunk requests
	bool updateRequired;
	bool drawRestricted;

	std::function<void()> unload;

	std::set<std::reference_wrapper<Player>> players;
	std::unordered_map<u64, Chunk> chunks;
	std::map<u64, std::set<uWS::HttpResponse *>> ongoingChunkRequests;

	std::vector<pixupd_t> pixelUpdates;
	std::set<std::reference_wrapper<Player>> playerUpdates;
	std::set<u32> playersLeft; // this could be a vector

public:
	World(std::tuple<std::string, std::string>, TaskBuffer&);
	~World();

	World(const World&) = delete;

	void setUnloadFunc(std::function<void()>);

	void configurePlayerBuilder(Player::Builder&);
	void playerJoined(Player&);
	void playerUpdated(Player&);
	void playerLeft(Player&);

	void schedUpdates();
	void sendUpdates();

	sz_t unloadOldChunks(bool force = false);

	const std::unordered_map<u64, Chunk>::iterator get_chunk(i32 x, i32 y, bool create = true);
	void setChunkProtection(i32 x, i32 y, bool state);
	bool sendChunk(uWS::HttpResponse *, i32 x, i32 y);
	void cancelChunkRequest(uWS::HttpResponse *, i32 x, i32 y);

	bool paint(Player&, i32 x, i32 y, RGB_u);

	void chat(Player&, const std::string&);
	void broadcast(const std::string& msg) const;

	bool save();

	sz_t getPlayerCount() const;
	void restrictDrawing(bool);

private:
	bool isActionPaintAllowed(const Chunk&, i32 x, i32 y, u8 rank); // to be changed
	bool tryUnloadAllChunks();
	void tryUnloadWorld();
};
