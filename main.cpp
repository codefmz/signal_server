#include "SignalingServer.h"

#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

namespace {

std::atomic_bool running{true};

void handleSignal(int) {
	running = false;
}

} // namespace

int main() {
	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);

	SignalingServer server;
	if (server.listen("127.0.0.1", 8000)) {
		std::cout << "Listening on 127.0.0.1:" << server.port() << std::endl;
	} else {
		std::cerr << "[Errno 10000] error while attempting to bind on address ('127.0.0.1', 8000)"
		          << std::endl;
		return 1;
	}

	while (running) {
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	server.stop();
	return 0;
}
