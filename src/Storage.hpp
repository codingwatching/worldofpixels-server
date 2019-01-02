#pragma once

#include <string>
#include <functional>
#include <tuple>
#include <vector>
#include <map>
#include <set>

#include <BansManager.hpp>
#include <explints.hpp>
#include <PropertyReader.hpp>
#include <color.hpp>

class World;
class Storage;

union twoi32 {
	struct {
		i32 x;
		i32 y;
	};
	u64 pos;
};

bool operator<(const twoi32& a, const twoi32& b);

class WorldStorage : PropertyReader {
	const std::string worldDir; // path for the world files
	const std::string worldName;

	std::map<u64, std::vector<twoi32>> pclust;
	std::set<twoi32> remainingOldClusters;

	// worldDir = directory of this world's data
	WorldStorage(std::string worldDir, std::string worldName);
	WorldStorage(std::tuple<std::string, std::string>);

public:
	// delete the copy constructors, we only want this class to be moved at most
	//WorldStorage(WorldStorage&&) = default;
	//WorldStorage& operator=(WorldStorage&&) = default;
	WorldStorage(const WorldStorage&) = delete;
	~WorldStorage();

	const std::string& getWorldName() const;
	const std::string& getWorldDir() const;
	std::string getChunkFilePath(i32 x, i32 y) const;

	bool save();

	bool hasMotd();
	bool hasPassword();

	u16 getPixelRate();
	RGB_u getBackgroundColor() const;
	std::string getMotd() const;
	std::string getPassword();
	bool getGlobalModeratorsAllowed(); // to be removed

	void setPixelRate(u16);
	void setBackgroundColor(RGB_u);
	void setMotd(std::string);
	void setPassword(std::string);
	void setGlobalModeratorsAllowed(bool);

	void convertNext();
	void maybeConvertChunk(i32, i32);
	void maybeConvert(i32, i32);
	void loadProtectionData();
	void saveProtectionData();

	friend World;
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
	std::tuple<std::string, std::string> getWorldStorageArgsFor(const std::string& worldName);
};
