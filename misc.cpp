#include <set>
#include <iostream>
#include <memory.h>
#include "misc.hpp"

size_t getUTF8strlen(const std::string& str) {
	size_t j = 0, i = 0, x = 1;
	while (i < str.size()) {
		if (x > 4) { /* Invalid unicode */
			return SIZE_MAX;
		}
		
		if ((str[i] & 0xC0) != 0x80) {
			j += x == 4 ? 2 : 1;
			x = 1;
		} else {
			x++;
		}
		i++;
	}
	if (x == 4) {
		j++;
	}
	return (j);
}

bool validate_name(const std::string& str) {
	/* Only allow chars a..z, 0..9, '_' and '.' */
	for(size_t i = str.size(); i--;){
		if(!((str[i] > 96 && str[i] < 123) ||
		     (str[i] > 47 && str[i] < 58) ||
		      str[i] == 95 || str[i] == 46)){
			return false;
		}
	}
	return true;
}

IDSys::IDSys()
	: ids(0) { }

uint32_t IDSys::get_id() {
	uint32_t id = 0;
	if(!freeids.empty()){
		auto it = freeids.end();
		id = *--it;
		freeids.erase(it);
	} else {
		id = ++ids;
	}
	std::cout << "Got id: " << id << std::endl;
	return id;
}

void IDSys::free_id(const uint32_t id) {
	if(id == ids){
		--ids;
	} else {
		freeids.emplace(id);
	}
	if(!freeids.empty()){
		auto it = freeids.end();
		while(--it != freeids.begin() && *it == ids){
			std::cout << "Erased " << *it << std::endl;
			freeids.erase(it);
			--ids;
			it = freeids.end();
		}
	}
}

LoginManager::LoginManager(const std::string& path)
	: path(path),
	  ids() { }

logininfo_t LoginManager::guest_login() {
	uint32_t id = ids.get_id();
	std::string name("Guest " + std::to_string(id));
	return {0, id, 0, name};
}

void LoginManager::logout(const uint32_t id) {
	ids.free_id(id);
}
