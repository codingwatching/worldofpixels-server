#pragma once
#include <map>
#include <string>

#include <World.hpp>

#include <misc/explints.hpp>

class TaskBuffer;
class Storage;
class TimedCallbacks;

class WorldManager {
	std::map<std::string, World> worlds;
	TaskBuffer& tb;
	Storage& s;
	u32 tickTimer;

public:
	WorldManager(TaskBuffer&, TimedCallbacks&, Storage&);

	static bool verifyWorldName(const std::string&);

	// should change World& for std::optional<World&> on c++17
	bool isLoaded(const std::string&) const;
	World& getOrLoadWorld(std::string);

	sz_t loadedWorlds() const;
	bool saveAll();

private:
	void tickWorlds();
	void unload(const std::map<std::string, World>::iterator);
};
