#pragma once

#include <array>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <bitset>

#include <misc/explints.hpp>
#include <misc/color.hpp>
#include <misc/PngImage.hpp>
#include <misc/utils.hpp>

class WorldStorage;

class Chunk {
public:
	using Pos = i32;
	using ProtPos = i32;

	static constexpr sz_t size = 256;
	static constexpr sz_t protectionAreaSize = 16;

	// pc**2 = dimensions of the protection array for chunks
	static constexpr sz_t pc = size / protectionAreaSize;

	static constexpr u32 pcShift  = popc(pc - 1);
	static constexpr u32 pSizeShift  = popc(protectionAreaSize - 1);
	static constexpr u32 posShift = popc(size - 1);

private:
	mutable std::shared_timed_mutex sm;
	i64 lastAction;
	const Pos x;
	const Pos y;
	const WorldStorage& ws;
	PngImage data;
	std::array<u32, pc * pc> protectionData; // split one chunk to protection cells
	// with specific GIDs, or GGIDs (grouped groups IDs)
	std::vector<u8> pngCache; // could get big
	bool canUnload;
	bool pngCacheOutdated;
	bool pngFileOutdated;
	bool moved;

public:
	Chunk(Pos x, Pos y, const WorldStorage& ws);
	Chunk(Chunk&&);
	~Chunk();

	bool setPixel(u16 x, u16 y, RGB_u);

	void setProtectionGid(ProtPos x, ProtPos y, u32 gid);
	u32 getProtectionGid(ProtPos x, ProtPos y) const;

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
