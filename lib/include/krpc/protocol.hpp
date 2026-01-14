#pragma once
#include "krpc/connection.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace krpc::protocol {
struct Header {
	static constexpr std::uint32_t version_v{1};

	std::uint32_t version{version_v};
	std::uint32_t payload_size{};
};

enum class Result : std::int8_t {
	Success = 0,
	InvalidArgument,
	InvalidHeader,
	IncompatibleHeader,
	HeaderFailure,
	PacketFailure,
};

auto send_packet(IConnection& connection, std::span<std::byte const> packet) -> Result;
auto send_packet(IConnection& connection, std::string_view packet) -> Result;

auto receive_packet(IConnection& connection, std::vector<std::byte>& out) -> Result;
auto receive_packet(IConnection& connection, std::string& out) -> Result;
} // namespace krpc::protocol
