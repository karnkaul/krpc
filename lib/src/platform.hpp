#pragma once
#include <chrono>
#include <span>

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

inline void close(Type const s) { ::close(s); }
[[nodiscard]] inline auto poll(std::span<PollFd> fds, std::chrono::milliseconds const timeout) -> int {
	return ::poll(fds.data(), nfds_t(fds.size()), int(timeout.count()));
}

#elif defined(_WIN32)

using Type = SOCKET;
using PollFd = WSAPOLLFD;
constexpr auto invalid_v{INVALID_SOCKET};
constexpr auto error_v{SOCKET_ERROR};

inline void close(Type const s) { ::closesocket(s); }
[[nodiscard]] inline auto poll(std::span<PollFd> fds, std::chrono::milliseconds const timeout) -> int {
	return ::WSAPoll(fds.data(), ULONG(fds.size()), INT(timeout.count()));
}

#endif
} // namespace krpc::socket
