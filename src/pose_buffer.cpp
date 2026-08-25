#include "xrbridge/pose_buffer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace xrbridge {

PoseBuffer::PoseBuffer(std::size_t capacity_per_device, std::int64_t max_age_ns)
    : capacity_per_device_(capacity_per_device), max_age_ns_(max_age_ns) {
  if (capacity_per_device == 0 || max_age_ns < 0) {
    throw std::invalid_argument("capacity must be positive and max age non-negative");
  }
}

std::size_t PoseBuffer::index(Device device) noexcept { return static_cast<std::size_t>(device); }

Result PoseBuffer::submit(Device device, TimestampedPose pose) {
  if (index(device) >= device_count || pose.timestamp_ns < 0 ||
      !is_finite(pose.transform.position) || !is_finite(pose.transform.rotation) ||
      norm(pose.transform.rotation) < 1e-12) {
    std::scoped_lock lock(mutex_);
    ++stats_.rejected_invalid;
    return norm(pose.transform.rotation) < 1e-12 ? Result::InvalidQuaternion
                                                 : Result::InvalidArgument;
  }
  try {
    pose.transform.rotation = normalize(pose.transform.rotation);
  } catch (const std::invalid_argument&) {
    std::scoped_lock lock(mutex_);
    ++stats_.rejected_invalid;
    return Result::InvalidQuaternion;
  }

  std::scoped_lock lock(mutex_);
  auto& queue = samples_[index(device)];
  if (!queue.empty()) {
    if (pose.timestamp_ns == queue.back().timestamp_ns) {
      ++stats_.rejected_duplicate;
      return Result::Duplicate;
    }
    if (pose.timestamp_ns < queue.back().timestamp_ns) {
      ++stats_.rejected_out_of_order;
      return Result::OutOfOrder;
    }
  }
  queue.push_back(pose);
  ++stats_.submitted;
  if (queue.size() > capacity_per_device_) {
    queue.pop_front();
    ++stats_.dropped_overflow;
  }
  return Result::Ok;
}

Result PoseBuffer::query(Device device, std::int64_t timestamp_ns, TimestampedPose& output) const {
  std::scoped_lock lock(mutex_);
  ++stats_.queries;
  if (index(device) >= device_count || timestamp_ns < 0) {
    return Result::InvalidArgument;
  }
  const auto& queue = samples_[index(device)];
  if (queue.empty() || timestamp_ns < queue.front().timestamp_ns) {
    ++stats_.unavailable_queries;
    return Result::Unavailable;
  }
  if (timestamp_ns > queue.back().timestamp_ns) {
    if (timestamp_ns - queue.back().timestamp_ns > max_age_ns_) {
      ++stats_.stale_queries;
      return Result::Stale;
    }
    output = queue.back();
    return Result::Ok;
  }

  const auto upper = std::lower_bound(
      queue.begin(), queue.end(), timestamp_ns,
      [](const TimestampedPose& pose, std::int64_t timestamp) { return pose.timestamp_ns < timestamp; });
  if (upper == queue.end() || upper->timestamp_ns == timestamp_ns) {
    output = upper == queue.end() ? queue.back() : *upper;
    return Result::Ok;
  }
  const auto lower = std::prev(upper);
  const auto span = upper->timestamp_ns - lower->timestamp_ns;
  if (span <= 0) {
    ++stats_.unavailable_queries;
    return Result::InternalError;
  }
  const double alpha = static_cast<double>(timestamp_ns - lower->timestamp_ns) /
                       static_cast<double>(span);
  output = {interpolate(lower->transform, upper->transform, alpha), timestamp_ns};
  ++stats_.interpolated;
  return Result::Ok;
}

RuntimeStats PoseBuffer::stats() const {
  std::scoped_lock lock(mutex_);
  return stats_;
}

void PoseBuffer::record_invalid_submission() {
  std::scoped_lock lock(mutex_);
  ++stats_.rejected_invalid;
}

std::size_t PoseBuffer::size(Device device) const {
  std::scoped_lock lock(mutex_);
  return index(device) < device_count ? samples_[index(device)].size() : 0;
}

}  // namespace xrbridge
