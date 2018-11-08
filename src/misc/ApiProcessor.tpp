#include <nlohmann/json.hpp>
#include <misc/stringparser.hpp>

template<typename Func>
struct TemplatedEndpointFactory : public TemplatedEndpointFactory<decltype(&Func::operator())> {};

template<typename Func, typename ReturnType, typename... Args>
struct TemplatedEndpointFactory<ReturnType(Func::*)(std::shared_ptr<Request>, nlohmann::json, Args...) const> {
	static std::unique_ptr<ApiProcessor::Endpoint> make(ApiProcessor::AccessRules ar, Func f, std::vector<std::string> varMarkers) {
		return std::make_unique<ApiProcessor::TemplatedEndpoint<Func, Args...>>(ar, std::move(f), std::move(varMarkers));
	}
};

template<typename F>
void ApiProcessor::TemplatedEndpointBuilder::end(F f) {
	targetClass.add(method, TemplatedEndpointFactory<F>::make(ar, std::move(f), std::move(varMarkers)));
}


template<typename Func, typename... Args>
ApiProcessor::TemplatedEndpoint<Func, Args...>::TemplatedEndpoint(ApiProcessor::AccessRules ar, Func f, std::vector<std::string> path)
: Endpoint(ar),
  handler(std::move(f)),
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

template<typename Func, typename... Args>
bool ApiProcessor::TemplatedEndpoint<Func, Args...>::verify(const std::vector<std::string>& args) {
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

template<typename Func, typename... Args>
void ApiProcessor::TemplatedEndpoint<Func, Args...>::exec(std::shared_ptr<Request> r, nlohmann::json j, std::vector<std::string> args) {
	execImpl(std::move(r), std::move(j), std::move(args), std::make_index_sequence<sizeof... (Args)>{});
}

template<typename Func, typename... Args>
template<std::size_t... Is>
void ApiProcessor::TemplatedEndpoint<Func, Args...>::execImpl(std::shared_ptr<Request> r, nlohmann::json j, std::vector<std::string> args, std::index_sequence<Is...>) {
	handler(std::move(r), std::move(j), fromString<Args>(args[varPositions[Is]])...);
}
