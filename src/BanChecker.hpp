#pragma once

#include "ConnectionProcessor.hpp"

class BansManager;

class BanChecker : public ConnectionProcessor {
	BansManager& bm;

public:
	BanChecker(BansManager&);

	bool preCheck(IncomingConnection&, uWS::HttpRequest&);
};
