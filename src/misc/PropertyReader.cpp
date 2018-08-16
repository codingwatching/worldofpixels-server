#include "PropertyReader.hpp"

#include <stdexcept>
#include <cstdint>
#include <fstream>

PropertyReader::PropertyReader(const std::string path)
: filePath(path),
  propsChanged(false) {
	readFromDisk();
}

PropertyReader::PropertyReader(PropertyReader&& p)
: filePath(std::move(p.filePath)),
  propsChanged(p.propsChanged),
  props(std::move(p.props)) {
  	// prevent the moved class from saving to the file on destruction
	p.propsChanged = false;
}

PropertyReader::~PropertyReader() {
	writeToDisk();
}

/* Returns false if it couldn't open the file */
bool PropertyReader::readFromDisk() {
	std::string prop;
	std::ifstream file(filePath);
	bool ok = !!file;
	while (file.good()) {
		std::getline(file, prop);
		if (prop.size() > 0) {
			std::size_t keylen = prop.find_first_of(' ');
			if (keylen != std::string::npos) {
				props[prop.substr(0, keylen)] = prop.substr(keylen + 1);
			}
		}
		prop.clear();
	}
	return ok;
}

bool PropertyReader::writeToDisk(bool force) {
	if (!(propsChanged || force)) {
		return true;
	}

	bool ok = true;
	if (props.size() != 0) {
		std::ofstream file(filePath, std::ios_base::trunc);
		ok = !!file;
		if (file.good()) {
			for (auto & kv : props) {
				file << kv.first << " " << kv.second << "\n";
			}
		}
	} else {
		std::remove(filePath.c_str());
	}

	return ok;
}

bool PropertyReader::isEmpty() const {
	return props.empty();
}

bool PropertyReader::hasProp(std::string key) const {
	return props.find(key) != props.end();
}

std::string PropertyReader::getProp(std::string key, std::string defval) const {
	auto search = props.find(key);
	if (search != props.end()) {
		return search->second;
	}
	return defval;
}

std::string PropertyReader::getOrSetProp(std::string key, std::string defval) {
	if (!hasProp(key)) {
		setProp(key, defval);
		return defval;
	}
	return getProp(key);
}

void PropertyReader::setProp(std::string key, std::string value) {
	if (!value.size()) {
		props.erase(key);
	} else {
		props[key] = value;
	}
	propsChanged = true;
}

bool PropertyReader::delProp(std::string key) {
	return props.erase(key);
}
