#pragma once

#include <map>
#include <string>
#include <functional>
#include <chrono>

#include <World.hpp>

#include <misc/explints.hpp>

class TaskBuffer;
class Storage;
class TimedCallbacks;

class WorldManager {
	using FloatMicros = std::chrono::duration<float, std::chrono::microseconds::period>;

	std::map<std::string, World> worlds;
	TaskBuffer& tb;
	Storage& s;

	FloatMicros averageTickInterval;
	//std::chrono::microseconds averageTickCost;
	std::chrono::steady_clock::time_point lastTickOn;

	u32 tickTimer;
	u32 ageTimer;

public:
	WorldManager(TaskBuffer&, TimedCallbacks&, Storage&);

	static bool verifyWorldName(const std::string&);

	// should change World& for std::optional<World&> on c++17
	bool isLoaded(const std::string&) const;
	World& getOrLoadWorld(std::string);
	void forEach(std::function<void(World&)>);

	sz_t loadedWorlds() const;
	bool saveAll();

	sz_t unloadOldChunks(bool all = false);

	float getTps() const;

private:
	void tickWorlds();
	void unload(const std::map<std::string, World>::iterator);
};
