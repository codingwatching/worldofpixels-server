#include "AsyncHttp.hpp"

#include <memory>
#include <algorithm>

#include <uWS.h>

/* FIXME: Name resolves seem to be blocking (see: c-ares) */

static struct CurlRaii {
	CurlRaii() { curl_global_init(CURL_GLOBAL_DEFAULT); }
	~CurlRaii() { curl_global_cleanup(); }
} autoCleanup;

AsyncHttp::Result::Result(Result && r)
: successful(r.successful),
  errnum(r.errnum),
  responseCode(r.responseCode),
  data(std::move(r.data)),
  errorString(r.errorString) { }

AsyncHttp::Result::Result(CURLcode cc, long httpResp, std::string && data, const char * err)
: successful(cc == CURLE_OK),
  errnum(cc),
  responseCode(httpResp),
  data(std::move(data)),
  errorString(err) { }



AsyncHttp::CurlHandle::CurlHandle(CurlHandle && c)
: onFinished(std::move(c.onFinished)),
  multiHandle(c.multiHandle),
  easyHandle(c.easyHandle),
  addedToMulti(c.addedToMulti),
  buffer(std::move(c.buffer)) {
	c.multiHandle = nullptr;
	c.easyHandle = nullptr;
	curl_easy_setopt(easyHandle, CURLOPT_PRIVATE, this);
	curl_easy_setopt(easyHandle, CURLOPT_WRITEDATA, &buffer);
}

AsyncHttp::CurlHandle::CurlHandle(CURLM * mHdl, std::string url,
		std::unordered_map<std::string, std::string> params, std::function<void(AsyncHttp::Result)> cb)
: onFinished(std::move(cb)),
  multiHandle(mHdl),
  easyHandle(curl_easy_init()),
  addedToMulti(false) {
	if (!easyHandle) {
		throw std::bad_alloc();
	}

	bool first = true;
	for (const auto & param : params) {
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

	curl_easy_setopt(easyHandle, CURLOPT_PRIVATE, this);
	//curl_easy_setopt(easyHandle, CURLOPT_TIMEOUT, 60);
	curl_easy_setopt(easyHandle, CURLOPT_TCP_FASTOPEN, 1);
	curl_easy_setopt(easyHandle, CURLOPT_WRITEFUNCTION, &AsyncHttp::CurlHandle::writer);
	curl_easy_setopt(easyHandle, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(easyHandle, CURLOPT_PIPEWAIT, 1);

	curl_easy_setopt(easyHandle, CURLOPT_URL, url.c_str());

	addToMulti();
}

AsyncHttp::CurlHandle::~CurlHandle() {
	if (easyHandle) {
		rmFromMulti();
		curl_easy_cleanup(easyHandle);
		easyHandle = nullptr;
	}
}

void AsyncHttp::CurlHandle::addToMulti() {
	if (!addedToMulti) {
		curl_multi_add_handle(multiHandle, easyHandle);
		addedToMulti = true;
	}
}

void AsyncHttp::CurlHandle::rmFromMulti() {
	if (addedToMulti) {
		curl_multi_remove_handle(multiHandle, easyHandle);
		addedToMulti = false;
	}
}

CURL * AsyncHttp::CurlHandle::getHandle() {
	return easyHandle;
}

bool AsyncHttp::CurlHandle::finished(CURLcode result) {
	bool successful = result == CURLE_OK;
	long responseCode = -1;
	curl_easy_getinfo(easyHandle, CURLINFO_RESPONSE_CODE, &responseCode);

	onFinished({result, responseCode, std::move(buffer),
			successful ? nullptr : curl_easy_strerror(result)});

	return successful;
}

int AsyncHttp::CurlHandle::writer(char * data, std::size_t size,
		std::size_t nmemb, std::string * writerData) {
	if (writerData == nullptr) {
		return 0;
	}

	writerData->append(data, size * nmemb);

	return size * nmemb;
}




AsyncHttp::AsyncHttp(AsyncHttp && http)
: timer(http.timer),
  pendingRequests(std::move(http.pendingRequests)),
  isTimerRunning(http.isTimerRunning),
  multiHandle(http.multiHandle),
  handleCount(http.handleCount) {
	http.timer = nullptr;
	http.multiHandle = nullptr;
}

AsyncHttp::AsyncHttp(uS::Loop * loop)
: timer(new uS::Timer(loop)),
  isTimerRunning(false),
  multiHandle(curl_multi_init()),
  handleCount(0) {
	if (!multiHandle) {
		throw std::bad_alloc();
	}

	timer->setData(this);

	curl_multi_setopt(multiHandle, CURLMOPT_PIPELINING, CURLPIPE_HTTP1 | CURLPIPE_MULTIPLEX);
	curl_multi_setopt(multiHandle, CURLMOPT_MAX_TOTAL_CONNECTIONS, 4);
	curl_multi_setopt(multiHandle, CURLMOPT_MAX_HOST_CONNECTIONS, 4);
	curl_multi_setopt(multiHandle, CURLMOPT_MAXCONNECTS, 6);
}

AsyncHttp::~AsyncHttp() {
	stopTimer();
	timer->close(); /* This deletes the timer */
	pendingRequests.clear();
	if (multiHandle) {
		curl_multi_cleanup(multiHandle);
	}
}

int AsyncHttp::activeHandles() {
	return handleCount;
}

void AsyncHttp::addRequest(std::string url, std::unordered_map<std::string, std::string> params,
		std::function<void(AsyncHttp::Result)> onFinished) {
	pendingRequests.push_back(std::make_unique<CurlHandle>(multiHandle, std::move(url), std::move(params), std::move(onFinished)));
	setNewTimeout();
}

void AsyncHttp::addRequest(std::string url, std::function<void(AsyncHttp::Result)> onFinished) {
	addRequest(std::move(url), {{}}, std::move(onFinished));
}

void AsyncHttp::update() {
	curl_multi_perform(multiHandle, &handleCount);
	if (handleCount < pendingRequests.size()) {
		processCompleted();
	}

	if (handleCount != 0) {
		setNewTimeout();
	}
}

void AsyncHttp::processCompleted() {
	int queued;
	while (CURLMsg * m = curl_multi_info_read(multiHandle, &queued)) {
		if (m->msg == CURLMSG_DONE) {
			CurlHandle * hdl = nullptr;
			curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &hdl);
			auto result = std::find_if(pendingRequests.begin(), pendingRequests.end(), [hdl](std::unique_ptr<CurlHandle> const & vhdl) {
				return vhdl.get() == hdl;
			});

			if (result != pendingRequests.end()) {
				auto moved = std::move(*result);
				pendingRequests.erase(result);
				moved->finished(m->data.result);
			}
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

void AsyncHttp::setNewTimeout() {
	long timeout;
	curl_multi_timeout(multiHandle, &timeout);
	if (timeout == -1) {
		startTimer(500);
	} else {
		startTimer(timeout > 100 ? 100 : timeout);
	}
}

void AsyncHttp::timerCallback(uS::Timer * timer) {
	AsyncHttp * const http = static_cast<AsyncHttp *>(timer->getData());
	http->stopTimer();
	http->update();
}
