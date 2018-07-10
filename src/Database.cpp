#include "Database.hpp"

#include <iostream>
#include <algorithm>

#include <unistd.h>
#include <cstring>

#include <misc/utils.hpp>
#include <config.hpp>

union twoi32tou64_u {
		struct {
			i32 x;
			i32 y;
		} p;
		u64 pos;
};

u64 key(i32 x, i32 y) {
	twoi32tou64_u s;
	s.p.x = x;
	s.p.y = y;
	return s.pos;
};

Database::Database(const std::string& dir)
: dir(dir),
  created_dir(fileExists(dir)),
  pr(dir + "props.txt"),
  changedProtects(false) {
	if (created_dir) {
		std::ifstream file(dir + "pchunks.bin", std::ios::binary);
		if(file.good()){
			file.seekg(0, std::fstream::end);
			const sz_t size = file.tellg();
			if (size % 8) {
				std::cerr << "Protection file corrupted, at: " << dir << ", ignoring." << std::endl;
			} else {
				const sz_t itemsonfile = size / 8;
				u64 * rankedarr = new u64[itemsonfile];
				file.seekg(0);
				file.read((char*)rankedarr, size);
				for (sz_t i = 0; i < itemsonfile; i++) {
					rankedChunks.emplace(rankedarr[i]);
				}
				delete[] rankedarr;
			}
		}
		file.close();
	}
}

Database::~Database() {
	for(const auto& hdl : handles){
		delete hdl.second;
	}
	handles.clear();
	save();
}

void Database::save() {
    pr.writeToDisk();
	if (!changedProtects) return;
	changedProtects = false;

	if (rankedChunks.size() > 0) {
		std::ofstream file(dir + "pchunks.bin", std::ios_base::binary | std::ios_base::trunc);

		if (file.good()) {
			u64 * rankarr = new u64[rankedChunks.size()];
			sz_t filesize = 0;
			for (u64 pos : rankedChunks) {
				rankarr[filesize++] = pos;
			}
			file.write((char*)rankarr, filesize * sizeof(u64));
			delete[] rankarr;
		}
		file.close();
	} else {
		std::remove((dir + "pchunks.bin").c_str());
	}
}

void Database::setChunkProtection(i32 x, i32 y, bool state) {
	union {
		struct {
			i32 x;
			i32 y;
		} p;
		u64 pos;
	} s;
	s.p.x = x;
	s.p.y = y;
	//u64 p = (*(u64 *)&x) << 32 | (*(u64 *)&y);
	if (state) {
		rankedChunks.emplace(s.pos);
	} else {
		rankedChunks.erase(s.pos);
	}
	changedProtects = true;
}

bool Database::getChunkProtection(i32 x, i32 y) {
	//u64 p = (*(u64 *)&x) << 32 | (*(u64 *)&y);
	union {
		struct {
			i32 x;
			i32 y;
		} p;
		u64 pos;
	} s;
	s.p.x = x;
	s.p.y = y;
	return rankedChunks.find(s.pos) != rankedChunks.end();
}

std::string Database::getProp(std::string key, std::string defval) {
	return pr.getProp(key, defval);
}

void Database::setProp(std::string key, std::string value) {
	pr.setProp(key, value);
}

std::fstream * Database::get_handle(const i32 x, const i32 y, const bool create) {
	if(!created_dir && create){
		created_dir = makedir(dir);
		if(!created_dir){
			std::cerr << "Could not create directory! (" << strerror(errno) << ")" << std::endl;
			return nullptr;
		}
	}
	const i32 rx = x >> 5;
	const i32 ry = y >> 5;
	const u64 mkey(key(rx, ry));
	const auto nsearch = nonexistant.find(mkey);
	if(nsearch != nonexistant.end() && !create){
		return nullptr;
	} else if(create){
		nonexistant.erase(mkey);
	}
	const auto search = handles.find(mkey);
	std::fstream * handle = nullptr;
	if(search == handles.end()){
		const std::string path(dir + "r." + std::to_string(rx) + "." + std::to_string(ry) + ".pxr");
		if(fileExists(path)){
			handle = new std::fstream(path, std::fstream::in | std::fstream::out | std::fstream::binary);
			/* std::cout << "Read file: '" << path << "'" << std::endl; */
		} else if(create){
			handle = new std::fstream(path, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::trunc);
			if(handle && handle->good()){
				u8 zero[3072]; /* There must be a better way of doing this */
				std::fill_n(zero, sizeof(zero), (u8)0);
				handle->write((char *)&zero, sizeof(zero));
				/* std::cout << "Made file: '" << path << "'" << std::endl; */
			}
		} else {
			/* We tried reading, didn't find the file, don't try to read it again. */
			if(nonexistant.size() >= 512){
				nonexistant.clear();
			}
			nonexistant.emplace(mkey);
		}
		if(handle && handle->good()){
			if(handles.size() > WORLD_MAX_FILE_HANDLES){
				auto it = handles.begin();
				/* ugly, get first element inserted */
				for(auto it2 = handles.begin(); ++it2 != handles.end(); ++it);
				/* look at the inline key func for explanation */
				/*std::cout << "Closed file handle to: '" << dir << "r."
					<< *((i32 *)it->first.c_str()) << "."
					<< *((i32 *)(it->first.c_str() + sizeof(i32))) << ".pxr'" << std::endl;*/
				delete it->second;
				handles.erase(it);
			}
			handles[mkey] = handle;
		} else if(handle) {
			std::cerr << "Could not create/read file in '" << path << "'! (" << strerror(errno) << ")" << std::endl;
			delete handle;
			handle = nullptr;
		}
	} else {
		handle = search->second;
		if(handle && !handle->good()){
			std::cerr << "A file handle has gone bad: '" << dir << "/r." << rx << "." << ry << ".pxr'" << std::endl;
			delete handle;
			handles.erase(search);
			/* oh boy, not sure if this will fix anything */
			handle = get_handle(x, y, create);
		}
	}
	return handle;
}

bool Database::get_chunk(const i32 x, const i32 y, char * const arr) {
	std::fstream * const file = get_handle(x, y, false);
	if(!file || !file->good()){
		return false;
	}
	file->seekg(0, std::fstream::end);
	const sz_t size = file->tellg();
	const u32 lookup = 3 * ((x & 31) + (y & 31) * 32);
	file->seekg(lookup);
	u32 chunkpos = 0;
	file->read(((char *)&chunkpos) + 1, 3);
	if(chunkpos == 0 || size < 3072 || size - chunkpos < 768){
		return false;
	}
	file->seekg(chunkpos);
	file->read(arr, 768);
	return true;
}

void Database::set_chunk(const i32 x, const i32 y, const char * const arr) {
	std::fstream * const file = get_handle(x, y, true);
	if(!file || !file->good()){
		std::cerr << "Could not save chunk X: " << x << ",  Y: " << y << std::endl;
		return;
	}
	file->seekg(0, std::fstream::end);
	const sz_t size = file->tellg();
	const u32 lookup = 3 * ((x & 31) + (y & 31) * 32);
	file->seekg(lookup);
	u32 chunkpos = 0;
	file->read(((char *)&chunkpos) + 1, 3);
	/* TODO: check for corruption */
	if(chunkpos == 0){
		file->seekp(lookup);
		file->write(((char *)&size) + 1, 3);
		chunkpos = size;
	}
	file->seekp(chunkpos);
	file->write(arr, 768);
	file->flush();
}
