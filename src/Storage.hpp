#pragma once

#include <string>
#include <unordered_map>
#include <set>
#include <map>
#include <unordered_set>
#include <fstream>

#include <misc/explints.hpp>
#include <misc/PropertyReader.hpp>

u64 key(i32, i32);

class WorldStorage : public PropertyReader {
	const std::string& baseDir;
	const std::string worldName;

	WorldStorage();

public:
	std::string getWorldDir();
	std::string getChunkFile(i32 x, i32 y);

	friend Storage;
};

class Storage : public PropertyReader {
	const std::string baseDir;

public:
	Storage(const std::string baseDir);
	~Storage();

	void save();

	std::string getProp(std::string key, std::string defval = "");
	void setProp(std::string key, std::string value);

	WorldStorage getWorldStorage(const std::string world);
};