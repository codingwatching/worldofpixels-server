#include "AsyncPostgres.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

#include <misc/explints.hpp>

#include <uWS.h>

void getStringPointers(const std::unordered_map<std::string, std::string>& str, const char ** k, const char ** v) {
	sz_t i = 0;
	for (auto it = str.begin(); it != str.end(); ++it) {
		k[i] = it->first.c_str();
		v[i] = it->second.c_str();
		++i;
	}

	// write null termination
	k[i] = v[i] = nullptr;
}

int postgresEvToUv(int ev) {
	switch (ev) {
		case PGRES_POLLING_READING:
			return UV_READABLE;

		case PGRES_POLLING_WRITING:
			return UV_WRITABLE;

		default:
			return 0;
	}
}

class PostgresSocket : uS::Poll {
	AsyncPostgres * ap;
	void (*cb)(AsyncPostgres *, PostgresSocket *, int, int);

public:
	PostgresSocket(AsyncPostgres * ap, int socketfd)
	: Poll(ap->loop, socketfd),
	  ap(ap) { }

	void start(int events, void (*callback)(AsyncPostgres *, PostgresSocket *, int status, int events)) {
		cb = callback;
		Poll::setCb([] (Poll * p, int s, int e) {
			PostgresSocket * cs = static_cast<PostgresSocket *>(p);
			cs->cb(cs->ap, cs, s, e);
		});

		Poll::start(ap->loop, this, events);
	}

	void change(int events) {
		Poll::change(ap->loop, this, events);
	}

	void close() {
		Poll::stop(ap->loop);
		Poll::close(ap->loop, [] (Poll * p) {
			delete static_cast<PostgresSocket *>(p);
		});
	}

	void setCb(void (*callback)(AsyncPostgres *, PostgresSocket *, int status, int events)) {
		cb = callback;
	}
};

AsyncPostgres::AsyncPostgres(uS::Loop * loop)
: loop(loop),
  nextCommandCaller(new uS::Async(loop), [] (uS::Async * a) { a->close(); }),
  pgConn(nullptr, PQfinish),
  pSock(nullptr, [] (PostgresSocket * p) { p->close(); }),
  notifFunc([] (Notification) {}),
  stopOnceEmpty(false),
  busy(false) {
  	nextCommandCaller->setData(this);
	nextCommandCaller->start(AsyncPostgres::nextCmdCallerCallback);
}

void AsyncPostgres::connect(std::unordered_map<std::string, std::string> connParams, bool expandDbname) {
	const char * keywords[connParams.size() + 1]; // +1 for null termination
	const char * values[connParams.size() + 1];
	getStringPointers(connParams, keywords, values);

	pgConn.reset(PQconnectStartParams(keywords, values, expandDbname));
	if (!pgConn) {
		throw std::bad_alloc();
	}

	if (getStatus() == CONNECTION_BAD) {
		std::string err("Invalid parameters passed to PQconnectStartParams (CONNECTION_BAD)");
		err += PQerrorMessage(pgConn.get());
		throw std::invalid_argument(err);
	}

	if (PQsetnonblocking(pgConn.get(), true) == -1) {
		throwLastError();
	}

	busy = true; // queue queries until we're connected

	pSock.reset(new PostgresSocket(this, PQsocket(pgConn.get())));
	pSock->start(UV_WRITABLE, +[] (AsyncPostgres * ap, PostgresSocket * ps, int s, int e) {
		std::cout << "connecting... (" << s << ", " << e << ")" << std::endl;

		int newEvs = PQconnectPoll(ap->pgConn.get());
		switch (newEvs) {
			case PGRES_POLLING_FAILED:
				std::cerr << "PGRES_POLLING_FAILED" << std::endl;
				ap->throwLastError();
				break;

			case PGRES_POLLING_OK:
				std::cout << "PGRES_POLLING_OK" << std::endl;
				ps->setCb(AsyncPostgres::socketCallback);
				ap->busy = false;
				ap->signalCompletion();
				break;

			case PGRES_POLLING_WRITING:
			case PGRES_POLLING_READING:
				ps->change(postgresEvToUv(newEvs));
				break;
		}
	});
}

void AsyncPostgres::connectBlocking(std::unordered_map<std::string, std::string> connParams, bool expandDbname) {
	const char * keywords[connParams.size() + 1]; // +1 for null termination
	const char * values[connParams.size() + 1];
	getStringPointers(connParams, keywords, values);

	pgConn.reset(PQconnectdbParams(keywords, values, expandDbname));
	if (!pgConn) {
		throw std::bad_alloc();
	}

	if (getStatus() == CONNECTION_BAD) {
		std::string err("Invalid parameters passed to PQconnectdbParams (CONNECTION_BAD)");
		err += PQerrorMessage(pgConn.get());
		throw std::invalid_argument(err);
	}

	if (PQsetnonblocking(pgConn.get(), true) == -1) {
		throwLastError();
	}

	pSock.reset(new PostgresSocket(this, PQsocket(pgConn.get())));
	pSock->start(UV_READABLE, AsyncPostgres::socketCallback);
}

void AsyncPostgres::lazyDisconnect() {
	stopOnceEmpty = true;
	if (!busy) {
		disconnect();
	}
}

void AsyncPostgres::disconnect() {
	pSock = nullptr;
	pgConn = nullptr;
	nextCommandCaller = nullptr; // !! not re-allocated if user connects again
	busy = false;
	stopOnceEmpty = false;
}

bool AsyncPostgres::isConnected() const {
	return getStatus() == CONNECTION_OK;
}

ConnStatusType AsyncPostgres::getStatus() const {
	return PQstatus(pgConn.get());
}

sz_t AsyncPostgres::queuedQueries() const {
	return queries.size();
}

void AsyncPostgres::setNotifyFunc(std::function<void(Notification)> f) {
	notifFunc = std::move(f);
}

void AsyncPostgres::signalCompletion() {
	nextCommandCaller->send();
}

void AsyncPostgres::processNextCommand() {
	if (!queries.empty()) {
		busy = true;
		if (!queries.front()->send(pgConn.get())) {
			printLastError();
			// XXX: is this ok? maybe look at the error, reconnect and retry if necessary
			currentCommandFinished(nullptr);
		}
	} else {
		busy = false;
		if (stopOnceEmpty) {
			disconnect();
		}
	}
}

// won't work yet with multiple row cb
void AsyncPostgres::currentCommandFinished(PGresult * r) {
	// XXX: no check if empty, shouldn't happen anyways
	queries.front()->done(r);
	queries.pop_front();

	signalCompletion();
}

void AsyncPostgres::manageSocketEvents(bool needsWrite) {
	if (!PQisBusy(pgConn.get())) {
		while (PGresult * r = PQgetResult(pgConn.get())) {
			ExecStatusType s = PQresultStatus(r);
			std::cout << "Popcb " << PQresStatus(s) << std::endl;
			switch (s) { // FIXME?: make it possible to enable single row mode
				case PGRES_BAD_RESPONSE:
				case PGRES_FATAL_ERROR:
					std::cerr << "[Postgre/manageSocketEvents()]: Bad result status " << PQresStatus(s) << std::endl;
					printLastError();
					break;
			}

			currentCommandFinished(r);
		}
	}

	// always listen for read because the server can send us notices at any time?
	int evs = UV_READABLE;
	evs |= needsWrite ? UV_WRITABLE : 0;

	pSock->change(evs);
}

void AsyncPostgres::printLastError() {
	std::cerr << PQerrorMessage(pgConn.get()) << std::endl;
}

void AsyncPostgres::throwLastError() {
	throw std::runtime_error(PQerrorMessage(pgConn.get()));
}

void AsyncPostgres::socketCallback(AsyncPostgres * ap, PostgresSocket * ps, int s, int e) {
	bool needsWrite = false;

	if (e & UV_READABLE) {
		if (!PQconsumeInput(ap->pgConn.get())) {
			ap->printLastError();
		}

		if (PGnotify * p = PQnotifies(ap->pgConn.get())) {
			ap->notifFunc(p);
		}
	}

	if (e & UV_WRITABLE) {
		needsWrite = PQflush(ap->pgConn.get());
	}

	ap->manageSocketEvents(needsWrite);
}

void AsyncPostgres::nextCmdCallerCallback(uS::Async * a) {
	AsyncPostgres * ap = static_cast<AsyncPostgres *>(a->getData());
	ap->processNextCommand();
}


AsyncPostgres::Query::Query(std::string cmd, const char ** vals, const int * lens, const int * fmts, int n)
: command(std::move(cmd)),
  onDone([] (AsyncPostgres::Result) {}),
  values(vals),
  lengths(lens),
  formats(fmts),
  nParams(n) { }

AsyncPostgres::Query::~Query() { }

void AsyncPostgres::Query::then(std::function<void(AsyncPostgres::Result)> f) {
	onDone = std::move(f);
}

int AsyncPostgres::Query::send(PGconn * conn) {
	return PQsendQueryParams(conn, command.c_str(), nParams,
		nullptr, values, lengths, formats, 0);
}

void AsyncPostgres::Query::done(AsyncPostgres::Result r) {
	onDone(std::move(r));
}


AsyncPostgres::Result::Result(PGresult * r)
: pgResult(r, PQclear) { }

sz_t AsyncPostgres::Result::size() const {
	return pgResult ? PQntuples(pgResult.get()) : 0;
}

sz_t AsyncPostgres::Result::rowSize() const {
	return pgResult ? PQnfields(pgResult.get()) : 0;
}

bool AsyncPostgres::Result::success() const {
	if (!pgResult.get()) { return false; }

	switch (PQresultStatus(pgResult.get())) {
		case PGRES_COMMAND_OK:
		case PGRES_TUPLES_OK:
		case PGRES_SINGLE_TUPLE:
			return true;
	}

	return false;
}

AsyncPostgres::Result::iterator AsyncPostgres::Result::begin() { return iterator(pgResult.get(), 0); }
AsyncPostgres::Result::iterator AsyncPostgres::Result::end()   { return iterator(pgResult.get(), size()); }

AsyncPostgres::Result::Row AsyncPostgres::Result::operator[](int i) { return Row(pgResult.get(), i); }

AsyncPostgres::Result::operator bool() const { return success(); }


AsyncPostgres::Result::Row::Row(PGresult * r, int i)
: r(r),
  rowIndex(i) { }

int AsyncPostgres::Result::Row::getRow() { return rowIndex; }


AsyncPostgres::Result::iterator::iterator(PGresult * r, int i)
: r(r),
  rowIndex(i) { }

AsyncPostgres::Result::iterator& AsyncPostgres::Result::iterator::operator++() {
	++rowIndex;
	return *this;
}

AsyncPostgres::Result::iterator  AsyncPostgres::Result::iterator::operator++(int) {
	auto it = *this;
	++rowIndex;
	return it;
}

bool AsyncPostgres::Result::iterator::operator==(iterator it) const {
	return rowIndex == it.rowIndex;
}

bool AsyncPostgres::Result::iterator::operator!=(iterator it) const {
	return rowIndex != it.rowIndex;
}

AsyncPostgres::Result::iterator::reference AsyncPostgres::Result::iterator::operator*() const {
	return AsyncPostgres::Result::Row(r, rowIndex);
}


AsyncPostgres::Notification::Notification(PGnotify * n)
: pgNotify(n, [] (PGnotify * n) { PQfreemem(n); }) { }

std::string AsyncPostgres::Notification::channelName() const { return pgNotify->relname; }
int         AsyncPostgres::Notification::bePid()       const { return pgNotify->be_pid; }
std::string AsyncPostgres::Notification::extra()       const { return pgNotify->extra; }
