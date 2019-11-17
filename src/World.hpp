#pragma once

#include <Storage.hpp>
#include <Chunk.hpp>
#include <Player.hpp>
#include <User.hpp>
#include <types.hpp>

#include <color.hpp>
#include <explints.hpp>
#include <IdSys.hpp>
#include <shared_ptr_ll.hpp>

#include <string>
#include <set>
#include <unordered_map>
#include <map>
#include <optional>
#include <vector>
#include <tuple>
#include <memory>
#include <limits>

class TaskBuffer;
class Client;
class Request;

class World : public WorldStorage {
public:
	using Pos = i32;

	static constexpr Chunk::Pos border = std::numeric_limits<Pos>::max() / Chunk::size;

private:
	IdSys<Player::Id> ids;
	TaskBuffer& tb; // for http chunk requests
	bool updateRequired;
	bool drawRestricted; // TODO: use to restrict drawing to owner only

	std::function<void()> unload;

	std::set<std::reference_wrapper<Player>> players;
	std::unordered_map<u64, Chunk> chunks;
	std::map<u64, std::vector<ll::shared_ptr<Request>>> ongoingChunkRequests;

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

	void sendUserUpdate(User&);
	void sendPlayerCountStats(u32 globalPlayerCount);
	bool sendChunk(Chunk::Pos x, Chunk::Pos y, ll::shared_ptr<Request>);
	//void cancelChunkRequest(Chunk::Pos x, Chunk::Pos y, ll::shared_ptr<Request>);

	void setAreaProtection(Chunk::ProtPos x, Chunk::ProtPos y, bool state);

	bool paint(Player&, World::Pos x, World::Pos y, RGB_u);

	void chat(Player&, const std::string&);
	void broadcast(const PrepMsg&);

	bool save();

	sz_t getPlayerCount() const;
	std::string_view getMotd() const;
	std::optional<User::Id> getOwner() const;

	void restrictDrawing(bool);

private:
	bool isActionPaintAllowed(const Chunk&,  World::Pos x,  World::Pos y, Player&);
	bool tryUnloadAllChunks();
	void tryUnloadWorld();
};

void to_json(nlohmann::json&, const World&);
