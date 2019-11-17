#pragma once

#include <functional>
#include <string>
#include <map>
#include <forward_list>
#include <list>
#include <typeindex>
#include <typeinfo>

#include <explints.hpp>
#include <shared_ptr_ll.hpp>
#include <fwd_uWS.h>
#include <Ip.hpp>

class ConnectionProcessor;
class IncomingConnection;
class Client;
class Session;
class HttpData;

struct ClosedConnection {
	uWS::WebSocket<true> * const ws;
	const Ip ip;
	const bool wasClient;

	ClosedConnection(Client&);
	ClosedConnection(IncomingConnection&);
};

struct ConnectionInfo {
	std::string world;
	ll::shared_ptr<Session> session;
};

struct IncomingConnection {
	ConnectionInfo ci;
	uWS::WebSocket<true> * ws;
	std::map<std::string, std::string> args;
	std::forward_list<std::unique_ptr<ConnectionProcessor>>::iterator nextProcessor;
	std::list<IncomingConnection>::iterator it; // own position in list
	Ip ip;
	// to be used ONLY on asyncChecks, cleared after using callback
	std::function<void()> onDisconnect;
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
	void handleIncoming(uWS::WebSocket<true> *, std::map<std::string, std::string>, HttpData, Ip);
	void handleAsync(IncomingConnection&);
	void handleFail(IncomingConnection&, const std::type_info&);
	void handleEnd(IncomingConnection&);

	void handleDisconnect(IncomingConnection&, bool);
	void handleDisconnect(Client&);
};

#include "ConnectionManager.tpp"
