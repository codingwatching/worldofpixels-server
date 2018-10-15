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
#include <limits>

class TaskBuffer;
class Client;

class World : public WorldStorage {
public:
	using Pos = i32;

	static constexpr Chunk::Pos border = std::numeric_limits<Pos>::max() / Chunk::size;

private:
	IdSys<Player::Id> ids;
	TaskBuffer& tb; // for http chunk requests
	bool updateRequired;
	bool drawRestricted;

	std::function<void()> unload;

	std::set<std::reference_wrapper<Player>> players;
	std::unordered_map<u64, Chunk> chunks;
	std::map<u64, std::set<uWS::HttpResponse *>> ongoingChunkRequests;

	std::vector<pixupd_t> pixelUpdates;
	std::set<std::reference_wrapper<Player>> playerUpdates;
	std::set<Player::Id> playersLeft; // this might be removed

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

	static bool verifyChunkPos(Chunk::Pos x, Chunk::Pos y);
	Chunk& getChunk(Chunk::Pos x, Chunk::Pos y);

	bool sendChunk(Chunk::Pos x, Chunk::Pos y, uWS::HttpResponse *);
	void cancelChunkRequest(Chunk::Pos x, Chunk::Pos y, uWS::HttpResponse *);

	void setChunkProtection(Chunk::Pos x, Chunk::Pos y, bool state);

	bool paint(Player&, i32 x, i32 y, RGB_u);

	void chat(Player&, const std::string&);
	void broadcast(const std::string& msg) const;

	bool save();

	sz_t getPlayerCount() const;

	void restrictDrawing(bool);

private:
	bool isActionPaintAllowed(const Chunk&, i32 x, i32 y, Player&);
	bool tryUnloadAllChunks();
	void tryUnloadWorld();
};
