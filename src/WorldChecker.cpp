#include "WorldChecker.hpp"

#include <ConnectionManager.hpp>
#include <WorldManager.hpp>

WorldChecker::WorldChecker(WorldManager& wm)
: wm(wm) { }

bool WorldChecker::preCheck(IncomingConnection& ic, uWS::HttpRequest&) {
	auto search = ic.args.find("world");

	// I know it's a static method, but the reference might be needed in the future
	if (search == ic.args.end() || !wm.verifyWorldName(search->second)) {
		return false;
	}

	ic.ci.world = std::move(search->second);
	ic.args.erase(search); // std::move might have emptied the string
	// so just erase it from args
	return true;
}
