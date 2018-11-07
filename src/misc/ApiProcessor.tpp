#include <nlohmann/json.hpp>
#include <misc/stringparser.hpp>

template<typename... Args>
void ApiProcessor::TemplatedEndpointBuilder::end(std::function<void(std::shared_ptr<ApiProcessor::Request>, nlohmann::json, Args...)> f) {
	targetClass.add(method, std::make_unique<TemplatedEndpoint<Args...>>(ar, std::move(f), std::move(varMarkers)));
}


template<typename... Args>
ApiProcessor::TemplatedEndpoint<Args...>::TemplatedEndpoint(AccessRules ar, std::function<void(std::shared_ptr<ApiProcessor::Request>, nlohmann::json, Args...)> f, std::vector<std::string> path)
: Endpoint(ar),
  handler(std::move(f)),
  pathSections(std::move(path)) {
  	sz_t j = 0;
	for (sz_t i = 0; i < pathSections.size(); i++) {
		if (pathSections[i].size() == 0) {
			if (j == varPositions.size()) {
				throw std::runtime_error("Templated arg count != Path var count");
			}

			varPositions[j++] = i;
		}
	}

	if (j + 1 != varPositions.size()) {
		throw std::runtime_error("Templated arg count != Path var count");
	}
}

template<typename... Args>
bool ApiProcessor::TemplatedEndpoint<Args...>::verify(const std::vector<std::string>& args) {
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

template<typename... Args>
void ApiProcessor::TemplatedEndpoint<Args...>::exec(std::shared_ptr<ApiProcessor::Request> r, nlohmann::json j, std::vector<std::string> args) {
	execImpl(std::move(r), std::move(j), std::move(args), std::make_index_sequence<sizeof... (Args)>{});
}

template<typename... Args>
template<std::size_t... Is>
void ApiProcessor::TemplatedEndpoint<Args...>::execImpl(std::shared_ptr<ApiProcessor::Request> r, nlohmann::json j, std::vector<std::string> args, std::index_sequence<Is...>) {
	handler(std::move(r), std::move(j), fromString<Args>(args[varPositions[Is]])...);
}
