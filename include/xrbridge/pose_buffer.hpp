#pragma once

#include "xrbridge/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

namespace xrbridge {

enum class Result : std::uint8_t {
  Ok = 0,
  NotInitialized,
  InvalidArgument,
  InvalidQuaternion,
  OutOfOrder,
  Duplicate,
  Unavailable,
  Stale,
  InternalError,
};

struct RuntimeStats {
  std::uint64_t submitted{};
  std::uint64_t queries{};
  std::uint64_t interpolated{};
  std::uint64_t rejected_invalid{};
  std::uint64_t rejected_out_of_order{};
  std::uint64_t rejected_duplicate{};
  std::uint64_t dropped_overflow{};
  std::uint64_t stale_queries{};
  std::uint64_t unavailable_queries{};
};

class PoseBuffer final {
 public:
  explicit PoseBuffer(std::size_t capacity_per_device, std::int64_t max_age_ns);

  [[nodiscard]] Result submit(Device device, TimestampedPose pose);
  [[nodiscard]] Result query(Device device, std::int64_t timestamp_ns,
                             TimestampedPose& output) const;
  [[nodiscard]] RuntimeStats stats() const;
  [[nodiscard]] std::size_t size(Device device) const;

 private:
  static constexpr std::size_t device_count = 3;
  [[nodiscard]] static std::size_t index(Device device) noexcept;

  const std::size_t capacity_per_device_;
  const std::int64_t max_age_ns_;
  mutable std::mutex mutex_;
  std::array<std::deque<TimestampedPose>, device_count> samples_;
  mutable RuntimeStats stats_{};
};

}  // namespace xrbridge
