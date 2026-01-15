#pragma once
#include "krpc/error.hpp"
#include <expected>

namespace krpc {
template <typename Type>
using Result = std::expected<Type, Error>;
} // namespace krpc
