#include "krpc/address.hpp"
#include "krpc/connection.hpp"
#include "krpc/error.hpp"
#include "krpc/library.hpp"
#include "krpc/listener.hpp"
#include "krpc/protocol.hpp"
#include "platform.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <format>
#include <memory>
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
	socket::net_to_present(AF_INET, addr.sin_addr, ret.host);
	return ret;
}

[[nodiscard]] auto to_address(::sockaddr const& addr) -> Result<Address> {
	auto const* sockaddr = to_sockaddr_in(addr);
	if (!sockaddr) { return std::unexpected{Error::InvalidArgument}; }
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
	if (result != 0) { return {}; }
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

	~Descriptor() {
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

[[nodiscard]] constexpr auto poll_result(int const value) -> Result<void> {
	if (value < 0) { return std::unexpected{Error::SocketFailure}; }
	if (value == 0) { return std::unexpected{Error::TimedOut}; }
	return {};
}

[[nodiscard]] constexpr auto io_result(std::int64_t const value) -> Result<void> {
	if (value == 0) { return std::unexpected{Error::Disconnected}; }
	if (value < 0) { return std::unexpected{Error::SocketFailure}; }
	return {};
}

auto bind(Type const socket, addrinfo const& addr) -> bool {
	static auto const yes = char{1};
	::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	return ::bind(socket, addr.ai_addr, socklen_t(addr.ai_addrlen)) != error_v;
}

auto connect(Type const socket, addrinfo const& addr) -> bool { return ::connect(socket, addr.ai_addr, socklen_t(addr.ai_addrlen)) != error_v; }

auto listen(Type const socket, int const backlog) -> bool { return ::listen(socket, backlog) != error_v; }

[[nodiscard]] auto poll_single(Type const socket, std::chrono::milliseconds const timeout, short const events) -> Result<void> {
	auto fds = std::array{
		PollFd{.fd = socket, .events = events, .revents = {}},
	};
	auto const value = socket::poll(fds, timeout);
	if (auto const result = socket::poll_result(value); !result) { return std::unexpected{result.error()}; }

	auto const revents = fds.front().revents;
	if ((revents & POLLHUP) != 0) { return std::unexpected{Error::Disconnected}; }
	if ((revents & events) == 0) { return std::unexpected{Error::SocketFailure}; }

	return {};
}

[[nodiscard]] auto accept(Type const listener) -> Result<Link> {
	auto remote_addr = ::sockaddr_storage{};
	auto remote_addr_size = socklen_t(sizeof(remote_addr));
	void* erased = &remote_addr;
	auto* sock_addr = static_cast<::sockaddr*>(erased);
	auto const descriptor = ::accept(listener, sock_addr, &remote_addr_size);
	if (descriptor == invalid_v) { return {}; }
	return to_address(*sock_addr).transform([&descriptor](Address address) { return Link{.address = std::move(address), .descriptor = descriptor}; });
}

[[nodiscard]] auto listen(addrinfo const& addr, int const backlog) -> Result<Link> {
	for (auto const* ptr = &addr; ptr; ptr = ptr->ai_next) {
		auto descriptor = socket::Descriptor{*ptr};
		if (descriptor == invalid_v) { continue; }
		if (!socket::bind(descriptor, *ptr)) { continue; }
		if (!socket::listen(descriptor, backlog)) { continue; }
		auto address = to_address(*ptr->ai_addr).value_or(Address{});
		return Link{.address = std::move(address), .descriptor = std::move(descriptor)};
	}
	return std::unexpected{Error::SocketFailure};
}
} // namespace
} // namespace socket

namespace {
class Connection : public IConnection {
  public:
	explicit Connection(socket::Link link) : m_link(std::move(link)) {}

  private:
	[[nodiscard]] auto get_address() const -> Address const& final { return m_link.address; }

	auto send(std::span<std::byte const> data) -> Result<void> final {
		if (data.empty()) { return std::unexpected{Error::InvalidArgument}; }

		static constexpr auto events_v = POLLOUT;
		auto result = socket::poll_single(m_link.descriptor, timeout, events_v);
		if (!result) { return std::unexpected{result.error()}; }

		while (!data.empty()) {
			auto const byte_count = socket::send(m_link.descriptor, data);
			if (auto result = socket::io_result(byte_count); !result) { return std::unexpected{result.error()}; }
			assert(std::size_t(byte_count) <= data.size());
			data = data.subspan(std::size_t(byte_count));
		}

		return {};
	}

	auto receive(std::span<std::byte> buffer) -> Result<void> final {
		if (buffer.empty()) { return std::unexpected{Error::InvalidArgument}; }

		static constexpr auto events_v = POLLIN;
		auto result = socket::poll_single(m_link.descriptor, timeout, events_v);
		if (!result) { return std::unexpected{result.error()}; }

		while (!buffer.empty()) {
			auto const byte_count = socket::receive(m_link.descriptor, buffer);
			if (auto result = socket::io_result(byte_count); !result) { return std::unexpected{result.error()}; }
			buffer = buffer.subspan(std::size_t(byte_count));
		}

		return {};
	}

	socket::Link m_link{};
};

class Listener : public IListener {
  public:
	explicit Listener(socket::Link link) : m_link(std::move(link)) {}

  private:
	[[nodiscard]] auto get_address() const -> Address const& final { return m_link.address; }

	[[nodiscard]] auto accept(std::chrono::milliseconds const timeout) const -> Result<std::unique_ptr<IConnection>> final {
		static constexpr auto events_v = POLLIN;
		return socket::poll_single(m_link.descriptor, timeout, events_v)
			.and_then([this] { return socket::accept(m_link.descriptor); })
			.transform([](socket::Link link) { return std::make_unique<Connection>(std::move(link)); });
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
		if (WSAStartup(MAKEWORD(2, 2), &m_data) != 0) { throw std::runtime_error{"WSAStartup failed"}; }
	}

	~WsaLib() { WSACleanup(); }

	[[nodiscard]] auto get_data() const -> WSADATA const& { return m_data; }

  private:
	WSADATA m_data{};
};
#endif

class Library : public ILibrary {
  public:
#if defined(_WIN32)
	explicit Library() {
		if (LOBYTE(m_lib.get_data().wVersion) != 2 || HIBYTE(m_lib.get_data().wVersion) != 2) { throw std::runtime_error{"Winsock 2.2 not available"}; }
	}
#else
	explicit Library() = default;
#endif

  private:
#if defined(_WIN32)
	WsaLib m_lib{};
#endif
};
} // namespace

auto ILibrary::create() -> Result<std::unique_ptr<ILibrary>> {
	try {
		return std::make_unique<Library>();
	} catch (std::exception const& /*e*/) { return std::unexpected{Error::InitializationFailure}; }
}

auto IConnection::create(Address const& address) -> Result<std::unique_ptr<IConnection>> {
	auto const addr_info = get_addr_info(address);
	if (!addr_info) { return std::unexpected{Error::InvalidArgument}; }

	for (auto* ptr = addr_info.get(); ptr; ptr = ptr->ai_next) {
		auto descriptor = socket::Descriptor{*addr_info};
		if (descriptor == socket::invalid_v) { continue; }
		if (!socket::connect(descriptor, *addr_info)) { continue; }
		return std::make_unique<Connection>(socket::Link{.address = to_address(*ptr->ai_addr).value_or(address), .descriptor = std::move(descriptor)});
	}

	return std::unexpected{Error::SocketFailure};
}

auto IListener::create(Address const& address, int const backlog) -> Result<std::unique_ptr<IListener>> {
	auto const addr_info = get_addr_info(address);
	if (!addr_info) { return std::unexpected{Error::InvalidArgument}; }
	return socket::listen(*addr_info, backlog).transform([](socket::Link link) { return std::make_unique<Listener>(std::move(link)); });
}

namespace protocol {
namespace {
[[nodiscard]] auto to_header(std::span<std::byte const> in) -> Result<Header> {
	if (in.size() != sizeof(Header)) { return std::unexpected{Error::InvalidHeader}; }
	auto ret = Header{};
	std::memcpy(&ret, in.data(), in.size());
	if (ret.payload_size == 0) { return std::unexpected{Error::InvalidHeader}; }
	if (ret.version != Header::version_v) { return std::unexpected{Error::IncompatibleHeader}; }
	return ret;
}

[[nodiscard]] auto to_bytes(Header const& header) -> std::array<std::byte, sizeof(Header)> {
	auto ret = std::array<std::byte, sizeof(Header)>{};
	std::memcpy(ret.data(), &header, sizeof(header));
	return ret;
}

template <typename ContainerT>
auto receive(IConnection& connection) -> Result<ContainerT> {
	auto ret = ContainerT{};
	ret.resize(sizeof(Header));
	auto bytes = std::as_writable_bytes(std::span{ret});
	return connection.receive(bytes)
		.and_then([bytes] { return to_header(bytes); })
		.and_then([&ret, &connection](Header const& header) {
			ret.resize(std::size_t(header.payload_size));
			return connection.receive(std::as_writable_bytes(std::span{ret}));
		})
		.transform([&ret] { return std::move(ret); });
}
} // namespace
} // namespace protocol

auto protocol::send_bytes(IConnection& connection, std::span<std::byte const> packet) -> Result<void> {
	if (packet.empty()) { return std::unexpected{Error::InvalidArgument}; }

	auto const header = Header{.payload_size = std::uint32_t(packet.size())};
	return connection.send(to_bytes(header)).and_then([&] { return connection.send(packet); });
}

auto protocol::send_string(IConnection& connection, std::string_view const packet) -> Result<void> {
	return send_bytes(connection, std::as_bytes(std::span{packet}));
}

auto protocol::receive_bytes(IConnection& connection) -> Result<std::vector<std::byte>> { return receive<std::vector<std::byte>>(connection); }

auto protocol::receive_string(IConnection& connection) -> Result<std::string> { return receive<std::string>(connection); }

} // namespace krpc
