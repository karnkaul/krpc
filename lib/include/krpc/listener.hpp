#pragma once
#include "krpc/connection.hpp"
#include <chrono>
#include <memory>

using namespace std::chrono_literals;

namespace krpc {
class IListener : public Polymorphic {
  public:
	static constexpr auto timeout_v = 1s;

	/// \param address Address to listen on.
	/// \param backlog Socket backlog.
	/// \returns Concrete instance on success.
	[[nodiscard]] static auto create(Address const& address, int backlog) -> Result<std::unique_ptr<IListener>>;

	/// \returns Address of this instance.
	[[nodiscard]] virtual auto get_address() const -> Address const& = 0;

	/// \param timeout Poll timeout.
	/// \returns Concrete instance on success.
	[[nodiscard]] virtual auto accept(std::chrono::milliseconds timeout = timeout_v) const -> Result<std::unique_ptr<IConnection>> = 0;
};
} // namespace krpc
