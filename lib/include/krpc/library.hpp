#pragma once
#include "krpc/polymorphic.hpp"
#include "krpc/result.hpp"
#include <memory>

namespace krpc {
class ILibrary : public Polymorphic {
  public:
	/// \returns Concrete instance on success.
	/// Error::InitializationFailure on failure.
	[[nodiscard]] static auto create() -> Result<std::unique_ptr<ILibrary>>;
};
} // namespace krpc
