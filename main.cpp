#include "server.hpp"
#include <signal.h>
#include "PropertyReader.hpp"

/* Just for the signal handler */
static Server * srvptr;

std::string gen_random_str(const size_t size) {
	static const char alphanum[] =
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz?_";

	std::string str(size, '0');
	for(size_t i = 0; i < size; ++i){
		str[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}
	return str;
}

void handler(int s) {
	std::cout << "Saving worlds and exiting..." << std::endl;
	srvptr->quit();
}

int main(int argc, char * argv[]) {
	std::cout << "Starting server..." << std::endl;
	srand(time(NULL));
	
	PropertyReader pr("props.txt");
	if (!pr.hasProp("modpass")) {
		pr.setProp("modpass", gen_random_str(10));
	}
	if (!pr.hasProp("adminpass")) {
		pr.setProp("adminpass", gen_random_str(10));
	}
	pr.writeToDisk();
	srvptr = new Server(std::stoul(pr.getProp("port", "13374")),
						pr.getProp("modpass"),
						pr.getProp("adminpass"),
						pr.getProp("worldfolder", "chunkdata"));
	
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	
	sigaction(SIGINT, &sigIntHandler, NULL);
	
	srvptr->run();
	delete srvptr;
	
	return 1;
}
