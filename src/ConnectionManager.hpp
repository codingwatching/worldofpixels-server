#pragma once

#include <functional>
#include <string>
#include <map>
#include <forward_list>
#include <list>
#include <typeindex>

#include <explints.hpp>
#include <fwd_uWS.h>
#include <Ip.hpp>

#include <User.hpp>

class ConnectionProcessor;
class IncomingConnection;
class Client;
class AuthManager;

struct ClosedConnection {
	uWS::WebSocket<true> * const ws;
	const Ip ip;
	const bool wasClient;

	ClosedConnection(Client&);
	ClosedConnection(IncomingConnection&);
};

struct ConnectionInfo {
	std::string world;
};

struct IncomingConnection {
	ConnectionInfo ci;
	Session& session;
	uWS::WebSocket<true> * ws;
	std::map<std::string, std::string> args;
	std::forward_list<std::unique_ptr<ConnectionProcessor>>::iterator nextProcessor;
	std::list<IncomingConnection>::iterator it; // own position in list
	Ip ip;
	bool cancelled;
};

class ConnectionManager {
	uWS::Group<true>& defaultGroup;
	AuthManager& am;

	std::forward_list<std::unique_ptr<ConnectionProcessor>> processors;
	std::list<IncomingConnection> pending;
	std::map<std::type_index, std::reference_wrapper<ConnectionProcessor>> processorTypeMap;
	std::function<Client*(IncomingConnection&)> clientTransformer;

public:
	ConnectionManager(uWS::Hub&, AuthManager&, std::string protoName);

	void onSocketChecked(std::function<Client*(IncomingConnection&)>);

	template<typename ProcessorType, typename... Args>
	ProcessorType& addToBeg(Args&&...);

	template<typename ProcessorType>
	ProcessorType& getProcessor();

	void forEachProcessor(std::function<void(ConnectionProcessor&)>);
	void forEachClient(std::function<void(Client&)>);

private:
	void handleIncoming(uWS::WebSocket<true> *, Session&, std::map<std::string, std::string>, uWS::HttpRequest, Ip);
	void handleAsync(IncomingConnection&);
	void handleFail(IncomingConnection&);
	void handleEnd(IncomingConnection&);

	void handleDisconnect(IncomingConnection&, bool);
	void handleDisconnect(Client&);
};

#include "ConnectionManager.tpp"
