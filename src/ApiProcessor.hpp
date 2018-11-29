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
#include <misc/tuple.hpp>

#include <nlohmann/json_fwd.hpp>

class Request;
class Session;

class ApiProcessor {
public:
	class Endpoint;
	class TemplatedEndpointBuilder;

	template<typename Tuple>
	class TemplatedEndpoint;

	template<typename Func, typename Tuple = typename sliceTuple<typename lambdaToTuple<Func>::type, 2>::type>
	class OutsiderTemplatedEndpoint;

	template<typename Func, typename Tuple = typename sliceTuple<typename lambdaToTuple<Func>::type, 3>::type>
	class FriendTemplatedEndpoint;

	template<typename OutsiderFunc, typename FriendFunc>
	class UniversalTemplatedEndpoint;

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

	TemplatedEndpointBuilder on(Method);
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

	TemplatedEndpointBuilder(ApiProcessor&, Method);

public:
	TemplatedEndpointBuilder& path(std::string);
	TemplatedEndpointBuilder& var();

	template<typename Func>
	void onOutsider(bool exclusive, Func);

	template<typename Func>
	void onFriend(Func);

	template<typename OutsiderFunc, typename FriendFunc>
	void onAny(OutsiderFunc, FriendFunc);

	friend ApiProcessor;
};

class ApiProcessor::Endpoint {
	bool outsiderExclusive;

public:
	Endpoint(bool);
	virtual ~Endpoint();

	virtual bool verify(const std::vector<std::string>&) = 0;
	virtual void exec(ll::shared_ptr<Request>, nlohmann::json, std::vector<std::string>);
	virtual void exec(ll::shared_ptr<Request>, nlohmann::json, Session&, std::vector<std::string>);
};

template<typename TTuple>
class ApiProcessor::TemplatedEndpoint : public ApiProcessor::Endpoint {
public:
	using Tuple = TTuple;

private:
	const std::vector<std::string> pathSections; // empty str = variable placeholder
	std::array<u8, std::tuple_size<TTuple>::value> varPositions; // indexes of variables in path

public:
	TemplatedEndpoint(std::vector<std::string>, bool = false);

	bool verify(const std::vector<std::string>&);

protected:
	TTuple getTuple(std::vector<std::string>);

private:
	template<std::size_t... Is>
	TTuple getTupleImpl(std::vector<std::string>, std::index_sequence<Is...>);
};

template<typename Func, typename TTuple>
class ApiProcessor::OutsiderTemplatedEndpoint
: public virtual TemplatedEndpoint<TTuple> {
	Func outsiderHandler;

public:
	OutsiderTemplatedEndpoint(std::vector<std::string>, Func, bool = false);

	void exec(ll::shared_ptr<Request>, nlohmann::json, std::vector<std::string>);
};

template<typename Func, typename TTuple>
class ApiProcessor::FriendTemplatedEndpoint
: public virtual TemplatedEndpoint<TTuple> {
	Func friendHandler;

public:
	FriendTemplatedEndpoint(std::vector<std::string>, Func);

	void exec(ll::shared_ptr<Request>, nlohmann::json, Session&, std::vector<std::string>);
};

template<typename OutsiderFunc, typename FriendFunc>
class ApiProcessor::UniversalTemplatedEndpoint
: public ApiProcessor::OutsiderTemplatedEndpoint<OutsiderFunc>,
  public ApiProcessor::FriendTemplatedEndpoint<FriendFunc> {
	static_assert(std::is_same<
			typename OutsiderTemplatedEndpoint<OutsiderFunc>::Tuple,
			typename FriendTemplatedEndpoint<FriendFunc>::Tuple
		>::value,
		"Outsider/Friend endpoint parameters should be the same");

public:
	UniversalTemplatedEndpoint(std::vector<std::string>, OutsiderFunc, FriendFunc);
};

#include "ApiProcessor.tpp"
