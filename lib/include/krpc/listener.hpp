#pragma once
#include "krpc/connection.hpp"
#include <chrono>

using namespace std::chrono_literals;

namespace krpc {
class Listener : public Polymorphic {
  public:
	static constexpr auto backlog_v{10};

	[[nodiscard]] virtual auto get_desired_backlog() const -> int { return backlog_v; }
	[[nodiscard]] virtual auto should_poll() const -> bool { return true; }
	[[nodiscard]] virtual auto get_poll_timeout() const -> std::chrono::milliseconds { return 250ms; }

	virtual void initialize(Address const& /*listener_address*/) {}
	virtual void on_accept(std::unique_ptr<IConnection> connection) = 0;
	virtual void shutdown() {}
};
} // namespace krpc
