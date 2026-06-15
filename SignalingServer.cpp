#include "SignalingServer.h"

#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <utility>

namespace {

template <class T>
std::weak_ptr<T> makeWeakPtr(const std::shared_ptr<T> &ptr) {
    return ptr;
}

} // namespace

SignalingServer::SignalingServer() = default;

SignalingServer::~SignalingServer() {
    stop();
}

bool SignalingServer::listen(const std::string &address, uint16_t port) {
    try {
        rtc::WebSocketServer::Configuration config;
        config.port = port;
        config.bindAddress = address;
        config.enableTls = false;
        config.maxMessageSize = 1024 * 1024;

        server = std::make_shared<rtc::WebSocketServer>(std::move(config));
        server->onClient([this](std::shared_ptr<rtc::WebSocket> client) {
            onClient(std::move(client));
        });

        return true;
    } catch (const std::exception &error) {
        std::cerr << "Failed to start signaling server on " << address << ":" << port << ": "
                    << error.what() << std::endl;
        server.reset();
        return false;
    }
}

void SignalingServer::stop() {
    if (server) {
        server->stop();
        server.reset();
    }

    std::lock_guard<std::mutex> lock(clientsMutex);
    clients.clear();
    pendingClients.clear();
}

uint16_t SignalingServer::port() const {
    return server ? server->port() : 0;
}

void SignalingServer::onClient(std::shared_ptr<rtc::WebSocket> client) {
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        pendingClients.insert(client);
    }

    std::cout << "Client connected";
    if (const auto address = client->remoteAddress()) {
        std::cout << " from " << *address;
    }
    std::cout << std::endl;

    const auto weakClient = makeWeakPtr(client);
    const auto clientId = std::make_shared<std::string>();

    client->onOpen([this, clientId, weakClient]() {
        const auto currentClient = weakClient.lock();
        if (!currentClient) {
            return;
        }

        *clientId = clientIdFromPath(currentClient);
        if (clientId->empty()) {
            std::cerr << "Rejecting client without id in path" << std::endl;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                pendingClients.erase(currentClient);
            }
            currentClient->close();
            return;
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            pendingClients.erase(currentClient);
            clients[*clientId] = currentClient;
        }

        std::cout << "Client " << *clientId << " connection open" << std::endl;
    });

    client->onClosed([this, clientId, weakClient]() {
        const auto currentClient = weakClient.lock();
        if (!currentClient) {
            return;
        }
        onDisconnected(*clientId, currentClient);
    });

    client->onError([clientId](std::string error) {
        std::cerr << "Client " << (clientId->empty() ? "<pending>" : *clientId)
                  << " error: " << error << std::endl;
    });

    client->onMessage([this, clientId, weakClient](rtc::message_variant message) {
        const auto currentClient = weakClient.lock();
        if (!currentClient || clientId->empty()) {
            return;
        }
        onMessage(*clientId, currentClient, std::move(message));
    });
}

void SignalingServer::onDisconnected(std::string clientId,
                                     const std::shared_ptr<rtc::WebSocket> &client) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    pendingClients.erase(client);
    if (!clientId.empty()) {
        const auto it = clients.find(clientId);
        if (it != clients.end() && it->second == client) {
            clients.erase(it);
        }
    }
    std::cout << "Client " << (clientId.empty() ? "<pending>" : clientId)
              << " disconnected" << std::endl;
}

void SignalingServer::onMessage(const std::string &clientId,
                                const std::shared_ptr<rtc::WebSocket> &client,
                                rtc::message_variant message) {
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        const auto it = clients.find(clientId);
        if (it == clients.end() || it->second != client) {
            return;
        }
    }

    if (std::holds_alternative<rtc::binary>(message)) {
        std::cout << "Client " << clientId << " sent binary message ("
                    << std::get<rtc::binary>(message).size() << " bytes)" << std::endl;
        return;
    }

    const auto &text = std::get<std::string>(message);
    std::cout << "Client " << clientId << " << " << text << std::endl;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(text);
    } catch (const nlohmann::json::parse_error &error) {
        std::cerr << "Client " << clientId << " sent invalid JSON: " << error.what() << std::endl;
        return;
    }

    const auto destination = payload.find("id");
    if (destination == payload.end() || !destination->is_string()) {
        std::cerr << "Client " << clientId << " message missing string field \"id\"" << std::endl;
        return;
    }

    const auto destinationId = destination->get<std::string>();
    std::shared_ptr<rtc::WebSocket> destinationClient;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        const auto it = clients.find(destinationId);
        if (it != clients.end()) {
            destinationClient = it->second;
        }
    }

    if (!destinationClient) {
        std::cout << "Client " << destinationId << " not found" << std::endl;
        return;
    }

    payload["id"] = clientId;
    const auto data = payload.dump();
    std::cout << "Client " << destinationId << " >> " << data << std::endl;
    destinationClient->send(data);
}

std::string SignalingServer::clientIdFromPath(const std::shared_ptr<rtc::WebSocket> &client) {
	const auto path = client->path();
	if (!path || path->empty()) {
        std::cerr << "Client has empty path" << std::endl;
		return {};
	}

	const auto first = path->find_first_not_of('/');
	if (first == std::string::npos) {
		return {};
	}

	const auto last = path->find('/', first);
	return path->substr(first, last == std::string::npos ? std::string::npos : last - first);
}
