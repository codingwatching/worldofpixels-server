#pragma once

#include <uWS.h>

#include <misc/color.hpp>
#include <misc/explints.hpp>
#include <misc/PngImage.hpp>
#include <types.hpp>

class Chunk {
	//const RGB bg;
	const i32 x;
	const i32 y;
	WorldStorage& ws;
	PngImage data;
	u32 protectionData[16 * 16]; // split one chunk to 16x16 protections
	// with specific GIDs
	std::vector<u8> pngCache; // could get big
	bool pngCacheOutdated;
	bool pngFileOutdated;

public:
	Chunk(i32 x, i32 y, RGB bg);
	~Chunk();

	bool setPixel(u16 x, u16 y, RGB);
	const std::vector<u8>& getPngData();
	void save();
};
