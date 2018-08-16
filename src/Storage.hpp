#pragma once

#include <string>
#include <functional>

#include <BansManager.hpp>
#include <misc/explints.hpp>
#include <misc/PropertyReader.hpp>
#include <misc/color.hpp>

class Storage;

class WorldStorage : PropertyReader {
	const std::string worldDir; // path for the world files
	const std::string worldName;

	// worldDir = directory of this world's data
	WorldStorage(std::string worldDir, std::string worldName);

public:
	// delete the copy constructors, we only want this class to be moved at most
	WorldStorage(WorldStorage&&) = default;
	WorldStorage& operator=(WorldStorage&&) = default;

	const std::string& getWorldName() const;
	const std::string& getWorldDir() const;
	std::string getChunkFilePath(i32 x, i32 y) const;

	void save();

	bool hasMotd();
	bool hasPassword();

	u16 getPixelRate();
	RGB getBackgroundColor();
	std::string getMotd();
	std::string getPassword();
	bool getGlobalModeratorsAllowed(); // to be removed

	void setPixelRate(u16);
	void setBackgroundColor(RGB);
	void setMotd(std::string);
	void setPassword(std::string);
	void setGlobalModeratorsAllowed(bool);

	void convertProtectionData(std::function<void(i32, i32)>);

	friend Storage;
};

// i'm wondering if i should make the PropertyReader functions private
class Storage : public PropertyReader {
	const std::string basePath; // the root dir of the server
	const std::string worldsDir; // the directory name of world data
	const std::string worldDirPath;
	BansManager bm;

public:
	Storage(std::string bPath);

	// no copies of this class
	Storage(const Storage&) = delete;
	Storage& operator=(const Storage&) = delete;

	static std::string getRandomPassword();

	std::string getModPass();
	std::string getAdminPass();
	std::string getBindAddress();
	u16 getBindPort();

	void setModPass(std::string);
	void setAdminPass(std::string);
	void setBindAddress(std::string);
	void setBindPort(u16);

	BansManager& getBansManager();
	WorldStorage getWorldStorageFor(std::string worldName);
};
