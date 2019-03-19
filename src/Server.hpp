#pragma once

#include <string>
#include <chrono>
#include <memory>

#include <ConnectionManager.hpp>
#include <Storage.hpp>
#include <WorldManager.hpp>
#include <ApiProcessor.hpp>
#include <AuthManager.hpp>

#include <PacketReader.hpp>
#include <explints.hpp>
#include <TimedCallbacks.hpp>
#include <AsyncHttp.hpp>
#include <TaskBuffer.hpp>
#include <AsyncPostgres.hpp>

#include <uWS.h>

class BansManager;
class Client;

class Server {
	const std::chrono::steady_clock::time_point startupTime;
	uWS::Hub h;
	// To stop the server from a signal handler, or other thread
	std::unique_ptr<uS::Async, void (*)(uS::Async *)> stopCaller;

	Storage s;
	BansManager& bm;
	TaskBuffer tb;
	TimedCallbacks tc;
	AuthManager am;
	WorldManager wm;
	ConnectionManager conn;
	ApiProcessor api;
	AsyncHttp hcli;
	AsyncPostgres ap;
	PacketReader<Client> pr;

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
