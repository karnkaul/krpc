#pragma once
#include <cstdint>
#include <string_view>

namespace krpc {
enum class Error : std::int8_t {
	InvalidArgument,
	InitializationFailure,
	Disconnected,
	TimedOut,
	SocketFailure,
	InvalidHeader,
	IncompatibleHeader,
	PacketTooLarge,
};

constexpr auto to_string_view(Error const error) -> std::string_view {
	switch (error) {
	case Error::InvalidArgument: return "InvalidArgument";
	case Error::InitializationFailure: return "InitializationFailure";
	case Error::Disconnected: return "Disconnected";
	case Error::TimedOut: return "TimedOut";
	case Error::SocketFailure: return "SocketFailure";
	case Error::IncompatibleHeader: return "IncompatibleHeader";
	case Error::PacketTooLarge: return "PacketTooLarge";
	default: return "Unknown";
	}
}
} // namespace krpc
