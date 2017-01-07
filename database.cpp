#include "server.hpp"

inline bool file_exists(const std::string& name) {
    return ( access( name.c_str(), F_OK ) != -1 );
}

/* Database class functions */

Database::Database(const std::string& dir)
	: dir(dir),
	  created_dir(file_exists(dir)) { }

Database::~Database() {
	for(const auto& hdl : handles){
		delete hdl.second;
	}
	handles.clear();
}

std::fstream * Database::get_handle(const int32_t x, const int32_t y, const bool create) {
	if(!created_dir && create){
		created_dir = (mkdir(dir.c_str(), 0700) == 0);
		if(!created_dir){
			std::cerr << "Could not create directory! (" << strerror(errno) << ")" << std::endl;
			return nullptr;
		}
	}
	const int32_t rx = x >> 5;
	const int32_t ry = y >> 5;
	const std::string mkey(key(rx, ry));
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
		if(file_exists(path)){
			handle = new std::fstream(path, std::fstream::in | std::fstream::out | std::fstream::binary);
			/* std::cout << "Read file: '" << path << "'" << std::endl; */
		} else if(create){
			handle = new std::fstream(path, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::trunc);
			if(handle && handle->good()){
				uint8_t zero[3072]; /* There must be a better way of doing this */
				memset(&zero, 0, sizeof(zero));
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
				std::cout << "Closed file handle to: '" << dir << "r."
					<< *((int32_t *)it->first.c_str()) << "."
					<< *((int32_t *)(it->first.c_str() + sizeof(int32_t))) << ".pxr'" << std::endl;
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

bool Database::get_chunk(const int32_t x, const int32_t y, char * const arr) {
	std::fstream * const file = get_handle(x, y, false);
	if(!file || !file->good()){
		return false;
	}
	file->seekg(0, std::fstream::end);
	const size_t size = file->tellg();
	const uint32_t lookup = 3 * ((x & 31) + (y & 31) * 32);
	file->seekg(lookup);
	uint32_t chunkpos = 0;
	file->read(((char *)&chunkpos) + 1, 3);
	if(chunkpos == 0 || size < 3072 || size - chunkpos < 768){
		return false;
	} 
	file->seekg(chunkpos);
	file->read(arr, 768);
	return true;
}

void Database::set_chunk(const int32_t x, const int32_t y, const char * const arr) {
	std::fstream * const file = get_handle(x, y, true);
	if(!file || !file->good()){
		std::cerr << "Could not save chunk X: " << x << ",  Y: " << y << std::endl;
		return;
	}
	file->seekg(0, std::fstream::end);
	const size_t size = file->tellg();
	const uint32_t lookup = 3 * ((x & 31) + (y & 31) * 32);
	file->seekg(lookup);
	uint32_t chunkpos = 0;
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
