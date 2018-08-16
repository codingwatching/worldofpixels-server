#pragma once

#include <Storage.hpp>
#include <Client.hpp>
#include <Chunk.hpp>
#include <types.hpp>

#include <misc/color.hpp>
#include <misc/explints.hpp>
#include <misc/IdSys.hpp>
#include <misc/TaskBuffer.hpp>

#include <string>
#include <set>
#include <unordered_map>
#include <map>
#include <vector>
#include <mutex>

namespace uWS {
	struct HttpResponse;
}

class World : public WorldStorage {
	IdSys ids;
	TaskBuffer& tb; // for http chunk requests
	bool updateRequired;
	bool drawRestricted;

	std::function<void()> unload;

	std::set<Client *> clients;
	std::unordered_map<u64, Chunk> chunks;
	std::map<u64, std::vector<uWS::HttpResponse *>> ongoingChunkRequests;

	std::vector<pixupd_t> pxupdates;
	std::set<Client *> plupdates;
	std::set<u32> plleft;

public:
	World(WorldStorage, TaskBuffer&);
	~World();

	World(World&&) = default;
	World& operator=(World&&) = default;

	void setUnloadFunc(std::function<void()>);

	sz_t unloadOldChunks(bool force = false);
	void update_all_clients();

	void setChunkProtection(i32 x, i32 y, bool state);

	void add_cli(Client * const);
	void upd_cli(Client * const);
	void rm_cli(Client * const);
	Client * get_cli(const u32 id) const;
	Client * get_cli(const std::string name) const;

	std::set<Client *> * get_pl();

	void sched_updates();
	void send_updates();

	const std::unordered_map<u64, Chunk>::iterator get_chunk(i32 x, i32 y, bool create = true);
	void send_chunk(uWS::HttpResponse *, i32 x, i32 y);

	void del_chunk(i32 x, i32 y, const RGB);
	void paste_chunk(const i32 x, const i32 y, char const * const);
	bool put_px(i32 x, i32 y, const RGB, u8 placerRank, u32 id);

	void broadcast(const std::string& msg) const;

	void save();

	bool is_empty() const;
	bool mods_enabled();
	bool is_pass(std::string const&);
	u8 get_default_rank();
	void restrictDrawing(bool);
	u16 get_paintrate();

private:
	bool isActionPaintAllowed(const Chunk&, i32 x, i32 y, u8 rank); // to be changed
	bool tryUnloadAllChunks();
	void tryUnload();
};
