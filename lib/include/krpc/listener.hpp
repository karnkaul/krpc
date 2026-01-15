#pragma once
#include "krpc/connection.hpp"
#include <chrono>
#include <memory>

using namespace std::chrono_literals;

namespace krpc {
class IListener : public Polymorphic {
  public:
	static constexpr auto timeout_v = 1s;

	[[nodiscard]] virtual auto get_address() const -> Address const& = 0;

	[[nodiscard]] virtual auto accept(std::chrono::milliseconds timeout = timeout_v) const -> Result<std::unique_ptr<IConnection>> = 0;
};

[[nodiscard]] auto create_listener(Address const& address, int backlog) -> Result<std::unique_ptr<IListener>>;
} // namespace krpc
