#pragma once

#include <array>
#include <vector>
#include <mutex>
#include <shared_mutex>

#include <misc/explints.hpp>
#include <misc/color.hpp>
#include <misc/PngImage.hpp>

class WorldStorage;

class Chunk {
	mutable std::shared_timed_mutex sm;
	i64 lastAction;
	const i32 x;
	const i32 y;
	const WorldStorage& ws;
	PngImage data;
	std::array<u32, 32 * 32> protectionData; // split one chunk to 32x32 protections
	// with specific GIDs, or GGIDs (grouped groups IDs)
	std::vector<u8> pngCache; // could get big
	bool canUnload;
	bool pngCacheOutdated;
	bool pngFileOutdated;
	bool moved;

public:
	Chunk(i32 x, i32 y, const WorldStorage& ws);
	Chunk(Chunk&&);
	~Chunk();

	bool setPixel(u16 x, u16 y, RGB_u);

	void setProtectionGid(u8 x, u8 y, u32 gid);
	u32 getProtectionGid(u8 x, u8 y) const;

	bool isPngCacheOutdated() const;
	void unsetCacheOutdatedFlag();
	void updatePngCache();
	const std::vector<u8>& getPngData() const;

	bool save();

	i64 getLastActionTime() const;
	bool shouldUnload(bool) const;
	void preventUnloading(bool);

private:
	void lockChunk();
	void unlockChunk();
};
