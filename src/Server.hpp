#pragma once

#include <string>
#include <chrono>
#include <memory>

#include <ConnectionManager.hpp>
#include <Storage.hpp>
#include <WorldManager.hpp>
#include <ApiProcessor.hpp>
#include <AuthManager.hpp>

#include <misc/PacketReader.hpp>
#include <misc/explints.hpp>
#include <misc/TimedCallbacks.hpp>
#include <misc/AsyncHttp.hpp>
#include <misc/TaskBuffer.hpp>
#include <misc/AsyncPostgres.hpp>

#include <uWS.h>

class BansManager;
class Client;

class Server {
	const std::chrono::steady_clock::time_point startupTime;
	uWS::Hub h;
	// To stop the server from a signal handler, or other thread
	std::unique_ptr<uS::Async, void (*)(uS::Async *)> stopCaller;

	AsyncPostgres ap;
	Storage s;
	BansManager& bm;
	TaskBuffer tb;
	TimedCallbacks tc;
	AuthManager am;
	ApiProcessor api;
	AsyncHttp hcli;
	WorldManager wm;
	PacketReader<Client> pr;
	ConnectionManager conn;

	u32 saveTimer;
	u32 statsTimer;

public:
	Server(std::string basePath = ".");

	bool listenAndRun();
	bool freeMemory();
	void kickInactivePlayers();
	void stop();

private:
	void registerEndpoints();
	void registerPackets();
	static void doStop(uS::Async *);
	void unsafeStop();
};
