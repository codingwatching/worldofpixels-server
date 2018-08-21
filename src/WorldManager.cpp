#include "WorldManager.hpp"

#include <utility>
#include <Storage.hpp>
//#include <misc/TaskBuffer.hpp>
#include <misc/TimedCallbacks.hpp>

WorldManager::WorldManager(TaskBuffer& tb, TimedCallbacks& tc, Storage& s)
: tb(tb),
  s(s) {
	tickTimer = tc.startTimer([this] {
		tickWorlds();
		return true;
	}, 60);
}

bool WorldManager::verifyWorldName(const std::string& name) {
	/* Validate world name, allowed chars are a..z, 0..9, '_' and '.' */
	if (name.size() > 24 || name.size() < 1) {
		return false;
	}

	for (char c : name) {
		if (!((c >  96 && c < 123) ||
			  (c >  47 && c <  58) ||
			   c == 95 || c == 46)) {
			return false;
		}
	}
	return true;
}

bool WorldManager::isLoaded(const std::string& name) const {
	return worlds.find(name) != worlds.end();
}

World& WorldManager::getOrLoadWorld(std::string name) {
	auto sr = worlds.find(name);
	if (sr == worlds.end()) {
		sr = worlds.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(name),
			std::forward_as_tuple(s.getWorldStorageArgsFor(name), tb)
		).first;

		sr->second.setUnloadFunc([this, sr] {
			unload(sr);
		});
	}

	return sr->second;
}

sz_t WorldManager::loadedWorlds() const {
	return worlds.size();
}

bool WorldManager::saveAll() {
	bool didStuff = false;
	for (auto& w : worlds) {
		didStuff |= w.second.save();
	}

	return didStuff;
}

void WorldManager::tickWorlds() {
	for (auto& w : worlds) {
		w.second.send_updates();
	}
}

void WorldManager::unload(std::map<std::string, World>::iterator it) {
	worlds.erase(it);
}
