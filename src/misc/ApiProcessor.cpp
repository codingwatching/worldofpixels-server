#include "ApiProcessor.hpp"

#include <uWS.h>
#include <misc/utils.hpp>

#include <nlohmann/json.hpp>

using Request = ApiProcessor::Request;
using Endpoint = ApiProcessor::Endpoint;

using TemplatedEndpointBuilder = ApiProcessor::TemplatedEndpointBuilder;

template<typename... Args>
using TemplatedEndpoint = ApiProcessor::TemplatedEndpoint<Args...>;

ApiProcessor::ApiProcessor(uWS::Hub& h) {
	// one connection can request multiple things before it closes
	h.onHttpRequest([this] (uWS::HttpResponse * res, uWS::HttpRequest req, char * data, sz_t len, sz_t rem) {
		std::shared_ptr<Request> * rs = static_cast<std::shared_ptr<Request> *>(res->getHttpSocket()->getUserData());

		if (!rs) {
			auto reqtmp(std::make_shared<Request>(res, &req));
			rs = new std::shared_ptr<Request>(std::move(reqtmp));
			res->getHttpSocket()->setUserData(rs);
		} else {
			(*rs)->updateData(res, &req);
		}

		nlohmann::json j;

		if (len != 0) {
			j = nlohmann::json::parse(data, data + len, nullptr, false);
		}

		exec(*rs, std::move(j), tokenize(req.getUrl().toString(), '/', true));
		(*rs)->invalidateData();
	});

	h.onCancelledHttpRequest([] (uWS::HttpResponse * res) {
		// requests only get cancelled when the requester disconnects, right?
		if (auto * rs = static_cast<std::shared_ptr<Request> *>(res->getHttpSocket()->getUserData())) {
			(*rs)->cancel(std::move(*rs));
			delete rs;
			res->getHttpSocket()->setUserData(nullptr);
		}
	});

	h.onHttpDisconnection([] (uWS::HttpSocket<uWS::SERVER> * s) {
		// this library is retarded so the disconnection handler is called
		// before the cancelled requests handler, so i need to do hacky things
		// if i don't want to use freed memory (there's no request completion handler), cool!
		std::shared_ptr<Request> * rs = static_cast<std::shared_ptr<Request> *>(s->getUserData());
		if (!s->outstandingResponsesHead) {
			// seems like no cancelled request handler will be called
			delete rs; // note: deleting null is ok
		}
	});
}

TemplatedEndpointBuilder ApiProcessor::on(ApiProcessor::Method m, ApiProcessor::AccessRules ar) {
	return TemplatedEndpointBuilder(*this, m, ar);
}

void ApiProcessor::add(ApiProcessor::Method m, std::unique_ptr<Endpoint> ep) {
	definedEndpoints[m].emplace_back(std::move(ep));
}

void ApiProcessor::exec(std::shared_ptr<Request> r, nlohmann::json j, std::vector<std::string> args) {
	int m = r->getData()->getMethod(); // lol

	if (m == ApiProcessor::Method::INVALID) {
		return;
	}

	for (auto& ep : definedEndpoints[m]) {
		if (ep->verify(args)) {
			ep->exec(std::move(r), std::move(j), std::move(args));
			break;
		}
	}

	r->writeStatus("501 Not Implemented");
	r->end();
}


Request::Request(uWS::HttpResponse * res, uWS::HttpRequest * req)
: res(res),
  req(req) { }

uWS::HttpResponse * Request::getResponse() {
	return res;
}

uWS::HttpRequest * Request::getData() {
	return req;
}

void Request::writeStatus(std::string s) {
	s.reserve(s.size() + 12);
	s.insert(0, "HTTP/1.1 ");
	s.append("\r\n");

	res->write(s.data(), s.size());
}

void Request::writeHeader(std::string key, std::string value) {
	key.reserve(key.size() + value.size() + 8);
	key.append(": ");
	key.append(value);
	key.append("\r\n");

	res->write(key.data(), key.size());
}

void Request::end(const u8 * buf, sz_t size) {
	if (res->hasHead) {
		writeHeader("Content-Length", std::to_string(size));
		res->write("\r\n", 2);
	}

	res->end(reinterpret_cast<const char *>(buf), size);
}

void Request::end(nlohmann::json j) {
	std::string s(j.dump());
	if (!res->hasHead) {
		writeStatus("200 OK");
	}

	writeHeader("Content-Type", "application/json");
	end(reinterpret_cast<const u8 *>(s.data()), s.size());
}

void Request::end() {
	if (res->hasHead) {
		res->write("Content-Length: 0\r\n\r\n", 21);
	}

	res->end();
}

bool Request::isCancelled() const {
	return res == nullptr;
}

void Request::onCancel(std::function<void(std::shared_ptr<Request>)> f) {
	cancelHandler = std::move(f);
}

void Request::cancel(std::shared_ptr<Request> r) {
	// careful, this could be the last reference
	if (cancelHandler) {
		cancelHandler(std::move(r));
	}
}

void Request::updateData(uWS::HttpResponse * res, uWS::HttpRequest * req) {
	res = res;
	req = req;
	cancelHandler = nullptr;
}

void Request::invalidateData() {
	req = nullptr;
}




TemplatedEndpointBuilder::TemplatedEndpointBuilder(ApiProcessor& tc, ApiProcessor::Method m, ApiProcessor::AccessRules ar)
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

Endpoint::Endpoint(ApiProcessor::AccessRules ar)
: ar(ar) { }

Endpoint::~Endpoint() { }
