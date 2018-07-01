#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <curl/curl.h>

class AsyncHTTPGETClient {
public:
	class Request {
	public:
		const std::string url;
		const std::string postFields;
		const std::function<void(CURL * const, const CURLcode, const std::string &)> callback;
		
		Request(const std::string & url, const std::string & postFields,
		        const std::function<void(CURL * const, const CURLcode, const std::string &)> callback);
	};
	
private:
	CURL * const curl;
	std::queue<Request> requestQueue;
	std::mutex queueLock;
	std::mutex waitLock;
	std::condition_variable cv;
	std::atomic<bool> terminateThread;
	std::thread requestThread;
	
public:
	AsyncHTTPGETClient();
	~AsyncHTTPGETClient();
	
	void removeRequests(const std::string & url);
	void queueRequest(const Request);
	
	CURL * const getCurl();
	
private:
	void processRequests();
};
