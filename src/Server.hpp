#pragma once

#include <uWS.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <Commands.hpp>

#include <misc/explints.hpp>
#include <misc/Bucket.hpp>
#include <misc/TimedCallbacks.hpp>
#include <misc/AsyncHttp.hpp>

class World;

class Server {
public:
	const u16 port;
	const std::string modpw;
	const std::string adminpw;
	const std::string path;
	const i64 startupTime;
	u32 totalConnections;
	uWS::Hub h;
	// To stop the server from a signal handler, or other thread
	std::unique_ptr<uS::Async, void (*)(uS::Async *)> stopCaller;
	Commands cmds;
	std::unordered_map<std::string, std::unique_ptr<World>> worlds;
	std::unordered_set<uWS::WebSocket<uWS::SERVER> *> connsws;
	std::unordered_set<std::string> ipwhitelist;
	std::unordered_set<std::string> ipblacklist;
	std::unordered_map<std::string, i64> ipban;
	std::unordered_map<std::string, u8> conns;
	Bucket connlimiter;
	TimedCallbacks tc;
	AsyncHttp hcli;

	u32 worldTickTimer;
	u32 saveTimer;

	u8 maxconns;
	bool captcha_required;
	bool lockdown;
	bool proxy_lock;
	bool instaban;
	bool trusting_captcha;
	u8 fastconnectaction;

	std::unordered_set<std::string> proxyquery_checking;

	Server(const u16 port, const std::string& modpw,
		const std::string& adminpw, const std::string& path);
	~Server();

	void broadcastmsg(const std::string&);

	bool listenAndRun(const std::string listenOn = "");

	void save_now();
	void tickWorlds();

	void join_world(uWS::WebSocket<uWS::SERVER> *, const std::string&);

	bool is_adminpw(const std::string&);
	bool is_modpw(const std::string&);
	u32 get_conns(const std::string&);

	void admintell(const std::string&, bool modsToo = false);

	void kickInactivePlayers();
	void kickall(World * const);
	void kickall();
	bool kickip(const std::string&);

	void clearexpbans();
	void banip(const std::string&, i64);
	std::unordered_map<std::string, i64> * getbans();
	std::unordered_set<std::string> * getwhitelist();
	std::unordered_set<std::string> * getblacklist();
	void whitelistip(const std::string&);

	void set_max_ip_conns(u8);
	void set_captcha_protection(bool);
	void lockdown_check();
	void set_lockdown(bool);
	void set_instaban(bool);
	void set_proxycheck(bool);

	bool is_connected(uWS::WebSocket<uWS::SERVER> *);

	void writefiles();
	void readfiles();

	void stop();
	static void doStop(uS::Async *);

private:
	void unsafeStop();
};
