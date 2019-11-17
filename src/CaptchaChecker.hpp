#pragma once

#include "ConnectionProcessor.hpp"

class RecaptchaRestApi;

class CaptchaChecker : public ConnectionProcessor {
public:
	enum class State { ALL, GUESTS, OFF };

private:
	RecaptchaRestApi& rcra;
	State state;

public:
	CaptchaChecker(RecaptchaRestApi&);

	void setState(State);

	bool isCaptchaEnabledFor(IncomingConnection&);

	bool isAsync(IncomingConnection&);

	bool preCheck(IncomingConnection&, HttpData);
	void asyncCheck(IncomingConnection&, std::function<void(bool)>);

	nlohmann::json getPublicInfo();
};
