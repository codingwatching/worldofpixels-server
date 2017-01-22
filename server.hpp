#include <uWS.h>
#include <utility>
#include <iostream>
#include <unordered_map>
#include <set>
#include <fstream>
#include <functional>
#include "limiter.hpp"
#include "config.hpp"
#include "misc.hpp"

class Client;

#include "network.hpp"


class Chunk;
class World;
class Server;

inline std::string key(int32_t i, int32_t j) {
	return std::string((char *)&i, sizeof(i)) + std::string((char *)&j, sizeof(j));
};

enum ranks : uint8_t {
	GUEST,
	USER,
	MODERATOR,
	ADMIN
};

struct pinfo_t {
	int32_t x;
	int32_t y;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t tool;
};

struct pixupd_t {
	int32_t x;
	int32_t y;
	uint8_t r;
	uint8_t g;
	uint8_t b;
} __attribute__((packed));

struct chunkpos_t {
	int32_t x;
	int32_t y;
};

struct RGB {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

double ColourDistance(RGB e1, RGB e2);

class Database {
	const std::string dir;
	bool created_dir;
	std::unordered_map<std::string, std::fstream *> handles;
	std::set<std::string> nonexistant;

public:
	Database(const std::string& dir);
	~Database();

	std::fstream * get_handle(const int32_t x, const int32_t y, const bool create);

	bool get_chunk(const int32_t x, const int32_t y, char * const arr);
	void set_chunk(const int32_t x, const int32_t y, const char * const arr);
};

class Chunk {
	Database * const db;
	const int32_t cx;
	const int32_t cy;
	uint8_t data[16 * 16 * 3];
	bool changed;

public:
	Chunk(const int32_t cx, const int32_t cy, Database * const);
	~Chunk();

	bool set_data(const uint8_t x, const uint8_t y, const RGB);
	void send_data(uWS::WebSocket<uWS::SERVER>);
	void save();

	void clear();
};

class Client {
	NetStage * msghandler;
	limiter::Bucket pixupdlimit;
	limiter::Bucket chatlimit;
	uv_timer_t idletimeout_hdl;
	uWS::WebSocket<uWS::SERVER> ws;
	World * wrld;
	uint16_t penalty;
	bool handledelete;
	uint8_t rank;
	uint32_t permissions;
	pinfo_t pos;
	RGB lastclr;
	uint32_t id;
	std::string name;

public:
	const std::string ip;

	Client(NetStage *, uWS::WebSocket<uWS::SERVER>, const std::string& ip);
	~Client();

	void msg(const char *, const size_t);

	bool can_edit();

	void get_chunk(const int32_t x, const int32_t y) const;
	void put_px(const int32_t x, const int32_t y, const RGB);

	void teleport(const int32_t x, const int32_t y);
	void move(const pinfo_t&);
	const pinfo_t * get_pos();

	bool can_chat();
	void chat(const std::string&);
	void tell(const std::string&);

	void updated();
	static void idle_timeout(uv_timer_t * const);

	void safedelete(const bool close);
	
	void send_cli_perms();
	void change_limits(const uint16_t rate, bool chat);
	void promote(const uint8_t rank);

	bool warn();

	bool is_admin() const;
	uWS::WebSocket<uWS::SERVER> get_ws() const;
	std::string get_nick() const;
	World * get_world() const;
	uint32_t get_id() const;

	void set_id(uint32_t);
	void set_world(World * const);
	void set_name(const std::string&);
	void set_msg_handler(NetStage * const);
};

class World {
	uv_timer_t upd_hdl;
	Database db;
	std::set<Client *> clients;
	std::unordered_map<std::string, Chunk *> chunks;
	std::vector<pixupd_t> pxupdates;
	std::set<Client *> plupdates;
	std::set<uint32_t> plleft;

public:
	const std::string name;

	World(const std::string& path, const std::string& name);
	~World();

	uint32_t get_id();
	void add_cli(Client * const);
	void upd_cli(Client * const);
	void rm_cli(Client * const);
	Client * get_cli(const uint32_t id) const;

	void sched_updates();
	static void send_updates(uv_timer_t * const);

	Chunk * get_chunk(const int32_t x, const int32_t y);
	void send_chunk(uWS::WebSocket<uWS::SERVER>, const int32_t x, const int32_t y);
	void del_chunk(const int32_t x, const int32_t y);
	bool put_px(const int32_t x, const int32_t y, const RGB);

	void safedelete();

	void broadcast(const std::string& msg) const;

	void save() const;

	bool is_empty() const;
};

class WorldManager {
	const std::string path;
	std::unordered_map<std::string, World *> worlds;

public:
	WorldManager(const std::string& path);
	~WorldManager();
	bool join_world(Client * const, const std::string&);
	World * get_world(const std::string&);
	void rm_world(World * const);
	void save() const;
};

class Commands {
	std::unordered_map<std::string, std::function<void(Client * const, const std::vector<std::string>&)>> usrcmds;
	std::unordered_map<std::string, std::function<void(Client * const, const std::vector<std::string>&)>> admincmds;

public:
	Commands(Server * const);

	std::string get_cmd_list(const bool admin) const;

	static std::vector<std::string> split_args(const std::string&);

	bool exec(Client * const, const std::string& msg) const;

	static void adminlogin(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);

	static void help(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void teleport(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void kick(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
};

class Server {
	const uint16_t port;
	const std::string adminpw;
	const Commands cmds;
	uv_timer_t save_hdl;
	WorldManager worlds;
	uWS::Hub h;
	LoginManager lm;
	NetStagePlay netplay;
	NetStageLogin netlogin;

public:
	Server(const uint16_t port, const std::string& adminpw, const std::string& path);
	~Server();

	void broadcastmsg(const std::string&);

	void run();
	void quit();

	void save_now() const;
	static void save_chunks(uv_timer_t * const);

	bool is_adminpw(const std::string&);
};
