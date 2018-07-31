#include "ApiProcessor.hpp"

#include <uWS.h>

ApiProcessor::ApiProcessor() { }

void ApiProcessor::set(std::string name, ApiProcessor::Func f) {
	methods[name] = std::move(f);
}

ApiProcessor::Status ApiProcessor::exec(uWS::HttpResponse * r, ApiProcessor::ArgList args) {
	if (args.size() == 0) return UNK_REQ;

	auto s = methods.find(args[0]);
	if (s != methods.end()) {
		return s->second(r, std::move(args));
	}

	return UNK_REQ;
}
