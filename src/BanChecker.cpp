#include "BanChecker.hpp"

#include <ConnectionManager.hpp>
#include <BansManager.hpp>

BanChecker::BanChecker(BansManager& bm)
: bm(bm) { }

bool BanChecker::preCheck(IncomingConnection& ic, uWS::HttpRequest&) {
	return bm.isBanned(ic.ip);
}
