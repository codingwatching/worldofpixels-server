#include "UviasRank.hpp"

#include <nlohmann/json.hpp>

UviasRank::UviasRank(UviasRank::Id id, std::string name, bool superUser, bool selfManage)
: name(std::move(name)),
  id(id),
  superUser(superUser),
  selfManage(selfManage) { }

UviasRank::Id UviasRank::getId() const {
	return id;
}

std::string_view UviasRank::getName() const {
	return name;
}

bool UviasRank::isSuperUser() const {
	return superUser;
}

bool UviasRank::canSelfManage() const {
	return selfManage;
}

bool UviasRank::deepEqual(const UviasRank& uv2) const {
	return id == uv2.id && name == uv2.name
		&& superUser == uv2.superUser
		&& selfManage == uv2.selfManage;
}

bool UviasRank::operator ==(const UviasRank& uv2) const {
	return id == uv2.id;
}

void to_json(nlohmann::json& j, const UviasRank& r) {
	j = {
		{ "id", r.getId() },
		{ "name", r.getName() },
		{ "superUser", r.isSuperUser() },
		{ "canSelfManage", r.canSelfManage() }
	};
}
