#pragma once

#include <uWS.h>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <CommandManager.hpp>
#include <Storage.hpp>
#include <WorldManager.hpp>

#include <misc/explints.hpp>
#include <misc/TimedCallbacks.hpp>
#include <misc/AsyncHttp.hpp>
#include <misc/ApiProcessor.hpp>
#include <misc/TaskBuffer.hpp>

class BansManager;

class Server {
	const i64 startupTime;
	uWS::Hub h;
	// To stop the server from a signal handler, or other thread
	std::unique_ptr<uS::Async, void (*)(uS::Async *)> stopCaller;

public:
	Storage s;
	BansManager& bm;
	TaskBuffer tb;
	TimedCallbacks tc;
	AsyncHttp hcli;
	WorldManager wm;
	ApiProcessor api;
	CommandManager cmd;
	ConnectionManager conn;

	//std::unordered_set<uWS::WebSocket<uWS::SERVER> *> connsws;

	//std::unordered_map<std::string, u8> conns;

	//u32 totalConnections;

	u32 saveTimer;

	u8 maxconns;
	bool captcha_required;
	bool lockdown;
	bool proxy_lock;
	bool instaban;
	bool trusting_captcha;
	u8 fastconnectaction;

	Server(std::string basePath = ".");

	void broadcastmsg(const std::string&);

	bool listenAndRun();

	void join_world(uWS::WebSocket<uWS::SERVER> *, const std::string&);

	bool is_adminpw(const std::string&);
	bool is_modpw(const std::string&);
	u32 get_conns(const std::string&);

	void admintell(const std::string&, bool modsToo = false);

	void kickInactivePlayers();
	void kickall(World&);
	void kickall();
	bool kickip(const std::string&);

//	void clearexpbans();
	void banip(const std::string&, i64);
//	std::unordered_map<std::string, i64> * getbans();
//	std::unordered_set<std::string> * getwhitelist();
//	std::unordered_set<std::string> * getblacklist();
//	void whitelistip(const std::string&);

	void set_max_ip_conns(u8);
	void set_captcha_protection(bool);
	void lockdown_check();
	void set_lockdown(bool);
	void set_instaban(bool);
	void set_proxycheck(bool);

	bool is_connected(uWS::WebSocket<uWS::SERVER> *);

//	void writefiles();
//	void readfiles();

	void stop();
	static void doStop(uS::Async *);

private:
	void unsafeStop();
};
