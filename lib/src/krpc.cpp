#include "krpc/address.hpp"
#include "krpc/connection.hpp"
#include "krpc/error.hpp"
#include "krpc/library.hpp"
#include "krpc/listener.hpp"
#include "platform.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <format>
#include <memory>
#include <optional>
#include <span>

namespace krpc {
using namespace std::chrono_literals;

namespace {
struct AddrInfoDeleter {
	void operator()(addrinfo* ptr) const noexcept { ::freeaddrinfo(ptr); }
};

[[nodiscard]] constexpr auto to_sockaddr_in(::sockaddr const& in) -> ::sockaddr_in const* {
	if (in.sa_family != AF_INET) { return nullptr; }
	void const* ptr = &in;
	return static_cast<sockaddr_in const*>(ptr);
}

[[nodiscard]] auto to_address(::sockaddr_in const& addr) -> Address {
	auto ret = Address{.port = int(::ntohs(addr.sin_port))};
	ret.host.resize(INET_ADDRSTRLEN);
	::inet_ntop(AF_INET, &addr.sin_addr, ret.host.data(), ::socklen_t(ret.host.size()));
	return ret;
}

[[nodiscard]] auto to_address(::sockaddr const& addr) -> Address {
	auto const* sockaddr = to_sockaddr_in(addr);
	if (!sockaddr) { return {}; }
	return to_address(*sockaddr);
}

[[nodiscard]] auto get_addr_info(char const* name, char const* service, addrinfo const* hints = nullptr) -> std::unique_ptr<addrinfo, AddrInfoDeleter> {
	auto hints_storage = addrinfo{};
	if (!hints) {
		hints_storage.ai_family = AF_INET;
		hints_storage.ai_socktype = SOCK_STREAM;
		hints = &hints_storage;
	}

	addrinfo* ptr{};
	auto const result = ::getaddrinfo(name, service, hints, &ptr);
	if (result != 0) { throw Error{::gai_strerror(result)}; }
	return std::unique_ptr<addrinfo, AddrInfoDeleter>{ptr};
}

[[nodiscard]] auto get_addr_info(Address const& address, addrinfo const* hints = nullptr) -> std::unique_ptr<addrinfo, AddrInfoDeleter> {
	return get_addr_info(address.host.c_str(), std::format("{}", address.port).c_str(), hints);
}
} // namespace

namespace socket {
namespace {
class Descriptor {
  public:
	Descriptor(Descriptor const&) = delete;
	auto operator=(Descriptor const&) = delete;

	Descriptor() = default;

	explicit Descriptor(addrinfo const& addr) : m_value(::socket(addr.ai_family, addr.ai_socktype, addr.ai_protocol)) {}

	explicit(false) Descriptor(Type const value) : m_value(value) {}

	Descriptor(Descriptor&& rhs) noexcept : Descriptor() { swap(*this, rhs); }

	Descriptor& operator=(Descriptor&& rhs) noexcept {
		if (&rhs != this) { swap(*this, rhs); }
		return *this;
	}

	~Descriptor() noexcept {
		if (m_value == invalid_v) { return; }
		socket::close(m_value);
	}

	[[nodiscard]] auto get_value() const -> Type { return m_value; }

	operator Type() const { return get_value(); }

	friend void swap(Descriptor& lhs, Descriptor& rhs) noexcept { std::swap(lhs.m_value, rhs.m_value); }

  private:
	Type m_value{invalid_v};
};

struct Link {
	Address address{};
	Descriptor descriptor{};
};

auto bind(Type const socket, addrinfo const& addr) -> bool {
	static auto const yes = char{1};
	::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	return ::bind(socket, addr.ai_addr, addr.ai_addrlen) != error_v;
}

auto connect(Type const socket, addrinfo const& addr) -> bool { return ::connect(socket, addr.ai_addr, addr.ai_addrlen) != error_v; }

auto listen(Type const socket, int const backlog) -> bool { return ::listen(socket, backlog) != error_v; }

[[nodiscard]] auto poll(Type const socket, short const events, std::chrono::milliseconds const timeout) -> int {
	auto fds = std::array{
		PollFd{.fd = socket, .events = events, .revents = {}},
	};
	auto const result = socket::poll(fds, timeout);
	if (result <= 0) { return result; }
	return fds.front().revents;
}

[[nodiscard]] auto poll_match(Type const socket, short const events, std::chrono::milliseconds const timeout) -> bool {
	auto const poll_result = poll(socket, events, timeout);
	return poll_result > 0 && (poll_result & events) != 0;
}

[[nodiscard]] auto accept(Type const listener) -> std::optional<Link> {
	auto remote_addr = ::sockaddr_storage{};
	auto remote_addr_size = socklen_t(sizeof(remote_addr));
	void* erased = &remote_addr;
	auto* sock_addr = static_cast<::sockaddr*>(erased);
	auto const descriptor = ::accept(listener, sock_addr, &remote_addr_size);
	if (descriptor == invalid_v) { return {}; }
	return Link{.address = to_address(*sock_addr), .descriptor = descriptor};
}

[[nodiscard]] auto listen(Address const& address, int const backlog) -> std::optional<Link> {
	auto const addr_info = get_addr_info(address);
	for (auto* ptr = addr_info.get(); ptr; ptr = ptr->ai_next) {
		auto descriptor = socket::Descriptor{*addr_info};
		if (descriptor == invalid_v) { continue; }
		if (!socket::bind(descriptor, *ptr)) { continue; }
		if (!socket::listen(descriptor, backlog)) { continue; }
		return Link{.address = to_address(*ptr->ai_addr), .descriptor = std::move(descriptor)};
	}
	return {};
}
} // namespace
} // namespace socket

namespace {
class Connection : public IConnection {
  public:
	explicit Connection(socket::Link link) : m_link(std::move(link)) {}

  private:
	[[nodiscard]] auto get_address() const noexcept -> Address const& final { return m_link.address; }

	auto send(std::span<char const> data) noexcept -> bool final {
		if (!socket::poll_match(m_link.descriptor, POLLOUT, timeout)) { return false; }
		while (!data.empty()) {
			auto const byte_count = ::send(m_link.descriptor, data.data(), data.size(), 0);
			if (byte_count <= 0) { return false; }
			assert(std::size_t(byte_count) <= data.size());
			data = data.subspan(std::size_t(byte_count));
		}
		return true;
	}

	auto receive_once(std::span<char> buffer) noexcept -> std::size_t final {
		if (buffer.empty()) { return 0; }
		if (!socket::poll_match(m_link.descriptor, POLLIN, timeout)) { return 0; }
		auto const ret = ::recv(m_link.descriptor, buffer.data(), buffer.size(), 0);
		if (ret < 0) { return 0; }
		return std::size_t(ret);
	}

	auto receive_exact(std::span<char> buffer) noexcept -> bool final {
		if (buffer.empty()) { return false; }
		if (!socket::poll_match(m_link.descriptor, POLLIN, timeout)) { return false; }
		while (!buffer.empty()) {
			auto const byte_count = ::recv(m_link.descriptor, buffer.data(), buffer.size(), 0);
			if (byte_count <= 0) { return false; }
			buffer = buffer.subspan(std::size_t(byte_count));
		}
		return true;
	}

	socket::Link m_link{};
};

#if defined(_WIN32)
class WsaLib {
  public:
	WsaLib(WsaLib const&) = delete;
	WsaLib(WsaLib&&) = delete;
	WsaLib& operator=(WsaLib const&) = delete;
	WsaLib& operator=(WsaLib&&) = delete;

	explicit WsaLib() {
		if (WSAStartup(MAKEWORD(2, 2), &m_data) != 0) { throw Error{"WSAStartup failed"}; }
	}

	~WsaLib() { WSACleanup(); }

	[[nodiscard]] auto get_data() const -> WSADATA const& { return m_data; }

  private:
	WSADATA m_data{};
};
#endif

class Library : public ILibrary {
  public:
	explicit Library([[maybe_unused]] Options const& options) {
#if defined(_WIN32)
		if (!options.skipWsaStartup) {
			m_lib.emplace();
			if (LOBYTE(m_lib->get_data().wVersion) != 2 || HIBYTE(m_lib->get_data().wVersion) != 2) { throw Error{"Winsock 2.2 not available"}; }
		}
#endif
	}

  private:
	[[nodiscard]] auto connect_to(Address const& address) const -> std::unique_ptr<IConnection> final {
		auto const addr_info = krpc::get_addr_info(address);
		for (auto* ptr = addr_info.get(); ptr; ptr = ptr->ai_next) {
			auto descriptor = socket::Descriptor{*addr_info};
			if (descriptor == socket::invalid_v) { continue; }
			if (!socket::connect(descriptor, *addr_info)) { continue; }
			return std::make_unique<Connection>(socket::Link{.address = to_address(*ptr->ai_addr), .descriptor = std::move(descriptor)});
		}

		throw Error{"Failed to connect"};
	}

	auto listen_on(Address const& address, Listener& listener) const -> bool final {
		auto link = socket::listen(address, listener.get_desired_backlog());
		if (!link) { return false; }

		listener.initialize(link->address);
		while (listener.should_poll()) {
			if (!socket::poll_match(link->descriptor, POLLIN, listener.get_poll_timeout())) { continue; }

			auto peer = socket::accept(link->descriptor);
			if (!peer) { continue; }

			listener.on_accept(std::make_unique<Connection>(std::move(*peer)));
		}
		listener.shutdown();

		return true;
	}

#if defined(_WIN32)
	std::optional<WsaLib> m_lib{};
#endif
};
} // namespace

auto ILibrary::create(Options const& options) -> std::unique_ptr<ILibrary> { return std::make_unique<Library>(options); }
} // namespace krpc
