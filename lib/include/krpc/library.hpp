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

	[[nodiscard]] static auto create(Options const& options = {}) noexcept(false) -> std::unique_ptr<ILibrary>;

	[[nodiscard]] virtual auto connect_to(Address const& address) const noexcept(false) -> std::unique_ptr<IConnection> = 0;

	virtual auto listen_on(Address const& address, Listener& listener) const -> bool = 0;
	auto listen_on(int const port, Listener& listener) const -> bool { return listen_on(Address{.host = "localhost", .port = port}, listener); }
};
} // namespace krpc
