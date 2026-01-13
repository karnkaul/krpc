#pragma once
#include "krpc/connection.hpp"
#include "krpc/listener.hpp"
#include <memory>

namespace krpc {
struct LibraryOptions {
	bool skipWsaStartup{false};
};

class ILibrary : public Polymorphic {
  public:
	using Options = LibraryOptions;

	static constexpr auto backlog_v{10};

	[[nodiscard]] static auto create(Options const& options = {}) noexcept(false) -> std::unique_ptr<ILibrary>;

	[[nodiscard]] virtual auto connect_to(Address const& address) noexcept(false) -> std::unique_ptr<IConnection> = 0;

	virtual auto listen_on(Address const& address, Listener& listener) -> bool = 0;
};
} // namespace krpc
