#pragma once

#include "ConnectionProcessor.hpp"

#include <string>
#include <vector>

class HeaderChecker : public ConnectionProcessor {
	std::vector<std::string> acceptedOrigins;

public:
	HeaderChecker(std::vector<std::string>);
	bool preCheck(IncomingConnection&, uWS::HttpRequest&);
};
