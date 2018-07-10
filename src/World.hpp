#pragma once

#include <Database.hpp>
#include <Client.hpp>
#include <Chunk.hpp>
#include <types.hpp>

#include <misc/color.hpp>
#include <misc/explints.hpp>

#include <string>
#include <set>
#include <unordered_map>
#include <vector>

class World {
	bool updateRequired;
	u32 bgclr;
	u32 pids;
	u16 paintrate;
	u8 defaultRank;
	Database db;
	std::string pass;
	std::set<Client *> clients;
	std::unordered_map<u64, Chunk *> chunks;
	std::vector<pixupd_t> pxupdates;
	std::set<Client *> plupdates;
	std::set<u32> plleft;

public:
	const std::string name;

	World(const std::string& path, const std::string& name);
	~World();

	void update_all_clients();

	void setChunkProtection(i32 x, i32 y, bool state);

	void reload();
	std::string getProp(std::string key, std::string defval = "");
	void setProp(std::string key, std::string value);

	u32 get_id();
	void add_cli(Client * const);
	void upd_cli(Client * const);
	void rm_cli(Client * const);
	Client * get_cli(const u32 id) const;
	Client * get_cli(const std::string name) const;

	std::set<Client *> * get_pl();

	void sched_updates();
	void send_updates();

	Chunk * get_chunk(const i32 x, const i32 y, bool create = true);
	void send_chunk(uWS::WebSocket<uWS::SERVER> *, const i32 x, const i32 y, bool compressed = false);
	void del_chunk(const i32 x, const i32 y, const RGB);
	void paste_chunk(const i32 x, const i32 y, char const * const);
	bool put_px(const i32 x, const i32 y, const RGB, u8 placerRank, u32 id);

	void broadcast(const std::string& msg) const;

	void save();

	bool is_empty() const;
	bool mods_enabled();
	bool is_pass(std::string const&) const;
	void set_default_rank(u8);
	u8 get_default_rank();
	u16 get_paintrate();
};