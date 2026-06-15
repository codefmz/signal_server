#ifndef SIGNALINGSERVER_H
#define SIGNALINGSERVER_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <unordered_map>

#include <rtc/rtc.hpp>

class SignalingServer {
public:
	SignalingServer();
	~SignalingServer();

	bool listen(const std::string &address = "0.0.0.0", uint16_t port = 0);
	void stop();
	uint16_t port() const;

private:
	void onClient(std::shared_ptr<rtc::WebSocket> client);
	void onDisconnected(std::string clientId, const std::shared_ptr<rtc::WebSocket> &client);
	void onMessage(const std::string &clientId, const std::shared_ptr<rtc::WebSocket> &client,
	               rtc::message_variant message);

	static std::string clientIdFromPath(const std::shared_ptr<rtc::WebSocket> &client);

private:
	std::shared_ptr<rtc::WebSocketServer> server;
	std::unordered_map<std::string, std::shared_ptr<rtc::WebSocket>> clients;
	std::unordered_set<std::shared_ptr<rtc::WebSocket>> pendingClients;
	mutable std::mutex clientsMutex;
};

#endif // SIGNALINGSERVER_H
