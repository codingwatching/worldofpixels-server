#pragma once
#include <string>
#include <map>

class PropertyReader {
	const std::string filePath;
	bool propsChanged;
	std::map<std::string, std::string> props;

public:
	PropertyReader(const std::string filePath);
	~PropertyReader();
	
	bool readFromDisk();
	bool writeToDisk(bool force = false);

	bool hasProp(const std::string key);
	std::string getProp(const std::string key, const std::string defval = "");
	void setProp(const std::string key, const std::string value);
};
