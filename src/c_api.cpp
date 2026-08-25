#include "xrbridge/xrbridge.h"

#include "xrbridge/pose_buffer.hpp"

#include <memory>
#include <mutex>
#include <new>
#include <string>

namespace {

std::mutex lifecycle_mutex;
std::shared_ptr<xrbridge::PoseBuffer> context;
thread_local std::string last_error_message;

xrbridge_result to_c(xrbridge::Result result) noexcept {
  return static_cast<xrbridge_result>(result);
}

bool valid_device(xrbridge_device device) noexcept {
  return device >= XRBRIDGE_DEVICE_HEAD && device <= XRBRIDGE_DEVICE_RIGHT_CONTROLLER;
}

xrbridge::Device to_native(xrbridge_device device) noexcept {
  return static_cast<xrbridge::Device>(device);
}

void set_error(xrbridge_result result) { last_error_message = xrbridge_result_string(result); }

std::shared_ptr<xrbridge::PoseBuffer> acquire_context() {
  std::scoped_lock lock(lifecycle_mutex);
  return context;
}

}  // namespace

extern "C" {

xrbridge_result xrbridge_initialize(const xrbridge_config* config) {
  if (config == nullptr || config->capacity_per_device == 0 || config->max_sample_age_ns < 0) {
    set_error(XRBRIDGE_INVALID_ARGUMENT);
    return XRBRIDGE_INVALID_ARGUMENT;
  }
  std::scoped_lock lock(lifecycle_mutex);
  if (context) {
    set_error(XRBRIDGE_INVALID_ARGUMENT);
    last_error_message = "XRBridge is already initialized";
    return XRBRIDGE_INVALID_ARGUMENT;
  }
  try {
    context = std::make_shared<xrbridge::PoseBuffer>(config->capacity_per_device,
                                                      config->max_sample_age_ns);
    last_error_message.clear();
    return XRBRIDGE_OK;
  } catch (const std::exception& error) {
    last_error_message = error.what();
    return XRBRIDGE_INTERNAL_ERROR;
  }
}

xrbridge_result xrbridge_shutdown(void) {
  std::scoped_lock lock(lifecycle_mutex);
  if (!context) {
    set_error(XRBRIDGE_NOT_INITIALIZED);
    return XRBRIDGE_NOT_INITIALIZED;
  }
  context.reset();
  last_error_message.clear();
  return XRBRIDGE_OK;
}

xrbridge_result xrbridge_submit_openxr_pose(xrbridge_device device, const xrbridge_pose* pose) {
  const auto active = acquire_context();
  if (!active) {
    set_error(XRBRIDGE_NOT_INITIALIZED);
    return XRBRIDGE_NOT_INITIALIZED;
  }
  if (!valid_device(device) || pose == nullptr) {
    set_error(XRBRIDGE_INVALID_ARGUMENT);
    return XRBRIDGE_INVALID_ARGUMENT;
  }
  try {
    const xrbridge::Transform openxr{{pose->position.x, pose->position.y, pose->position.z},
                                     {pose->rotation.x, pose->rotation.y, pose->rotation.z,
                                      pose->rotation.w}};
    if (!xrbridge::is_finite(openxr.position)) {
      set_error(XRBRIDGE_INVALID_ARGUMENT);
      return XRBRIDGE_INVALID_ARGUMENT;
    }
    if (!xrbridge::is_finite(openxr.rotation) || xrbridge::norm(openxr.rotation) < 1e-12) {
      set_error(XRBRIDGE_INVALID_QUATERNION);
      return XRBRIDGE_INVALID_QUATERNION;
    }
    const auto result = active->submit(
        to_native(device), {xrbridge::openxr_to_unity(openxr), pose->timestamp_ns});
    const auto converted = to_c(result);
    if (converted != XRBRIDGE_OK) {
      set_error(converted);
    } else {
      last_error_message.clear();
    }
    return converted;
  } catch (const std::invalid_argument& error) {
    last_error_message = error.what();
    return XRBRIDGE_INVALID_QUATERNION;
  } catch (const std::exception& error) {
    last_error_message = error.what();
    return XRBRIDGE_INTERNAL_ERROR;
  }
}

xrbridge_result xrbridge_query_unity_pose(xrbridge_device device, int64_t timestamp_ns,
                                           xrbridge_pose* output) {
  const auto active = acquire_context();
  if (!active) {
    set_error(XRBRIDGE_NOT_INITIALIZED);
    return XRBRIDGE_NOT_INITIALIZED;
  }
  if (!valid_device(device) || output == nullptr || timestamp_ns < 0) {
    set_error(XRBRIDGE_INVALID_ARGUMENT);
    return XRBRIDGE_INVALID_ARGUMENT;
  }
  xrbridge::TimestampedPose pose{};
  const auto result = active->query(to_native(device), timestamp_ns, pose);
  const auto converted = to_c(result);
  if (result != xrbridge::Result::Ok) {
    set_error(converted);
    return converted;
  }
  *output = {{pose.transform.position.x, pose.transform.position.y, pose.transform.position.z},
             {pose.transform.rotation.x, pose.transform.rotation.y, pose.transform.rotation.z,
              pose.transform.rotation.w},
             pose.timestamp_ns};
  last_error_message.clear();
  return XRBRIDGE_OK;
}

xrbridge_result xrbridge_get_stats(xrbridge_stats* output) {
  const auto active = acquire_context();
  if (!active) {
    set_error(XRBRIDGE_NOT_INITIALIZED);
    return XRBRIDGE_NOT_INITIALIZED;
  }
  if (output == nullptr) {
    set_error(XRBRIDGE_INVALID_ARGUMENT);
    return XRBRIDGE_INVALID_ARGUMENT;
  }
  const auto stats = active->stats();
  *output = {stats.submitted,
             stats.queries,
             stats.interpolated,
             stats.rejected_invalid,
             stats.rejected_out_of_order,
             stats.rejected_duplicate,
             stats.dropped_overflow,
             stats.stale_queries,
             stats.unavailable_queries};
  last_error_message.clear();
  return XRBRIDGE_OK;
}

const char* xrbridge_result_string(xrbridge_result result) {
  switch (result) {
    case XRBRIDGE_OK: return "ok";
    case XRBRIDGE_NOT_INITIALIZED: return "not initialized";
    case XRBRIDGE_INVALID_ARGUMENT: return "invalid argument";
    case XRBRIDGE_INVALID_QUATERNION: return "invalid quaternion";
    case XRBRIDGE_OUT_OF_ORDER: return "out-of-order sample";
    case XRBRIDGE_DUPLICATE: return "duplicate sample";
    case XRBRIDGE_UNAVAILABLE: return "pose unavailable";
    case XRBRIDGE_STALE: return "stale pose";
    case XRBRIDGE_INTERNAL_ERROR: return "internal error";
  }
  return "unknown result";
}

const char* xrbridge_last_error(void) { return last_error_message.c_str(); }

}  // extern "C"
