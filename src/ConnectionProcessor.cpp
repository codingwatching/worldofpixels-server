#include "ConnectionProcessor.hpp"

bool ConnectionProcessor::isAsync(IncomingConnection&) { return false; }

bool ConnectionProcessor::preCheck(IncomingConnection&) { return true; }
void ConnectionProcessor::asyncCheck(IncomingConnection&, std::function<void(bool)>) { }
bool ConnectionProcessor::endCheck(IncomingConnection&) { return true; }

void ConnectionProcessor::connected(Client&) { }
void ConnectionProcessor::disconnected(ClosedConnection&) { }

nlohmann::json ConnectionProcessor::getPublicInfo() { return nullptr; }
