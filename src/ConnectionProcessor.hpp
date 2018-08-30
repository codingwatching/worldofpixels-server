#pragma once

#include <functional>

struct IncomingConnection;
struct ClosedConnection;

class ConnectionProcessor {
public:
	virtual ~ConnectionProcessor() = default;

	virtual bool preCheck(IncomingConnection&) = 0;
	virtual bool asyncCheck(IncomingConnection&, std::function<void(bool)> cb) = 0;
	virtual bool endCheck(IncomingConnection&) = 0;

	virtual void disconnected(ClosedConnection&) = 0;
};
