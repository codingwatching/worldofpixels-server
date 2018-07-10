#include <iostream>
#include <memory>

#include <Server.hpp>
#include <misc/PropertyReader.hpp>

#include <signal.h>

/* Just for the signal handler */
std::unique_ptr<Server> s;

void stopServer() {
	s->stop();
}

std::string gen_random_str(const sz_t size) {
	static const char alphanum[] =
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz?_";

	std::string str(size, '0');
	for(sz_t i = 0; i < size; ++i){
		str[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}
	return str;
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
	return std::signal(SIGINT, signalHandler) != SIG_ERR;
}

#endif

int main(int argc, char * argv[]) {
	std::cout << "Starting server..." << std::endl;

	srand(time(NULL));

	if (!installSignalHandler()) {
		std::cerr << "Failed to install signal handler" << std::endl;
	}

	PropertyReader pr("props.txt");
	if (!pr.hasProp("modpass")) {
		pr.setProp("modpass", gen_random_str(10));
	}
	if (!pr.hasProp("adminpass")) {
		pr.setProp("adminpass", gen_random_str(10));
	}
	pr.writeToDisk();

	u16 port = std::stoul(pr.getProp("port", "13374"));
	s = std::make_unique<Server>(
		port,
		pr.getProp("modpass"),
		pr.getProp("adminpass"),
		pr.getProp("worldfolder", "chunkdata")
	);

	std::string bind(pr.getProp("bindto"));
	if (!s->listenAndRun(bind)) {
		std::cerr << "Couldn't listen on " << bind << ":" << port << std::endl;
		return 1;
	}

	std::cout << "Server stopped running, exiting." << std::endl;
	return 0;
}
