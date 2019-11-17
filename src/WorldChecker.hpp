#pragma once

#include "ConnectionProcessor.hpp"

class WorldManager;

class WorldChecker : public ConnectionProcessor {
	WorldManager& wm;

public:
	WorldChecker(WorldManager&);

	bool preCheck(IncomingConnection&, HttpData);
};
