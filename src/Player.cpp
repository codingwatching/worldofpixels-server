#include "Player.hpp"

#include <memory>
#include <iostream>

#include <Client.hpp>
#include <World.hpp>

Player::Player(Client& c, World& w, u32 pid, i32 startX, i32 startY,
		Bucket pL, Bucket cL, bool chat, bool cmds, bool mod)
: cl(c),
  world(w),
  playerId(pid),
  x(startX),
  y(startY),
  paintLimiter(std::move(pL)),
  chatLimiter(std::move(cL)),
  chatAllowed(chat), // TODO: get from world
  cmdsAllowed(cmds),
  modifyWorldAllowed(mod) {
	// send player data to the client
  	std::cout << "New player on world: " << world.getWorldName() << ", PID: " << playerId << ", UID: " << getUserInfo().uid << std::endl;
}

Player::~Player() {
	world.playerLeft(*this);
}

bool Player::canChat() const {
	return chatAllowed;
}

bool Player::canUseCommands() const {
	return cmdsAllowed;
}

bool Player::canModifyWorld() const {
	return modifyWorldAllowed;
}

Client& Player::getClient() const {
	return cl;
}

World& Player::getWorld() const {
	return world;
}

UserInfo& Player::getUserInfo() const {
	return cl.getUserInfo();
}

i32 Player::getX() const {
	return x;
}

i32 Player::getY() const {
	return y;
}

u32 Player::getPid() const {
	return playerId;
}

void Player::teleportTo(i32 newX, i32 newY) {
	x = newX;
	y = newY;
	world.playerUpdated(*this);
}

void Player::tell(const std::string& s) {
	send({
		{"t", "pm"},
		{"from", 0},
		{"msg", s}
	});
}

void Player::tryPaint(i32 x, i32 y, RGB_u rgb) {
	world.paint(*this, x, y, rgb);
}

void Player::tryMoveTo(i32 newX, i32 newY, u8 newToolId) {
	x = newX;
	y = newY;
	toolId = newToolId;
	world.playerUpdated(*this);
}

void Player::tryChat(const std::string& s) {
	world.chat(*this, std::move(s));
}

void Player::send(const u8 * buf, sz_t len, bool text) {
	cl.send(buf, len, text);
}

void Player::send(const nlohmann::json& j) {
	std::string s(j.dump());
	send(reinterpret_cast<const u8*>(s.data()), s.size(), true);
}

bool Player::operator ==(const Player& p) const {
	// compare client classes, which will compare socket pointers
	return cl == p.cl;
}


Player::Builder::Builder()
: cl(nullptr),
  wo(nullptr),
  playerId(-1),
  startX(0),
  startY(0),
  paintLimiter(0, 0),
  chatLimiter(0, 0),
  chatAllowed(true),
  cmdsAllowed(true),
  modifyWorldAllowed(true) { }

// oh god
Player::Builder& Player::Builder::setClient(Client& c) {
	cl = std::addressof(c);
	return *this;
}

Player::Builder& Player::Builder::setWorld(World& w) {
	wo = std::addressof(w);
	return *this;
}

Player::Builder& Player::Builder::setPlayerId(u32 pId) {
	playerId = pId;
	return *this;
}

Player::Builder& Player::Builder::setSpawnPoint(i32 x, i32 y) {
	startX = x;
	startY = y;
	return *this;
}

Player::Builder& Player::Builder::setPaintBucket(Bucket b) {
	paintLimiter = std::move(b);
	return *this;
}

Player::Builder& Player::Builder::setChatBucket(Bucket b) {
	chatLimiter = std::move(b);
	return *this;
}

Player::Builder& Player::Builder::setChatAllowed(bool s) {
	chatAllowed = s;
	return *this;
}

Player::Builder& Player::Builder::setCmdsAllowed(bool s) {
	cmdsAllowed = s;
	return *this;
}

Player::Builder& Player::Builder::setModifyWorldAllowed(bool s) {
	modifyWorldAllowed = s;
	return *this;
}

Player Player::Builder::build() {
	return Player(*cl, *wo, playerId, startX, startY,
		std::move(paintLimiter), std::move(chatLimiter),
		chatAllowed, cmdsAllowed, modifyWorldAllowed);
}
