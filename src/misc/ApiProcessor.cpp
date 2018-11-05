#include "ApiProcessor.hpp"

#include <uWS.h>
#include <misc/utils.hpp>

using ApiProcessor::Request;
using ApiProcessor::Endpoint;
using ApiProcessor::TemplatedEndpointBuilder;
using ApiProcessor::TemplatedEndpoint;

ApiProcessor::ApiProcessor(uWS::Hub& h, std::string defaultRequest) {
	// one connection can request multiple things before it closes
	h.onHttpRequest([this, dr{std::move(defaultRequest)}] (uWS::HttpResponse * res, uWS::HttpRequest req, char * data, sz_t len, sz_t rem) {
		RequestStorage * rs = static_cast<RequestStorage*>(res->getHttpSocket()->getUserData());
		if (!rs) {
			rs = new RequestStorage;
			res->getHttpSocket()->setUserData(rs);
		} else {
			rs->onCancel = nullptr; // I'm gonna get mad if requests can be made on the same socket before the last one completes
		}

		auto args(tokenize(req.getUrl().toString(), '/', true));

		if (args.size() == 0) {
			args.emplace_back(dr);
		}

		if (auto status = exec(res, *rs, std::move(args))) {
			res->end("\"Unknown request\"", 17);
		}
	});

	h.onCancelledHttpRequest([] (uWS::HttpResponse * res) {
		RequestStorage * rs = static_cast<RequestStorage*>(res->getHttpSocket()->getUserData());
		if (rs && rs->onCancel) {
			rs->onCancel();
		}

		delete rs;
		res->getHttpSocket()->setUserData(nullptr);
	});

	h.onHttpDisconnection([] (uWS::HttpSocket<uWS::SERVER> * s) {
		// this library is retarded so the disconnection handler is called
		// before the cancelled requests handler, so i need to do hacky things
		// if i don't want to use freed memory (there's no request completion handler), cool!
		RequestStorage * rs = static_cast<RequestStorage*>(s->getUserData());
		if (!s->outstandingResponsesHead) {
			// seems like no cancelled request handler will be called
			delete rs; // note: deleting null is ok
		}
	});
}

void ApiProcessor::set(std::string name, ApiProcessor::Func f) {
	methods[name] = std::move(f);
}

ApiProcessor::Status ApiProcessor::exec(uWS::HttpResponse * r, RequestStorage& rs, ApiProcessor::ArgList args) {
	if (args.size() == 0) return UNK_REQ;

	auto s = methods.find(args[0]);
	if (s != methods.end()) {
		return s->second(r, rs, std::move(args));
	}

	return UNK_REQ;
}





TemplatedEndpointBuilder::TemplatedEndpointBuilder(ApiProcessor& tc, Method m, AccessRules ar)
: targetClass(tc),
  method(m),
  ar(ar) { }

TemplatedEndpointBuilder& TemplatedEndpointBuilder::path(std::string s) {
	if (s.size() == 0) {
		throw std::runtime_error("Path sections can't be empty!");
	}

#pragma message("TODO: Check for non-URL characters?")

	varMarkers.emplace_back(std::move(s));
	return *this;
}

TemplatedEndpointBuilder& TemplatedEndpointBuilder::var() {
	varMarkers.emplace_back("");
	return *this;
}

template<typename... Args>
void TemplatedEndpointBuilder::end(std::function<void(std::shared_ptr<Request>, nlohmann::json, Args...)> f) {
	targetClass.add(method, std::make_unique<TemplatedEndpoint<Args...>>(ar, std::move(f), std::move(varMarkers)));
}







Endpoint::Endpoint(AccessRules ar)
: ar(ar) { }

Endpoint::~Endpoint() { }







template<typename... Args>
TemplatedEndpoint<Args...>::TemplatedEndpoint(AccessRules ar, std::function<void(std::shared_ptr<Request>, nlohmann::json, Args...)> f, std::vector<std::string> path)
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
bool TemplatedEndpoint<Args...>::verify(const std::vector<std::string>& args) {
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
void TemplatedEndpoint<Args...>::exec(std::shared_ptr<Request> r, nlohmann::json j, const std::vector<std::string>& args) {
	execImpl(std::move(r), std::move(j), args, std::make_index_sequence<sizeof... (Args)>{});
}

template<typename... Args>
template<std::size_t... Is>
void TemplatedEndpoint<Args...>::execImpl(std::shared_ptr<Request> r, nlohmann::json j, const std::vector<std::string>& args, std::index_sequence<Is...>) {
	handler(std::move(r), std::move(j), fromString<Args>(args[varPositions[Is]])...);
}
