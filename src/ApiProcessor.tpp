#include <nlohmann/json.hpp>
#include <stringparser.hpp>

template<typename Func>
void ApiProcessor::TemplatedEndpointBuilder::onOutsider(bool exclusive, Func f) {
	targetClass.add(method, std::make_unique<ApiProcessor::OutsiderTemplatedEndpoint<Func>>(
		std::move(varMarkers), std::move(f), exclusive));
}

template<typename Func>
void ApiProcessor::TemplatedEndpointBuilder::onFriend(Func f) {
	targetClass.add(method, std::make_unique<ApiProcessor::FriendTemplatedEndpoint<Func>>(
		std::move(varMarkers), std::move(f)));
}

template<typename OutsiderFunc, typename FriendFunc>
void ApiProcessor::TemplatedEndpointBuilder::onAny(OutsiderFunc of, FriendFunc ff) {
	targetClass.add(method, std::make_unique<ApiProcessor::UniversalTemplatedEndpoint<OutsiderFunc, FriendFunc>>(
		std::move(varMarkers), std::move(of), std::move(ff)));
}

template<typename TTuple>
ApiProcessor::TemplatedEndpoint<TTuple>::TemplatedEndpoint(std::vector<std::string> path, bool exclusive)
: Endpoint(exclusive),
  pathSections(std::move(path)) {
	sz_t j = 0;
	for (sz_t i = 0; i < pathSections.size(); i++) {
		if (pathSections[i].size() == 0) {
			if (j == varPositions.size()) {
				throw std::runtime_error("Templated arg count > Path var count");
			}

			varPositions[j++] = i;
		}
	}

	if (j != varPositions.size()) {
		throw std::runtime_error("Templated arg count (" + std::to_string(varPositions.size()) + ") != Path var count (" + std::to_string(j) + ")");
	}
}

template<typename TTuple>
bool ApiProcessor::TemplatedEndpoint<TTuple>::verify(const std::vector<std::string>& args) {
	if (args.size() != pathSections.size()) {
		return false;
	}

	for (sz_t i = 0; i < pathSections.size(); i++) {
		if (pathSections[i].size() != 0 && pathSections[i] != args[i]) {
			return false;
		}
	}

	return true;
}

template<typename TTuple>
TTuple ApiProcessor::TemplatedEndpoint<TTuple>::getTuple(std::vector<std::string> args) {
	return getTupleImpl(std::move(args), std::make_index_sequence<std::tuple_size<TTuple>::value>{});
}

template<typename TTuple>
template<std::size_t... Is>
TTuple ApiProcessor::TemplatedEndpoint<TTuple>::getTupleImpl(std::vector<std::string> args, std::index_sequence<Is...>) {
	return TTuple{fromString<typename std::tuple_element<Is, TTuple>::type>(args[varPositions[Is]])...};
}

template<typename Func, typename TTuple>
ApiProcessor::OutsiderTemplatedEndpoint<Func, TTuple>::OutsiderTemplatedEndpoint(std::vector<std::string> path, Func f, bool exclusive)
: TemplatedEndpoint<TTuple>(std::move(path), exclusive),
  outsiderHandler(std::move(f)) { }

template<typename Func, typename TTuple>
void ApiProcessor::OutsiderTemplatedEndpoint<Func, TTuple>::exec(ll::shared_ptr<Request> req, nlohmann::json j, std::vector<std::string> args) {
	multiApply(outsiderHandler, ApiProcessor::TemplatedEndpoint<TTuple>::getTuple(std::move(args)), std::move(req), std::move(j));
}

template<typename Func, typename TTuple>
ApiProcessor::FriendTemplatedEndpoint<Func, TTuple>::FriendTemplatedEndpoint(std::vector<std::string> path, Func f)
: TemplatedEndpoint<TTuple>(std::move(path)),
  friendHandler(std::move(f)) { }

template<typename Func, typename TTuple>
void ApiProcessor::FriendTemplatedEndpoint<Func, TTuple>::exec(ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::vector<std::string> args) {
	multiApply(friendHandler, ApiProcessor::TemplatedEndpoint<TTuple>::getTuple(std::move(args)), std::move(req), std::move(j), s);
}

template<typename OutsiderFunc, typename FriendFunc>
ApiProcessor::UniversalTemplatedEndpoint<OutsiderFunc, FriendFunc>::UniversalTemplatedEndpoint(std::vector<std::string> path, OutsiderFunc of, FriendFunc ff)
: TemplatedEndpoint<typename OutsiderTemplatedEndpoint<OutsiderFunc>::Tuple>(std::move(path)),
  OutsiderTemplatedEndpoint<OutsiderFunc>({}, std::move(of)),
  FriendTemplatedEndpoint<FriendFunc>({}, std::move(ff)) { }
