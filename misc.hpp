struct logininfo_t {
	uint8_t status;
	uint32_t id;
	uint8_t rank;
	std::string name;
};

size_t getUTF8strlen(const std::string& str);
bool validate_name(const std::string& str);

class IDSys {
	uint32_t ids;
	std::set<uint32_t> freeids;

public:
	IDSys();
	uint32_t get_id();
	void free_id(const uint32_t);
};

class LoginManager {
	const std::string path;
	IDSys ids;
	/*std::unordered_map<std::string, std::string> udb; // user db
	std::unordered_map<std::string, int64_t> ipbdb; // ipbans db
	std::unordered_map<std::string, int64_t> ubdb; // userbans db*/

public:
	LoginManager(const std::string& path);
	logininfo_t guest_login();
	logininfo_t user_login(const std::string& name, const std::string& password);
	logininfo_t user_signup(const std::string& name, const std::string& password);

	void logout(const uint32_t id);
};

