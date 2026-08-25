#pragma once

#include <array>
#include <cstdint>

namespace xrbridge {

struct Vec3 {
  double x{};
  double y{};
  double z{};
};

struct Quaternion {
  double x{};
  double y{};
  double z{};
  double w{1.0};
};

using Mat3 = std::array<std::array<double, 3>, 3>;

struct Transform {
  Vec3 position{};
  Quaternion rotation{};
};

struct TimestampedPose {
  Transform transform{};
  std::int64_t timestamp_ns{};
};

enum class Device : std::uint8_t { Head = 0, LeftController = 1, RightController = 2 };

[[nodiscard]] bool is_finite(Vec3 value) noexcept;
[[nodiscard]] bool is_finite(Quaternion value) noexcept;
[[nodiscard]] double norm(Quaternion value) noexcept;
[[nodiscard]] Quaternion normalize(Quaternion value);
[[nodiscard]] Quaternion multiply(Quaternion lhs, Quaternion rhs) noexcept;
[[nodiscard]] Vec3 rotate(Quaternion rotation, Vec3 value);
[[nodiscard]] Mat3 to_rotation_matrix(Quaternion value);
[[nodiscard]] Quaternion from_rotation_matrix(const Mat3& value);
[[nodiscard]] Quaternion slerp(Quaternion from, Quaternion to, double alpha);
[[nodiscard]] Transform interpolate(const Transform& from, const Transform& to, double alpha);
[[nodiscard]] Transform compose(const Transform& parent, const Transform& child);
[[nodiscard]] Transform openxr_to_unity(const Transform& value);
[[nodiscard]] Transform unity_to_openxr(const Transform& value);
[[nodiscard]] double position_distance(Vec3 lhs, Vec3 rhs) noexcept;
[[nodiscard]] double angular_distance_radians(Quaternion lhs, Quaternion rhs);

}  // namespace xrbridge
