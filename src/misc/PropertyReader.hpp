#pragma once
#include <string>
#include <map>

class PropertyReader {
	const std::string filePath;
	bool propsChanged;
	std::map<std::string, std::string> props;

public:
	PropertyReader(const std::string filePath);

	PropertyReader(PropertyReader&&);
	PropertyReader(const PropertyReader&) = delete;
	PropertyReader& operator=(const PropertyReader&) = delete;

	~PropertyReader();

	bool readFromDisk();
	bool writeToDisk(bool force = false);

	bool isEmpty() const;
	bool hasProp(std::string key) const;
	std::string getProp(std::string key, std::string defval = "") const;
	std::string getOrSetProp(std::string key, std::string defval);
	void setProp(std::string key, std::string value);
	bool delProp(std::string key);
};
