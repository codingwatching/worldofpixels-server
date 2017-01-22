class LoginManager;
class WorldManager;

/*enum login_sbound : uint8_t {
	LOGIN,
	REGISTER,
	GUEST_LOGIN
};*/

enum login_cbound : uint8_t {
	LOGIN_RESPONSE
};

/*enum play_sbound : uint8_t {
	REQUEST_CHUNK,
	PUT_PIXEL,
	PLAYER_UPDATE,
	FILL_CHUNK,
	CL_CHAT_MESSAGE,
	CHANGE_WORLD
};*/

enum play_cbound : uint8_t {
	JOIN_WORLD,
	PERMISSIONS,
	CHUNK_DATA,
	WORLD_UPDATE,
	CHAT_MESSAGE,
	TELEPORT
};

class NetStage {
	std::vector<std::function<void(Client * const, const char *, const size_t)>> netmsgs;
	NetStage * const next_stage;

public:
	NetStage(std::vector<std::function<void(Client * const, const char *, const size_t)>>, NetStage * const);
	void process_msg(Client * const, const char * msg, const size_t len);
	void upgrade(Client * const);
};

class NetStageLogin : NetStage {
public:
	NetStageLogin(NetStage * const, LoginManager * const);
	static void login_user(NetStage * const, LoginManager * const, Client * const, const char * msg, const size_t len);
	static void register_user(NetStage * const, LoginManager * const, Client * const, const char * msg, const size_t len);
	static void login_guest(NetStage * const, LoginManager * const, Client * const, const char * msg, const size_t len);
};

class NetStagePlay : NetStage {
public:
	NetStagePlay(WorldManager * const);
	static void req_chunk(Client * const, const char * msg, const size_t len);
	static void put_pixel(Client * const, const char * msg, const size_t len);
	static void player_upd(Client * const, const char * msg, const size_t len);
	static void fill_chunk(Client * const, const char * msg, const size_t len); /* prob to be replaced by special admin messages? */
	static void send_chat(Client * const, const char * msg, const size_t len);
	static void change_world(WorldManager * const, Client * const, const char * msg, const size_t len);
};
