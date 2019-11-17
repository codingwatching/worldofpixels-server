#include "WorldManager.hpp"

#include <iostream>
#include <utility>
#include <Storage.hpp>
//#include <TaskBuffer.hpp>
#include <TimedCallbacks.hpp>

WorldManager::WorldManager(TaskBuffer& tb, TimedCallbacks& tc, Storage& s)
: tb(tb),
  s(s),
  averageTickInterval(50000),
  lastTickOn(std::chrono::steady_clock::now()) {
	tickTimer = tc.startTimer([this] {
		tickWorlds();
		return true;
	}, 60);

	ageTimer = tc.startTimer([this] {
		unloadOldChunks();
		return true;
	}, 65000);
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

std::string_view WorldManager::getDefaultWorldName() const {
	return s.getDefaultWorldName();
}

bool WorldManager::setDefaultWorldName(std::string str) {
	if (!verifyWorldName(str)) {
		return false;
	}

	s.setDefaultWorldName(std::move(str));
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

void WorldManager::forEach(std::function<void(World&)> f) {
	// this loop will crash if a world is unloaded while iterating
	for (auto& world : worlds) {
		f(world.second);
	}
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

sz_t WorldManager::unloadOldChunks(bool all) {
	sz_t totalUnloaded = 0;
	for (auto& w : worlds) {
		totalUnloaded += w.second.unloadOldChunks(all);
	}

	return totalUnloaded;
}

float WorldManager::getTps() const {
	return (std::chrono::seconds(1) / averageTickInterval);
}

void WorldManager::tickWorlds() {
	auto now(std::chrono::steady_clock::now());

	for (auto& w : worlds) {
		w.second.sendUpdates();
	}

	averageTickInterval = (now - lastTickOn + averageTickInterval) / 2.f;
	lastTickOn = now;
}

void WorldManager::unload(std::map<std::string, World>::iterator it) {
	worlds.erase(it);
}
