#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <tuple>
#include <iterator>
#include <deque>

#include <misc/fwd_uWS.h>
#include <misc/explints.hpp>
#include <misc/tuple.hpp>

#include <postgresql/libpq-fe.h>

class PostgresSocket;

class AsyncPostgres {
public:
	class Query;
	template<typename... Ts>
	class TemplatedQuery;
	class Result;
	class Notification;

private:
	uS::Loop * loop;
	std::unique_ptr<uS::Async, void (*)(uS::Async *)> nextCommandCaller;
	std::unique_ptr<PGconn, void (*)(PGconn *)> pgConn;
	std::unique_ptr<PostgresSocket, void (*)(PostgresSocket *)> pSock;
	std::deque<std::unique_ptr<Query>> queries;

	std::function<void(Notification)> notifFunc;
	bool stopOnceEmpty;
	bool busy;

public:
	AsyncPostgres(uS::Loop *);

	void connect(std::unordered_map<std::string, std::string> connParams = {}, bool expandDbname = false);
	void connectBlocking(std::unordered_map<std::string, std::string> connParams = {}, bool expandDbname = false);
	void lazyDisconnect(); // will disconnect when the query queue is empty
	void disconnect();

	template<typename... Ts>
	Query& query(std::string, Ts&&...);

	bool isConnected() const;
	ConnStatusType getStatus() const;
	sz_t queuedQueries() const;

	void setNotifyFunc(std::function<void(Notification)>);

private:
	void signalCompletion();
	void processNextCommand();
	void currentCommandFinished(PGresult *);
	void manageSocketEvents(bool);

	void printLastError();
	void throwLastError();

	static void socketCallback(AsyncPostgres *, PostgresSocket *, int, int);
	static void nextCmdCallerCallback(uS::Async *);

	friend PostgresSocket;
};

class AsyncPostgres::Query {
	std::string command;
	std::function<void(Result)> onDone;
	const char ** values;
	const int * lengths;
	const int * formats;
	int nParams;

public:
	Query(std::string, const char **, const int *, const int *, int);
	virtual ~Query();

	void then(std::function<void(Result)>);
	//void cancel();

private:
	int send(PGconn *);
	void done(Result);

	friend AsyncPostgres;
};

template<typename... Ts>
class AsyncPostgres::TemplatedQuery : public AsyncPostgres::Query {
	std::tuple<Ts...> valueStorage;
	const char * realValues[sizeof... (Ts)];
	const int realLengths[sizeof... (Ts)];
	const int realFormats[sizeof... (Ts)];

	template<std::size_t... Is>
	TemplatedQuery(std::index_sequence<Is...>, std::string, Ts&&...);

public:
	TemplatedQuery(std::string, Ts&&...);
};

class AsyncPostgres::Result {
public:
	class Row;
	class iterator;

private:
	std::unique_ptr<PGresult, void (*)(PGresult *)> pgResult;

	Result(PGresult *);

public:
	sz_t size() const;
	sz_t rowSize() const;
	bool success() const;

	iterator begin();
	iterator end();

	template<typename Func, typename Tuple = typename lambdaToTuple<Func>::type>
	void forEach(Func);

	Row operator[](int);
	operator bool() const;

	friend AsyncPostgres;
};

class AsyncPostgres::Result::Row {
	PGresult * r;
	const int rowIndex;

public:
	Row(PGresult *, int);

	template<typename... Ts>
	std::tuple<Ts...> get();

	int getRow();

private:
	template<typename Tuple, std::size_t... Is>
	Tuple getImpl(std::index_sequence<Is...>);

	friend AsyncPostgres::Result;
};

class AsyncPostgres::Result::iterator
: public std::iterator<
	std::random_access_iterator_tag,
	AsyncPostgres::Result::Row,
	int,
	AsyncPostgres::Result::Row *,
	AsyncPostgres::Result::Row
> {
	PGresult * r;
	int rowIndex;

public:
	explicit iterator(PGresult *, int);
	iterator& operator++();
	iterator operator++(int);
	bool operator==(iterator) const;
	bool operator!=(iterator) const;
	reference operator*() const;
};

class AsyncPostgres::Notification {
	std::unique_ptr<PGnotify, void (*)(PGnotify *)> pgNotify;

	Notification(PGnotify *);

public:
	// TODO: change these std::strings to std::string_view
	std::string channelName() const;
	int bePid() const;
	std::string extra() const;

	friend AsyncPostgres;
};

#include "AsyncPostgres.tpp"
