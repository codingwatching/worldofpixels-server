#include "IdSys.hpp"

IdSys::IdSys()
: currentId(0) { }

std::uint32_t IdSys::getId() {
	std::uint32_t id;
	if (!freeIds.empty()) {
		auto it = freeIds.begin();
		id = *it;
		freeIds.erase(it);
	} else {
		id = ++currentId;
	}

	return id;
}

void IdSys::freeId(std::uint32_t id) {
	if (id == currentId) {
		--currentId;
	} else {
		freeIds.emplace(id);
	}

	shrink();
}

void IdSys::shrink() {
	if (!freeIds.empty()) {
		auto it = freeIds.end();
		while (--it != freeIds.begin() && *it == currentId) {
			it = freeIds.erase(it);
			--currentId;
		}
	}
}