#include <iostream>
#include <memory>
#include <new>

#include <Server.hpp>

/* Just for the signal handler */
std::unique_ptr<Server> s;

void stopServer() {
	s->stop();
}

void outOfMemoryHandler() {
	std::cerr << "Out of mem! Trying to free some." << std::endl;
	
	if (!s->freeMemory()) {
		std::cerr << "Couldn't free any memory :(" << std::endl;
		throw std::bad_alloc();
	}
}

#ifdef _WIN32
#include <windows.h>

BOOL WINAPI signalHandler(_In_ DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		stopServer();
		Sleep(10000);
		return TRUE;
	default:
		return FALSE;
	}
}

bool installSignalHandler() {
	return SetConsoleCtrlHandler(signalHandler, TRUE) == TRUE;
}

#elif __linux__
#include <csignal>

void signalHandler(int s) {
	stopServer();
}

bool installSignalHandler() {
	return std::signal(SIGINT, signalHandler) != SIG_ERR
		&& std::signal(SIGTERM, signalHandler) != SIG_ERR;
}

#endif

int main(int argc, char * argv[]) {
	std::cout << "Starting server..." << std::endl;

	std::set_new_handler(outOfMemoryHandler);

	if (!installSignalHandler()) {
		std::cerr << "Failed to install signal handler" << std::endl;
	}

	s = std::make_unique<Server>(); // TODO: configurable baseDir

	if (!s->listenAndRun()) {
		return 1;
	}

	std::cout << "Server stopped running, exiting." << std::endl;
	return 0;
}
