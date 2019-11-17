#include "WorldChecker.hpp"

#include <ConnectionManager.hpp>
#include <WorldManager.hpp>

#include <HttpData.hpp>

#include <utils.hpp>

#include <iostream>

WorldChecker::WorldChecker(WorldManager& wm)
: wm(wm) { }

bool WorldChecker::preCheck(IncomingConnection& ic, HttpData hd) {
	std::string_view url(hd.getUrl());
	if (!url.size() || url[0] != '/') {
		return false;
	}

	url.remove_prefix(1);
	auto pos = url.find_first_of("/?");
	if (pos != url.npos) {
		url.remove_suffix(url.size() - pos);
	}

	std::string world(mkurldecoded_v(url));
	if (!world.size()) {
		world = wm.getDefaultWorldName();
	}

	if (!wm.verifyWorldName(world)) {
		return false;
	}

	ic.ci.world = std::move(world);
	return true;
}
