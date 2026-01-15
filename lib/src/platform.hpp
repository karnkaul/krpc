#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#if defined(__linux__)

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#elif defined(_WIN32)

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#include <Winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#else

#error "Unsupported platform"

#endif

namespace krpc::socket {
#if defined(__linux__)

using Type = int;
using PollFd = pollfd;
constexpr auto invalid_v = Type{-1};
constexpr auto error_v{-1};

inline void net_to_present(int const family, in_addr const& addr, std::string& out) { ::inet_ntop(family, &addr, out.data(), socklen_t(out.size())); }
inline void close(Type const s) { ::close(s); }
[[nodiscard]] inline auto poll(std::span<PollFd> fds, std::chrono::milliseconds const timeout) -> int {
	return ::poll(fds.data(), nfds_t(fds.size()), int(timeout.count()));
}
inline auto send(Type const socket, std::span<std::byte const> bytes) -> std::int64_t { return std::int64_t(::send(socket, bytes.data(), bytes.size(), 0)); }
inline auto receive(Type const socket, std::span<std::byte> buffer) -> std::int64_t { return std::int64_t(::recv(socket, buffer.data(), buffer.size(), 0)); }

#elif defined(_WIN32)

using Type = SOCKET;
using PollFd = WSAPOLLFD;
constexpr auto invalid_v{INVALID_SOCKET};
constexpr auto error_v{SOCKET_ERROR};

inline void net_to_present(int const family, in_addr const& addr, std::string& out) { ::inet_ntop(INT(family), &addr, out.data(), out.size()); }
inline void close(Type const s) { ::closesocket(s); }
[[nodiscard]] inline auto poll(std::span<PollFd> fds, std::chrono::milliseconds const timeout) -> int {
	return ::WSAPoll(fds.data(), ULONG(fds.size()), INT(timeout.count()));
}
inline auto send(Type const socket, std::span<std::byte const> bytes) -> std::int64_t {
	void const* erased = bytes.data();
	return std::int64_t(::send(socket, static_cast<char const*>(erased), int(bytes.size()), 0));
}
inline auto receive(Type const socket, std::span<std::byte> buffer) -> std::int64_t {
	void* erased = buffer.data();
	return std::int64_t(::recv(socket, static_cast<char*>(erased), int(buffer.size()), 0));
}

#endif
} // namespace krpc::socket
