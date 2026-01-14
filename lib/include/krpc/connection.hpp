#pragma once
#include "krpc/address.hpp"
#include "krpc/polymorphic.hpp"
#include <chrono>
#include <cstddef>
#include <span>

using namespace std::chrono_literals;

namespace krpc {
class IConnection : public Polymorphic {
  public:
	static constexpr auto timeout_v = 1s;

	[[nodiscard]] virtual auto get_address() const noexcept -> Address const& = 0;

	virtual auto send(std::span<std::byte const> data) noexcept -> bool = 0;

	virtual auto receive_once(std::span<std::byte> buffer) noexcept -> std::size_t = 0;
	virtual auto receive_exact(std::span<std::byte> buffer) noexcept -> bool = 0;

	std::chrono::milliseconds timeout{timeout_v};
};
} // namespace krpc
