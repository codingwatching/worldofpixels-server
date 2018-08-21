#include "ApiProcessor.hpp"

#include <uWS.h>
#include <misc/utils.hpp>

ApiProcessor::ApiProcessor(uWS::Hub& h, std::string defaultRequest) {
	// one connection can request multiple things before it closes
	h.onHttpRequest([this, dr{std::move(defaultRequest)}](uWS::HttpResponse * res, uWS::HttpRequest req, char * data, sz_t len, sz_t rem) {
		RequestStorage * rs = static_cast<RequestStorage*>(res->getHttpSocket()->getUserData());
		if (!rs) {
			rs = new RequestStorage;
			res->getHttpSocket()->setUserData(rs);
		} else {
			rs->onCancel = nullptr; // I'm gonna get mad if requests can be made on the same socket before the last one completes
		}

		auto args(tokenize(req.getUrl().toString(), '/', true));

		if (args.size() == 0) {
			args.emplace_back(dr);
		}

		if (auto status = exec(res, *rs, std::move(args))) {
			res->end("\"Unknown request\"", 17);
		}
	});

	h.onCancelledHttpRequest([](uWS::HttpResponse * res) {
		RequestStorage * rs = static_cast<RequestStorage*>(res->getHttpSocket()->getUserData());
		if (rs && rs->onCancel) {
			rs->onCancel();
		}

		delete rs;
		res->getHttpSocket()->setUserData(nullptr);
	});

	h.onHttpDisconnection([](uWS::HttpSocket<uWS::SERVER> * s) {
		// this library is retarded so the disconnection handler is called
		// before the cancelled requests handler, so i need to do hacky things
		// if i don't want to use freed memory (there's no request completion handler), cool!
		RequestStorage * rs = static_cast<RequestStorage*>(s->getUserData());
		if (!s->outstandingResponsesHead) {
			// seems like no cancelled request handler will be called
			delete rs; // note: deleting null is ok
		}
	});
}

void ApiProcessor::set(std::string name, ApiProcessor::Func f) {
	methods[name] = std::move(f);
}

ApiProcessor::Status ApiProcessor::exec(uWS::HttpResponse * r, RequestStorage& rs, ApiProcessor::ArgList args) {
	if (args.size() == 0) return UNK_REQ;

	auto s = methods.find(args[0]);
	if (s != methods.end()) {
		return s->second(r, rs, std::move(args));
	}

	return UNK_REQ;
}
