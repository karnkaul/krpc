#pragma once
#include "krpc/address.hpp"
#include "krpc/polymorphic.hpp"
#include "krpc/result.hpp"
#include <chrono>
#include <cstddef>
#include <span>
#include <vector>

using namespace std::chrono_literals;

namespace krpc {
enum class Transfer : std::int8_t { Complete, Incomplete };

class IConnection : public Polymorphic {
  public:
	/// \brief Default polling timeout.
	static constexpr auto timeout_v = 1s;

	/// \brief Connect to a given Address.
	/// \param address Destination address.
	/// \returns Concrete instance on successful connection.
	[[nodiscard]] static auto create(Address const& address) -> Result<std::unique_ptr<IConnection>>;

	[[nodiscard]] virtual auto get_address() const -> Address const& = 0;

	/// \param data Non-empty array of bytes to send.
	/// \returns Success if all data sent.
	virtual auto send(std::span<std::byte const> data) -> Result<void> = 0;
	/// \param buffer Buffer to receive data into.
	/// \returns krpc::Transfer::Complete or krpc::Transfer::Incomplete if successfully received.
	virtual auto receive_to(std::vector<std::byte>& out_buffer) -> Result<Transfer> = 0;

	/// \brief Polling timeout.
	std::chrono::milliseconds timeout{timeout_v};
};
} // namespace krpc
