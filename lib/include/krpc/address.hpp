#pragma once
#include <format>
#include <string>

namespace krpc {
struct Address {
	std::string host{};
	int port{};
};
} // namespace krpc

template <>
struct std::formatter<krpc::Address> {
	template <typename FormatContext>
	static constexpr auto parse(FormatContext& fc) {
		return fc.begin();
	}

	static auto format(krpc::Address const& address, format_context& fc) -> format_context::iterator {
		return format_to(fc.out(), "{}:{}", address.host, address.port);
	}
};
