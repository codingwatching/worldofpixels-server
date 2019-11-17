#include "BanChecker.hpp"

#include <ConnectionManager.hpp>
#include <BansManager.hpp>
#include <HttpData.hpp>

BanChecker::BanChecker(BansManager& bm)
: bm(bm) { }

bool BanChecker::preCheck(IncomingConnection& ic, HttpData) {
	return !bm.isBanned(ic.ip);
}
