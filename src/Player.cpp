#include "Player.hpp"

#include <memory>
#include <iostream>

#include <World.hpp>
#include <Client.hpp>
#include <UserInfo.hpp>
#include <PacketDefinitions.hpp>

#include <nlohmann/json.hpp>

Player::Player(Client& c, World& w, u32 pid, World::Pos startX, World::Pos startY,
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
  modifyWorldAllowed(mod),
  toolId(0),
  pixelStep(0) {
	// send player data to the client
	world.playerJoined(*this);
  	std::cout << "New player on world: " << world.getWorldName() << ", PID: " << playerId << ", UID: " << getUserInfo().uid << std::endl;
}

Player::Player(const Player::Builder& pb)
: Player(*pb.cl, *pb.wo, pb.playerId, pb.startX, pb.startY, pb.paintLimiter,
	pb.chatLimiter, pb.chatAllowed, pb.cmdsAllowed, pb.modifyWorldAllowed) { }


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

World::Pos Player::getX() const {
	return x;
}

World::Pos Player::getY() const {
	return y;
}

Player::Step Player::getStep() const {
	return pixelStep;
}

Player::Id Player::getPid() const {
	return playerId;
}

void Player::teleportTo(World::Pos newX, World::Pos newY) {
	x = newX;
	y = newY;
	world.playerUpdated(*this);
}

void Player::tell(const std::string& s) {
	ChatMessage::one(cl.getWs(), 0, s);
}

void Player::tryPaint(World::Pos x, World::Pos y, RGB_u rgb) {
	world.paint(*this, x, y, rgb);
}

void Player::tryMoveTo(World::Pos newX, World::Pos newY, Step prec, Tid newToolId) {
	x = newX;
	y = newY;
	toolId = newToolId;
	pixelStep = prec;
	world.playerUpdated(*this);
}

void Player::tryChat(const std::string& s) {
	world.chat(*this, std::move(s));
}

void Player::send(const PrepMsg& p) {
	cl.send(p);
}

bool Player::operator ==(const Player& p) const {
	// XXX: why would you compare players from different worlds?
	return playerId == p.playerId;
}

bool Player::operator  <(const Player& p) const {
	return playerId < p.playerId;
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

Player::Builder& Player::Builder::setPlayerId(Player::Id pId) {
	playerId = pId;
	return *this;
}

Player::Builder& Player::Builder::setSpawnPoint(World::Pos x, World::Pos y) {
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

/*Player Player::Builder::build() {
	return Player(*cl, *wo, playerId, startX, startY,
		std::move(paintLimiter), std::move(chatLimiter),
		chatAllowed, cmdsAllowed, modifyWorldAllowed);
}
*/
