#pragma once
#include <stdexcept>

namespace krpc {
struct Error : std::runtime_error {
	using std::runtime_error::runtime_error;
};
} // namespace krpc
