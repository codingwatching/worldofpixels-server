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

class Database {
	const std::string dir;
	bool created_dir;
	std::unordered_map<u64, std::fstream *> handles;
	std::set<u64> nonexistant;
	PropertyReader pr;
	std::unordered_set<u64> rankedChunks;
	bool changedProtects;

public:
	Database(const std::string& dir);
	~Database();

	void save();

	void setChunkProtection(i32 x, i32 y, bool state);
	bool getChunkProtection(i32 x, i32 y);

	std::string getProp(std::string key, std::string defval = "");
	void setProp(std::string key, std::string value);

	std::fstream * get_handle(const i32 x, const i32 y, const bool create);

	bool get_chunk(const i32 x, const i32 y, char * const arr);
	void set_chunk(const i32 x, const i32 y, const char * const arr);
};