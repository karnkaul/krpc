#include "krpc/build_version.hpp"
#include "krpc/library.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <future>
#include <optional>
#include <print>

#include <thread>

namespace {
using namespace std::chrono_literals;

struct Header {
	std::uint32_t version{1};
	std::uint32_t payload_size{};
};

[[nodiscard]] auto to_header(std::string_view const in) -> std::optional<Header> {
	if (in.size() != sizeof(Header)) { return {}; }
	auto ret = Header{};
	std::memcpy(&ret, in.data(), in.size());
	return ret;
}

[[nodiscard]] auto to_str(Header const& header) -> std::array<char, sizeof(Header)> {
	auto ret = std::array<char, sizeof(Header)>{};
	std::memcpy(ret.data(), &header, sizeof(header));
	return ret;
}

struct Packet {
	auto send_to(krpc::IConnection& connection) const -> bool {
		if (message.empty()) { return false; }

		auto const header = to_str(Header{.payload_size = std::uint32_t(message.size())});
		if (!connection.send(std::as_bytes(std::span{header}))) {
			std::println(stderr, "Failed to send header");
			return false;
		}

		if (!connection.send(std::as_bytes(std::span{message}))) {
			std::println(stderr, "Failed to send message");
			return false;
		}

		return true;
	}

	auto receive_from(krpc::IConnection& connection) -> bool {
		message.clear();

		auto buffer = std::string{};
		buffer.resize(sizeof(Header));
		if (!connection.receive_exact(std::as_writable_bytes(std::span{buffer}))) {
			std::println(stderr, "Failed to receive header");
			return false;
		}

		auto const header = to_header(buffer);
		if (!header) {
			std::println(stderr, "Invalid header");
			return false;
		}

		if (header->payload_size == 0) { return true; }

		buffer.resize(std::size_t(header->payload_size));
		if (!connection.receive_exact(std::as_writable_bytes(std::span{buffer}))) {
			std::println(stderr, "Failed to receive message");
			return false;
		}

		message = std::string{buffer.c_str()};
		return true;
	}

	std::string message{};
};

auto const server_address = krpc::Address{.host = "localhost", .port = 51234};

class Server : public krpc::Listener {
	[[nodiscard]] auto should_poll() const -> bool final { return m_should_poll; }

	void initialize(krpc::Address const& listener_address) final { std::println("Server: listening on {}", listener_address); }

	void on_accept(std::unique_ptr<krpc::IConnection> connection) final {
		std::println("Server: connected to peer: {}", connection->get_address());

		auto packet = Packet{};
		if (!packet.receive_from(*connection)) { return; }
		std::println("Server: received packet:\n  {}", packet.message);

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
		auto packet = Packet{.message = "hello world!"};
		packet.send_to(*m_connection);
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
