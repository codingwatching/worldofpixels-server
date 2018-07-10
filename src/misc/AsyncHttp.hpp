#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <curl/curl.h>

namespace uS {
	struct Loop;
	struct Timer;
};

class AsyncHttp {
public:
	class Result {
	public:
		bool const successful;
		CURLcode const errnum;
		long const responseCode;
		std::string const data;
		const char * const errorString;

		Result(Result &&);
		Result(CURLcode, long, std::string &&, const char *);
	};

private:
	class CurlHandle {
		std::function<void(AsyncHttp::Result)> onFinished;
		CURLM * multiHandle;
		CURL * easyHandle;
		bool addedToMulti;
		std::string buffer;

	public:
		CurlHandle(CurlHandle const &) = delete;
		CurlHandle(CurlHandle &&);
		CurlHandle(CURLM *, std::string, std::unordered_map<std::string, std::string>,
				std::function<void(AsyncHttp::Result)>);
		~CurlHandle();

		void addToMulti();
		void rmFromMulti();
		CURL * getHandle();
		bool finished(CURLcode result); /* Returns false if error occurred */

	private:
		static int writer(char *, std::size_t, std::size_t, std::string *);
	};

	uS::Timer * timer;
	std::vector<std::unique_ptr<CurlHandle>> pendingRequests;
	bool isTimerRunning;
	CURLM * multiHandle;
	int handleCount;

public:
	AsyncHttp(AsyncHttp const &) = delete;
	AsyncHttp(AsyncHttp &&);
	AsyncHttp(uS::Loop *);
	~AsyncHttp();

	int activeHandles();

	void addRequest(std::string url, std::unordered_map<std::string, std::string> params,
		std::function<void(AsyncHttp::Result)>);
	void addRequest(std::string url, std::function<void(AsyncHttp::Result)>);
	/* No cancelRequest(), see: https://github.com/curl/curl/issues/2101 */

private:
	void update();
	void processCompleted();
	void startTimer(long);
	void stopTimer();
	void setNewTimeout();
	static void timerCallback(uS::Timer *);
};
