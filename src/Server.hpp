#pragma once

#include <string>
#include <memory>

#include <ConnectionManager.hpp>
//#include <CommandManager.hpp>
#include <Storage.hpp>
#include <WorldManager.hpp>
#include <PacketReader.hpp>

#include <misc/explints.hpp>
#include <misc/TimedCallbacks.hpp>
#include <misc/AsyncHttp.hpp>
#include <misc/ApiProcessor.hpp>
#include <misc/TaskBuffer.hpp>

#include <uWS.h>

class BansManager;

class Server {
	const i64 startupTime;
	uWS::Hub h;
	// To stop the server from a signal handler, or other thread
	std::unique_ptr<uS::Async, void (*)(uS::Async *)> stopCaller;

	Storage s;
	BansManager& bm;
	TaskBuffer tb;
	TimedCallbacks tc;
	AsyncHttp hcli;
	WorldManager wm;
	PacketReader pr;
	ApiProcessor api;
	//CommandManager cmd;
	ConnectionManager conn;

	u32 saveTimer;

public:
	Server(std::string basePath = ".");

	bool listenAndRun();
	void kickInactivePlayers();
	void stop();

private:
	void registerEndpoints();
	void registerPackets();
	static void doStop(uS::Async *);
	void unsafeStop();
};
