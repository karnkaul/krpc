#pragma once
#include "krpc/address.hpp"
#include "krpc/polymorphic.hpp"
#include "krpc/result.hpp"
#include <chrono>
#include <cstddef>
#include <span>

using namespace std::chrono_literals;

namespace krpc {
class IConnection : public Polymorphic {
  public:
	/// \brief Default polling timeout.
	static constexpr auto timeout_v = 1s;

	[[nodiscard]] virtual auto get_address() const -> Address const& = 0;

	/// \param data Non-empty array of bytes to send.
	/// \returns Success if all data sent.
	virtual auto send(std::span<std::byte const> data) -> Result<void> = 0;
	/// \param buffer Non-empty buffer to receive data into until full.
	/// \returns Success if buffer is full.
	virtual auto receive(std::span<std::byte> buffer) -> Result<void> = 0;

	/// \brief Polling timeout.
	std::chrono::milliseconds timeout{timeout_v};
};

/// \brief Connect to a given Address.
/// \param address Destination address.
/// \param out Output connection, set to a concrete instance on success.
/// \returns Concrete instance on successful connection.
[[nodiscard]] auto connect_to(Address const& address) -> Result<std::unique_ptr<IConnection>>;
} // namespace krpc
