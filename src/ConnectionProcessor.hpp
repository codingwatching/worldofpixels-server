#pragma once

#include <functional>

#include <misc/fwd_uWS.h>

#include <nlohmann/json.hpp>

struct IncomingConnection;
struct ClosedConnection;
class Client;

class ConnectionProcessor {
public:
	virtual ~ConnectionProcessor() = default;

	virtual bool isAsync(IncomingConnection&);

	virtual bool preCheck(IncomingConnection&, uWS::HttpRequest&);
	virtual void asyncCheck(IncomingConnection&, std::function<void(bool)> cb);
	virtual bool endCheck(IncomingConnection&);

	virtual void connected(Client&);
	virtual void disconnected(ClosedConnection&);

	virtual nlohmann::json getPublicInfo();
};
