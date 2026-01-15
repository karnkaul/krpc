#include "krpc/library.hpp"
#include "krpc/listener.hpp"
#include "krpc/protocol.hpp"
#include <future>
#include <print>
#include <stdexcept>

namespace {
auto const server_address = krpc::Address{.host = "localhost", .port = 31245};

[[nodiscard]] auto to_error(std::string_view const message, krpc::Error const code) {
	return std::runtime_error{std::format("{} ({})", message, krpc::to_string_view(code))};
}

class Sender {
  public:
	explicit Sender() {
		auto result = krpc::IConnection::create(server_address);
		if (!result) { throw to_error("Failed to create Sender Connection", result.error()); }
		m_connection = std::move(*result);
		std::println("Sender: connected to {}", m_connection->get_address());
	}

	auto send() -> bool {
		static constexpr auto message = std::string_view{"hello world"};

		auto const result = krpc::protocol::send_string(*m_connection, message);
		if (!result) {
			std::println("Sender: failed to send message ({})", krpc::to_string_view(result.error()));
			return false;
		}

		std::println("Sender: message sent");
		return true;
	}

  private:
	std::unique_ptr<krpc::IConnection> m_connection{};
};

class Receiver {
  public:
	explicit Receiver() {
		auto result = krpc::IListener::create(server_address, 10);
		if (!result) { throw to_error("Failed to create Receiver Listener", result.error()); }
		m_listener = std::move(*result);
		std::println("Receiver: listening on {}", m_listener->get_address());
	}

	auto receive() -> bool {
		auto connection = m_listener->accept();
		if (!connection) {
			std::println("Receiver: failed to accept connection ({})", krpc::to_string_view(connection.error()));
			return false;
		}

		std::println("Receiver: connected to {}", connection.value()->get_address());
		auto message = krpc::protocol::receive_string(*connection.value());
		if (!message) {
			std::println("Receiver: failed to receive message ({})", krpc::to_string_view(message.error()));
			return false;
		}

		std::println("Receiver: received message:\n  {}", *message);
		return true;
	}

  private:
	std::unique_ptr<krpc::IListener> m_listener{};
};

class App {
  public:
	void run() {
		auto result = krpc::ILibrary::create();
		if (!result) { throw to_error("Failed to create Library", result.error()); }
		m_library = std::move(*result);

		auto receiver = Receiver{};
		auto future = std::async([&] { receiver.receive(); });

		auto sender = Sender{};
		sender.send();
	}

  private:
	std::unique_ptr<krpc::ILibrary> m_library{};
};
} // namespace

int main() {
	try {
		App{}.run();
	} catch (std::exception const& e) {
		std::println(stderr, "PANIC: {}", e.what());
		return EXIT_FAILURE;
	} catch (...) {
		std::println(stderr, "PANIC!");
		return EXIT_FAILURE;
	}
}
