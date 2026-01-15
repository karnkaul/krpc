#include "krpc/build_version.hpp"
#include "krpc/library.hpp"
#include "krpc/listener.hpp"
#include "krpc/protocol.hpp"
#include <cstdlib>
#include <exception>
#include <future>
#include <print>

#include <thread>

namespace {
using namespace std::chrono_literals;

auto const server_address = krpc::Address{.host = "localhost", .port = 51234};

class Server {
  public:
	explicit Server() {
		auto result = krpc::create_listener(server_address, 10);
		if (!result) { throw std::runtime_error{"Failed to create listener"}; }
		m_listener = std::move(*result);
		std::println("Server: listening on {}", m_listener->get_address());
	}

	void run_for(std::chrono::milliseconds const duration = 10s) {
		auto const start = std::chrono::steady_clock::now();
		for (auto now = std::chrono::steady_clock::now(); now - start < duration; now = std::chrono::steady_clock::now()) {
			auto result = m_listener->accept();
			if (!result) {
				if (result.error() == krpc::Error::TimedOut) {
					std::println("Server: accept() timeout");
					continue;
				}
				std::println(stderr, "Server: failed to accept peer");
				return;
			}

			auto peer = std::move(*result);
			std::println("Server: connected to peer: {}", peer->get_address());
			auto packet = krpc::protocol::receive_string(*peer);
			if (!packet) {
				std::println(stderr, "failed to receive packet");
				return;
			}

			std::println("Server: received packet:\n  {}", *packet);

			peer.reset();
			std::println("Server: disconnected peer");
			return;
		}
	}

  private:
	std::unique_ptr<krpc::IListener> m_listener{};
};

class Client {
  public:
	explicit Client() {
		std::println("Client: connecting...");
		auto result = krpc::connect_to(server_address);
		if (!result) { throw std::runtime_error{"Failed to connect"}; }
		m_connection = std::move(*result);
		std::println("Client: connected to {}", m_connection->get_address());
	}

	void send_packet() {
		std::println("Client: sending packet");
		if (!krpc::protocol::send_string(*m_connection, "hello world!")) {
			std::println(stderr, "Client: failed to send packet");
			return;
		}
		std::println("Client: packet sent");
	}

  private:
	std::unique_ptr<krpc::IConnection> m_connection{};
};

void run_test() {
	std::println("krpc v{}", krpc::build_version_v);

	auto library = krpc::ILibrary::create();
	if (!library) { throw std::runtime_error{"Initialization failure"}; }

	auto server = Server{};
	auto future = std::async([&] {
		// std::this_thread::sleep_for(5s);
		server.run_for(3s);
	});

	{
		std::this_thread::sleep_for(2s);
		auto client = Client{};
		client.send_packet();
	}
}
} // namespace

int main() {
	try {
		run_test();
	} catch (std::exception const& e) {
		std::println(stderr, "PANIC: {}", e.what());
		return EXIT_FAILURE;
	} catch (...) {
		std::println(stderr, "PANIC!");
		return EXIT_FAILURE;
	}
}
