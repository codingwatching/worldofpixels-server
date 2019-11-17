#include "ConnectionProcessor.hpp"

#include <HttpData.hpp>

#include <nlohmann/json.hpp>

bool ConnectionProcessor::isAsync(IncomingConnection&) { return false; }

bool ConnectionProcessor::preCheck(IncomingConnection&, HttpData) { return true; }
void ConnectionProcessor::asyncCheck(IncomingConnection&, std::function<void(bool)>) { }
bool ConnectionProcessor::endCheck(IncomingConnection&) { return true; }

void ConnectionProcessor::connected(Client&) { }
void ConnectionProcessor::disconnected(ClosedConnection&) { }

nlohmann::json ConnectionProcessor::getPublicInfo() { return nullptr; }
