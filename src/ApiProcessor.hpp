#pragma once

#include <functional>
#include <array>
#include <vector>
#include <memory>
#include <string>

#include <AuthManager.hpp>

#include <misc/explints.hpp>
#include <misc/fwd_uWS.h>
#include <misc/shared_ptr_ll.hpp>

#include <nlohmann/json_fwd.hpp>

class Request;
class Session;

class ApiProcessor {
public:
	class Endpoint;
	class TemplatedEndpointBuilder;

	template<typename Func, typename... Args>
	class TemplatedEndpoint;

	enum AccessRules : u8 {
		FOREIGN    = 0b01,
		AUTHORIZED = 0b10
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
	AuthManager& am;
	/* one vector for each method, except invalid */
	std::array<std::vector<std::unique_ptr<Endpoint>>, 9> definedEndpoints;

public:
	ApiProcessor(uWS::Hub&, AuthManager&);

	TemplatedEndpointBuilder on(Method, AccessRules = AccessRules::AUTHORIZED);
	void add(Method, std::unique_ptr<Endpoint>);

private:
	void exec(ll::shared_ptr<Request>, nlohmann::json, std::vector<std::string>);
	Session * getSession(Request&);
};

class Request {
	std::function<void(ll::shared_ptr<Request>)> cancelHandler;
	std::string bufferedData;

	uWS::HttpResponse * res;
	uWS::HttpRequest * req;

public:
	Request(uWS::HttpResponse *, uWS::HttpRequest *);

	uWS::HttpResponse * getResponse();
	uWS::HttpRequest * getData();

	void writeStatus(std::string);
	void writeStatus(const char *, sz_t);
	void writeHeader(std::string key, std::string val);
	void end(const char *, sz_t);
	void end(nlohmann::json);
	void end();

	bool isCancelled() const;
	void onCancel(std::function<void(ll::shared_ptr<Request>)>);

private:
	void write(const char *, sz_t);
	void writeAndEnd(const char *, sz_t);

	void cancel(ll::shared_ptr<Request>);
	void updateData(uWS::HttpResponse *, uWS::HttpRequest *);
	void invalidateData();

	friend ApiProcessor;
};

class ApiProcessor::TemplatedEndpointBuilder {
	ApiProcessor& targetClass;

	std::vector<std::string> varMarkers;
	Method method;
	AccessRules ar;

	TemplatedEndpointBuilder(ApiProcessor&, Method, AccessRules);

public:
	TemplatedEndpointBuilder& path(std::string);
	TemplatedEndpointBuilder& var();

	template<typename Func>
	void end(Func);

	friend ApiProcessor;
};

class ApiProcessor::Endpoint {
	AccessRules ar;

public:
	Endpoint(AccessRules);
	virtual ~Endpoint();

	AccessRules getRules() const;

	virtual bool verify(const std::vector<std::string>&) = 0;
	virtual void exec(ll::shared_ptr<Request>, nlohmann::json, std::vector<std::string>);
	virtual void exec(ll::shared_ptr<Request>, nlohmann::json, Session&, std::vector<std::string>);
};

template<typename Func, typename... Args>
class ApiProcessor::TemplatedEndpoint : public ApiProcessor::Endpoint {
	Func handler;

	const std::vector<std::string> pathSections; // empty str = variable placeholder
	std::array<u8, sizeof... (Args)> varPositions; // indexes of variables in path

public:
	TemplatedEndpoint(AccessRules, Func, std::vector<std::string>);

	bool verify(const std::vector<std::string>&);
	void exec(ll::shared_ptr<Request>, nlohmann::json, std::vector<std::string>);

private:
	template<std::size_t... Is>
	void execImpl(ll::shared_ptr<Request>, nlohmann::json, std::vector<std::string>, std::index_sequence<Is...>);
};

#include "ApiProcessor.tpp"
