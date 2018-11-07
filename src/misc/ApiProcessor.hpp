#pragma once

#include <functional>
#include <array>
#include <vector>
#include <memory>
#include <string>

#include <misc/explints.hpp>
#include <misc/fwd_uWS.h>

#include <nlohmann/json_fwd.hpp>

/*
api.onGet()
   .path("view")
   .var()
   .var()
   .var()
.end([] (Request& r, nlohmann::json body, std::string s, i32 x, i32 y) {
});
*/

class ApiProcessor {
public:
	class Request;
	class Endpoint;
	class TemplatedEndpointBuilder;

	template<typename... Args>
	class TemplatedEndpoint;

	enum AccessRules : u8 {
		FOREIGN = 0b00001,
		GUEST   = 0b00010,
		MOD     = 0b00100,
		ADMIN   = 0b01000
	};

	enum Method {
		GET,
		POST,
		PUT,
		DELETE,
		PATCH,
		OPTIONS,
		HEAD,
		TRACE,
		CONNECT,
		INVALID
	};

private:
	/* one vector for each method, except invalid */
	std::array<std::vector<std::unique_ptr<Endpoint>>, 9> definedEndpoints;

public:
	ApiProcessor(uWS::Hub&);

	TemplatedEndpointBuilder on(Method, AccessRules);
	void add(Method, std::unique_ptr<Endpoint>);

private:
	void exec(std::shared_ptr<Request>, nlohmann::json, std::vector<std::string>);
};

class ApiProcessor::Request {
	std::function<void(std::shared_ptr<Request>)> cancelHandler;

	uWS::HttpResponse * res;
	uWS::HttpRequest * req;

public:
	Request(uWS::HttpResponse *, uWS::HttpRequest *);

	uWS::HttpResponse * getResponse();
	uWS::HttpRequest * getData();

	void writeStatus(std::string);
	void writeHeader(std::string key, std::string val);
	void end(const u8 *, sz_t);
	void end(nlohmann::json);
	void end();

	bool isCancelled() const;
	void onCancel(std::function<void(std::shared_ptr<Request>)>);

private:
	// arg will refer to this, moved from the current request map to avoid changing ref for the cancel handler
	void cancel(std::shared_ptr<Request>);
	void updateData(uWS::HttpResponse *, uWS::HttpRequest *);
	void invalidateData();

	friend ApiProcessor;
};

class ApiProcessor::TemplatedEndpointBuilder {
	using Request = ApiProcessor::Request;

	ApiProcessor& targetClass;

	std::vector<std::string> varMarkers;
	Method method;
	AccessRules ar;

	TemplatedEndpointBuilder(ApiProcessor&, Method, AccessRules);

public:
	TemplatedEndpointBuilder& path(std::string);
	TemplatedEndpointBuilder& var();

	template<typename... Args>
	void end(std::function<void(std::shared_ptr<Request>, nlohmann::json, Args...)>);

	friend ApiProcessor;
};

class ApiProcessor::Endpoint {
	using Request = ApiProcessor::Request;
	AccessRules ar;

public:
	Endpoint(AccessRules);
	virtual ~Endpoint();

#pragma message("Endpoint::verify() should take User info as well, or have Request hold it")
	virtual bool verify(const std::vector<std::string>&) = 0;
	virtual void exec(std::shared_ptr<Request>, nlohmann::json, std::vector<std::string>) = 0;
};

template<typename... Args>
class ApiProcessor::TemplatedEndpoint : public ApiProcessor::Endpoint {
	using Request = ApiProcessor::Request;

	std::function<void(std::shared_ptr<Request>, nlohmann::json, Args...)> handler;

	const std::vector<std::string> pathSections; // empty str = variable placeholder
	std::array<u8, sizeof... (Args)> varPositions; // indexes of variables in path

public:
	TemplatedEndpoint(AccessRules, std::function<void(std::shared_ptr<Request>, nlohmann::json, Args...)>, std::vector<std::string>);

	bool verify(const std::vector<std::string>&);
	void exec(std::shared_ptr<Request>, nlohmann::json, std::vector<std::string>);

private:
	template<std::size_t... Is>
	void execImpl(std::shared_ptr<Request>, nlohmann::json, std::vector<std::string>, std::index_sequence<Is...>);
};

#include "ApiProcessor.tpp"