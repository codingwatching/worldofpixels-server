#include "AsyncHttp.hpp"

#include <uWS.h>
#include <curl/curl.h>

/* FIXME: Name resolves seem to be blocking (see: c-ares) */

void mc(CURLMcode code) {
	if (code != CURLM_OK && code != CURLM_BAD_SOCKET) {
		throw std::runtime_error(std::string("CURLM call failed: ") + curl_multi_strerror(code));
	}
}

void ec(CURLcode code) {
	if (code != CURLE_OK) {
		throw std::runtime_error(std::string("CURL call failed: ") + curl_easy_strerror(code));
	}
}

static struct CurlRaii {
	CurlRaii() { ec(curl_global_init(CURL_GLOBAL_DEFAULT)); }
	~CurlRaii() { curl_global_cleanup(); }
} autoCleanup;

inline int curlEvToUv(int curlEvents) {
	return (curlEvents & CURL_POLL_IN ? UV_READABLE : 0)
		| (curlEvents & CURL_POLL_OUT ? UV_WRITABLE : 0);
}

inline int uvEvToCurl(int uvEvents) {
	return (uvEvents & UV_READABLE ? CURL_CSELECT_IN : 0)
		| (uvEvents & UV_WRITABLE ? CURL_CSELECT_OUT : 0);
}

class CurlHandle {
	CURLM * multiHandle;
	CURL * easyHandle;
	std::function<void(AsyncHttp::Result)> onFinished;
	std::string buffer;

public:
	CurlHandle(CURLM *, std::string, std::unordered_map<std::string, std::string>,
			std::function<void(AsyncHttp::Result)>);
	~CurlHandle();

	CURL * getHandle();

private:
	bool finished(CURLcode result); /* Returns false if error occurred */
	static int writer(char *, std::size_t, std::size_t, std::string *);

	friend AsyncHttp;
};

class CurlSocket : public uS::Poll {
	AsyncHttp * ah;
	void (*cb)(AsyncHttp *, CurlSocket *, int, int);

public:
	CurlSocket(uS::Loop * loop, AsyncHttp * ah, curl_socket_t fd)
	: Poll(loop, static_cast<int>(fd)),
	  ah(ah) { }

	void start(uS::Loop * loop, int events, void (*callback)(AsyncHttp *, CurlSocket *, int status, int events)) {
		cb = callback;
		Poll::setCb([] (Poll * p, int s, int e) {
			CurlSocket * cs = static_cast<CurlSocket *>(p);
			cs->cb(cs->ah, cs, s, uvEvToCurl(e));
		});

		Poll::start(loop, this, curlEvToUv(events));
	}

	void change(uS::Loop * loop, int events) {
		Poll::change(loop, this, curlEvToUv(events));
	}

	void close(uS::Loop * loop) {
		Poll::stop(loop);
		Poll::close(loop, [] (Poll * p) {
			delete static_cast<CurlSocket *>(p);
		});
	}
};

AsyncHttp::Result::Result(long httpResp, std::string data, const char * err)
: successful(err == nullptr),
  responseCode(httpResp),
  data(std::move(data)),
  errorString(err) { }


CurlHandle::CurlHandle(CURLM * mHdl, std::string url,
		std::unordered_map<std::string, std::string> params, std::function<void(AsyncHttp::Result)> cb)
: multiHandle(mHdl),
  easyHandle(curl_easy_init()),
  onFinished(std::move(cb)) {
	if (!easyHandle) {
		throw std::bad_alloc();
	}

	bool first = true;
	for (const auto& param : params) {
		url += first ? '?' : '&';
		first = false;
		url += param.first; // should this be escaped as well?
		url += '=';
		// lots of allocs can happen here, bad!
		char * escaped = curl_easy_escape(easyHandle, param.second.c_str(), param.second.size());
		if (!escaped) { throw std::bad_alloc(); }
		url += escaped;
		curl_free(escaped);
	}

	ec(curl_easy_setopt(easyHandle, CURLOPT_PRIVATE, this));
	ec(curl_easy_setopt(easyHandle, CURLOPT_WRITEFUNCTION, &CurlHandle::writer));
	ec(curl_easy_setopt(easyHandle, CURLOPT_WRITEDATA, &buffer));
	ec(curl_easy_setopt(easyHandle, CURLOPT_URL, url.c_str()));

	//ec(curl_easy_setopt(easyHandle, CURLOPT_TIMEOUT, 60)); // idk if this caused any problems
	//ec(curl_easy_setopt(easyHandle, CURLOPT_TCP_FASTOPEN, 1)); // makes http requests fail?
	ec(curl_easy_setopt(easyHandle, CURLOPT_TCP_NODELAY, 0));
	ec(curl_easy_setopt(easyHandle, CURLOPT_PIPEWAIT, 1));
	//ec(curl_easy_setopt(easyHandle, CURLOPT_VERBOSE, 1));

	mc(curl_multi_add_handle(multiHandle, easyHandle));
}

CurlHandle::~CurlHandle() {
	mc(curl_multi_remove_handle(multiHandle, easyHandle));
	curl_easy_cleanup(easyHandle);
}

CURL * CurlHandle::getHandle() {
	return easyHandle;
}

bool CurlHandle::finished(CURLcode result) {
	bool successful = result == CURLE_OK;
	long responseCode = -1;
	ec(curl_easy_getinfo(easyHandle, CURLINFO_RESPONSE_CODE, &responseCode));

	onFinished({responseCode, std::move(buffer),
			successful ? nullptr : curl_easy_strerror(result)});

	return successful;
}

int CurlHandle::writer(char * data, std::size_t size, std::size_t nmemb, std::string * writerData) {
	// writerData will never be null
	writerData->append(data, size * nmemb);

	return size * nmemb;
}

AsyncHttp::AsyncHttp(uS::Loop * loop)
: loop(loop),
  timer(new uS::Timer(loop)),
  multiHandle(curl_multi_init()),
  handleCount(0),
  isTimerRunning(false) {
	if (!multiHandle) {
		throw std::bad_alloc();
	}

	timer->setData(this);

	mc(curl_multi_setopt(multiHandle, CURLMOPT_PIPELINING, CURLPIPE_HTTP1 | CURLPIPE_MULTIPLEX));
	mc(curl_multi_setopt(multiHandle, CURLMOPT_MAX_TOTAL_CONNECTIONS, 8));
	mc(curl_multi_setopt(multiHandle, CURLMOPT_MAX_HOST_CONNECTIONS, 1));
	mc(curl_multi_setopt(multiHandle, CURLMOPT_MAXCONNECTS, 8));

	mc(curl_multi_setopt(multiHandle, CURLMOPT_TIMERDATA, this));
	mc(curl_multi_setopt(multiHandle, CURLMOPT_TIMERFUNCTION, +[] (CURLM * multi, long tmo_ms, void * u) -> int {
		// notice the plus sign on the lambda definition, needed to get the pointer to it
		AsyncHttp * ah = static_cast<AsyncHttp *>(u);
		switch (tmo_ms) {
			case -1:
				ah->stopTimer();
				break;

			case 0:
				mc(curl_multi_socket_action(ah->multiHandle, CURL_SOCKET_TIMEOUT, 0, &ah->handleCount));
				break;

			default:
				ah->startTimer(tmo_ms);
				break;
		}

		return 0;
	}));

	mc(curl_multi_setopt(multiHandle, CURLMOPT_SOCKETDATA, this));
	mc(curl_multi_setopt(multiHandle, CURLMOPT_SOCKETFUNCTION, +[] (CURL * e, curl_socket_t s, int what, void * up, void * sp) -> int {
		// listen for event what on socket s of easy handle e, s priv data on sp
		AsyncHttp * ah = static_cast<AsyncHttp *>(up);
		CurlSocket * cs = static_cast<CurlSocket *>(sp);

		if (what == CURL_POLL_REMOVE) {
			if (cs) {
				cs->close(ah->loop); // also deletes when the event loop is ready
				mc(curl_multi_assign(ah->multiHandle, s, nullptr));
			}
		} else {
			if (!cs) {
				cs = new CurlSocket(ah->loop, ah, s);
				cs->start(ah->loop, what, +[] (AsyncHttp * ah, CurlSocket * cs, int status, int events) {
					mc(curl_multi_socket_action(ah->multiHandle, reinterpret_cast<curl_socket_t>(cs->getFd()), events, &ah->handleCount));
					ah->processCompleted();

					if (ah->handleCount == 0) {
						ah->stopTimer();
					}
				});

				mc(curl_multi_assign(ah->multiHandle, s, cs));
			} else {
				cs->change(ah->loop, what);
			}
		}

		return 0;
	}));
}

AsyncHttp::~AsyncHttp() {
	stopTimer();
	timer->close(); /* This deletes the timer */

	for (CurlHandle * ch : pendingRequests) {
		delete ch;
	}

	mc(curl_multi_cleanup(multiHandle));
}

int AsyncHttp::activeHandles() const {
	return handleCount;
}

int AsyncHttp::queuedRequests() const {
	return pendingRequests.size();
}

void AsyncHttp::addRequest(std::string url, std::unordered_map<std::string, std::string> params,
		std::function<void(AsyncHttp::Result)> onFinished) {
	pendingRequests.emplace(new CurlHandle(multiHandle, std::move(url), std::move(params), std::move(onFinished)));
}

void AsyncHttp::addRequest(std::string url, std::function<void(AsyncHttp::Result)> onFinished) {
	addRequest(std::move(url), {}, std::move(onFinished));
}

void AsyncHttp::update() {
	mc(curl_multi_socket_action(multiHandle, CURL_SOCKET_TIMEOUT, 0, &handleCount));
	processCompleted();
}

void AsyncHttp::processCompleted() {
	int queued;
	CurlHandle * hdl;

	while (CURLMsg * m = curl_multi_info_read(multiHandle, &queued)) {
		if (m->msg == CURLMSG_DONE) {
			ec(curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &hdl));

			auto search = pendingRequests.find(hdl);

			if (search != pendingRequests.end()) {
				pendingRequests.erase(search);
			}

			hdl->finished(m->data.result);
			delete hdl;
		}
	}
}

void AsyncHttp::startTimer(long timeout) {
	if (isTimerRunning) {
		stopTimer();
	}

	if (!isTimerRunning) {
		timer->start(&AsyncHttp::timerCallback, timeout, timeout);
		isTimerRunning = true;
	}
}

void AsyncHttp::stopTimer() {
	if (isTimerRunning) {
		timer->stop();
		isTimerRunning = false;
	}
}

void AsyncHttp::timerCallback(uS::Timer * timer) {
	AsyncHttp * const http = static_cast<AsyncHttp *>(timer->getData());
	http->stopTimer();
	http->update();
}
