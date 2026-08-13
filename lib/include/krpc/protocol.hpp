#pragma once
#include "krpc/connection.hpp"
#include "krpc/result.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace krpc {
namespace protocol {
struct Header {
	static constexpr std::uint32_t version_v{1};

	std::uint32_t version{version_v};
	std::uint32_t payload_size{};
};
} // namespace protocol

class Protocol {
  public:
	auto send_bytes(IConnection& connection, std::span<std::byte const> packet) -> Result<void>;
	auto send_string(IConnection& connection, std::string_view packet) -> Result<void>;

	auto receive_bytes(IConnection& connection) -> Result<std::span<std::byte const>>;
	auto receive_string(IConnection& connection) -> Result<std::string_view>;

  private:
	std::vector<std::byte> m_buffer{};
};
} // namespace krpc
