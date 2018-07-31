#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <string>
#include <utility>

#include <misc/explints.hpp>
#include <misc/color.hpp>

class PngImage {
	std::unique_ptr<u8[]> data;
	std::map<std::string, std::function<bool(u8*, sz_t)>> chunkReaders;
	std::map<std::string, std::function<std::pair<std::unique_ptr<u8[]>, sz_t>()>> chunkWriters;
	u32 w;
	u32 h;

public:
	PngImage();
	PngImage(const std::string&);
	PngImage(u8* filebuf, sz_t len);
	PngImage(u32 w, u32 h, RGB = {255, 255, 255});

	void applyTransform(std::function<RGB(u32 x, u32 y)>);
	RGB getPixel(u32 x, u32 y) const;
	void setPixel(u32 x, u32 y, RGB);
	void fill(RGB);

	void setChunkReader(const std::string&, std::function<bool(u8*, sz_t)>);
	void setChunkWriter(const std::string&, std::function<std::pair<std::unique_ptr<u8[]>, sz_t>()>);

	void allocate(u32 w, u32 h, RGB);
	void readFile(const std::string&);
	void readFileOnMem(u8 * filebuf, sz_t len);
	void writeFileOnMem(std::vector<u8>& out);
	void writeFile(const std::string&);
};
