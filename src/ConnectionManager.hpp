#pragma once

#include <functional>
#include <string>
#include <map>
#include <forward_list>
#include <list>

#include <misc/explints.hpp>
#include <misc/fwd_uWS.h>

#include <ConnectionProcessor.hpp>

class ConnectionProcessor;
class Client;

struct ClosedConnection {
	uWS::WebSocket<true> * const ws;
	const std::string ip;

	ClosedConnection(Client&);
	ClosedConnection(IncomingConnection&);
};

struct ConnectionInfo {
	std::string world;
};

struct IncomingConnection {
	uWS::WebSocket<true> * ws;
	std::map<std::string, std::string> args;
	ProcessorList::iterator nextProcessor;
	PendingList::iterator it; // own position in list
	std::string ip;
	bool cancelled;
};

class ConnectionManager {
public:
	using ProcessorList = std::forward_list<std::unique_ptr<ConnectionProcessor>>;
	using PendingList = std::list<IncomingConnection>;
	using ArgList = std::map<std::string, std::string>;

private:
	ProcessorList processors;
	PendingList pending;
	std::function<Client*(IncomingConnection&)> clientTransformer;

public:
	ConnectionManager(uWS::Hub&, std::string protoName);

	void addToBeg(std::unique_ptr<ConnectionProcessor>);
	void onSocketChecked(std::function<Client*(IncomingConnection&)>);

private:
	void handleIncoming(uWS::WebSocket<true> *, ArgList, uWS::HttpRequest, std::string ip);
	void handleAsync(IncomingConnection&);
	void handleFail(IncomingConnection&);
	void handleEnd(IncomingConnection&);

	void handleDisconnect(IncomingConnection&);
	void handleDisconnect(Client&);
};
