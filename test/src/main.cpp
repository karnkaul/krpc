#include "krpc/build_version.hpp"
#include "krpc/library.hpp"
#include "krpc/protocol.hpp"
#include <cstdlib>
#include <exception>
#include <future>
#include <print>

#include <thread>

namespace {
using namespace std::chrono_literals;

auto const server_address = krpc::Address{.host = "localhost", .port = 51234};

class Server : public krpc::Listener {
	[[nodiscard]] auto should_poll() const -> bool final { return m_should_poll; }

	void initialize(krpc::Address const& listener_address) final { std::println("Server: listening on {}", listener_address); }

	void on_accept(std::unique_ptr<krpc::IConnection> connection) final {
		std::println("Server: connected to peer: {}", connection->get_address());

		auto packet = std::string{};
		if (krpc::protocol::receive_packet(*connection, packet) != krpc::protocol::Result::Success) { return; }
		std::println("Server: received packet:\n  {}", packet);

		m_should_poll = false;
	}

	void shutdown() final { std::println("Server: shutting down"); }

	bool m_should_poll{true};
};

class Client {
  public:
	explicit Client(krpc::ILibrary& library) : m_connection(library.connect_to(server_address)) {
		std::println("Client: connected to {}", m_connection->get_address());
	}

	void send_packet() {
		auto packet = std::string_view{"hello world!"};
		if (krpc::protocol::send_packet(*m_connection, packet) != krpc::protocol::Result::Success) {
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

	auto server = Server{};
	auto future = std::async([&] {
		if (!library->listen_on(server_address, server)) { std::println(stderr, "Server: failed to listen"); }
	});

	{
		std::this_thread::sleep_for(20ms);
		auto client = Client{*library};
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
