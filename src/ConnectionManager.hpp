#pragma once

#include <functional>
#include <string>
#include <map>
#include <forward_list>
#include <list>
#include <typeindex>

#include <misc/explints.hpp>
#include <misc/fwd_uWS.h>

#include <Client.hpp>
#include <ConnectionProcessor.hpp>

class ConnectionProcessor;

struct ClosedConnection {
	uWS::WebSocket<true> * const ws;
	const std::string ip;
	const bool wasClient;

	ClosedConnection(Client&);
	ClosedConnection(IncomingConnection&);
};

struct ConnectionInfo {
	UserInfo ui;
	std::string world;
};

struct IncomingConnection {
	ConnectionInfo ci;
	uWS::WebSocket<true> * ws;
	std::map<std::string, std::string> args;
	std::forward_list<std::unique_ptr<ConnectionProcessor>>::iterator nextProcessor;
	std::list<IncomingConnection>::iterator it; // own position in list
	std::string ip;
	bool cancelled;
};

class ConnectionManager {
	uWS::Group<true>& defaultGroup;
	std::forward_list<std::unique_ptr<ConnectionProcessor>> processors;
	std::list<IncomingConnection> pending;
	std::map<std::type_index, std::reference_wrapper<ConnectionProcessor>> processorTypeMap;
	std::function<Client*(IncomingConnection&)> clientTransformer;

public:
	ConnectionManager(uWS::Hub&, std::string protoName);

	void onSocketChecked(std::function<Client*(IncomingConnection&)>);

	template<typename ProcessorType, typename... Args>
	ProcessorType& addToBeg(Args&&...);

	template<typename ProcessorType>
	ProcessorType& getProcessor();

	void forEachProcessor(std::function<void(ConnectionProcessor&)>);
	void forEachClient(std::function<void(Client&)>);

private:
	void handleIncoming(uWS::WebSocket<true> *, std::map<std::string, std::string>, uWS::HttpRequest, std::string ip);
	void handleAsync(IncomingConnection&);
	void handleFail(IncomingConnection&);
	void handleEnd(IncomingConnection&);

	void handleDisconnect(IncomingConnection&, bool);
	void handleDisconnect(Client&);
};

#include "ConnectionManager.tpp"
