#include "AsyncHTTPGETClient.hpp"

#include <stdexcept>

/* Error message, and received data */
static char errorBuffer[CURL_ERROR_SIZE];
static std::string buffer;

/* Writer function for libCURL */
static int writer(char * data, std::size_t size, std::size_t nmemb,
                  std::string * writerData) {
	if (writerData == nullptr) {
		return 0;
	}
	
	writerData->append(data, size * nmemb);
	
	return size * nmemb;
}

AsyncHTTPGETClient::Request::Request(const std::string & url, const std::string & postFields,
                                     const std::function<void(CURL * const, const CURLcode, const std::string &)> callback)
: url(url),
  postFields(postFields),
  callback(callback) { }

AsyncHTTPGETClient::AsyncHTTPGETClient()
: curl(curl_easy_init()),
  terminateThread(false),
  requestThread([this] { processRequests(); }) {
	CURLcode code;
	if (curl == nullptr) {
		throw std::runtime_error("Couldn't initialize libCURL!");
	}
	
	if (curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer) != CURLE_OK) {
		throw std::runtime_error("Couldn't set the libCURL error buffer!");
	}
	
	code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
	if (code != CURLE_OK) {
		throw std::runtime_error("Couldn't set the libCURL writer function! " + std::string(errorBuffer));
	}
	
	code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	if (code != CURLE_OK) {
		throw std::runtime_error("Couldn't set the libCURL write buffer! " + std::string(errorBuffer));
	}
	
	//curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 4);
}

AsyncHTTPGETClient::~AsyncHTTPGETClient() {
	terminateThread = true;
	cv.notify_all();
	requestThread.join();
	if (curl) {
		curl_easy_cleanup(curl);
	}
}

void AsyncHTTPGETClient::removeRequests(const std::string & url) {
	/* Iterating queues isn't the best thing one can do */
	queueLock.lock();
	size_t queueSize = requestQueue.size();
	while (queueSize--) {
		Request req = requestQueue.front();
		requestQueue.pop();
		if (req.url != url) {
			requestQueue.push(req);
		}
	}
	queueLock.unlock();
}

void AsyncHTTPGETClient::queueRequest(const AsyncHTTPGETClient::Request req) {
	std::unique_lock<std::mutex> lck(waitLock);
	queueLock.lock();
	requestQueue.push(req);
	queueLock.unlock();
	cv.notify_all();
}

void AsyncHTTPGETClient::processRequests() {
	while (!terminateThread) {
		std::unique_lock<std::mutex> lck(waitLock);
		queueLock.lock(); /* Seems like this needs too much (un)locking for my tastes */
		while (requestQueue.size() != 0) {
			CURLcode code;
			
			Request req = requestQueue.front();
			requestQueue.pop();
			
			queueLock.unlock();
			
			std::string url = req.url;
			if (req.postFields.size() > 0) {
				url += "?" + req.postFields;
			}
			
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			
			code = curl_easy_perform(curl);
			
			/* NOTE: This is called from the request thread. */
			req.callback(curl, code, buffer);
			buffer.clear();
			
			queueLock.lock();
		}
		
		queueLock.unlock();
		
		cv.wait(lck);
	}
}

CURL * const AsyncHTTPGETClient::getCurl() {
	return curl;
}
