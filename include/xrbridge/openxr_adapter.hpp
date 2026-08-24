#pragma once

#include "xrbridge/types.hpp"

#if defined(XRBRIDGE_WITH_OPENXR)
#include <openxr/openxr.h>

namespace xrbridge {

[[nodiscard]] inline Transform from_openxr(const XrPosef& pose) noexcept {
  return {{pose.position.x, pose.position.y, pose.position.z},
          {pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w}};
}

}  // namespace xrbridge
#endif
