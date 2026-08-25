#include "xrbridge/types.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace xrbridge {
namespace {

constexpr double epsilon = 1e-12;

Vec3 add(Vec3 lhs, Vec3 rhs) noexcept {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 lerp(Vec3 from, Vec3 to, double alpha) noexcept {
  return {from.x + (to.x - from.x) * alpha, from.y + (to.y - from.y) * alpha,
          from.z + (to.z - from.z) * alpha};
}

double dot(Quaternion lhs, Quaternion rhs) noexcept {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

Quaternion scale(Quaternion value, double factor) noexcept {
  return {value.x * factor, value.y * factor, value.z * factor, value.w * factor};
}

Quaternion add(Quaternion lhs, Quaternion rhs) noexcept {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
}

}  // namespace

bool is_finite(Vec3 value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool is_finite(Quaternion value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
         std::isfinite(value.w);
}

double norm(Quaternion value) noexcept { return std::sqrt(dot(value, value)); }

Quaternion normalize(Quaternion value) {
  const double length = norm(value);
  if (!is_finite(value) || !std::isfinite(length) || length < epsilon) {
    throw std::invalid_argument("quaternion must be finite and non-zero");
  }
  return scale(value, 1.0 / length);
}

Quaternion multiply(Quaternion lhs, Quaternion rhs) noexcept {
  return {lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
          lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
          lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
          lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z};
}

Vec3 rotate(Quaternion rotation, Vec3 value) {
  const Quaternion q = normalize(rotation);
  const Quaternion vector{value.x, value.y, value.z, 0.0};
  const Quaternion conjugate{-q.x, -q.y, -q.z, q.w};
  const Quaternion result = multiply(multiply(q, vector), conjugate);
  return {result.x, result.y, result.z};
}

Mat3 to_rotation_matrix(Quaternion value) {
  const Quaternion q = normalize(value);
  const double xx = q.x * q.x;
  const double yy = q.y * q.y;
  const double zz = q.z * q.z;
  const double xy = q.x * q.y;
  const double xz = q.x * q.z;
  const double yz = q.y * q.z;
  const double wx = q.w * q.x;
  const double wy = q.w * q.y;
  const double wz = q.w * q.z;
  return {{{1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy)},
           {2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx)},
           {2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy)}}};
}

Quaternion from_rotation_matrix(const Mat3& matrix) {
  Quaternion q{};
  const double trace = matrix[0][0] + matrix[1][1] + matrix[2][2];
  if (trace > 0.0) {
    const double s = std::sqrt(trace + 1.0) * 2.0;
    q = {(matrix[2][1] - matrix[1][2]) / s, (matrix[0][2] - matrix[2][0]) / s,
         (matrix[1][0] - matrix[0][1]) / s, 0.25 * s};
  } else if (matrix[0][0] > matrix[1][1] && matrix[0][0] > matrix[2][2]) {
    const double s = std::sqrt(1.0 + matrix[0][0] - matrix[1][1] - matrix[2][2]) * 2.0;
    q = {0.25 * s, (matrix[0][1] + matrix[1][0]) / s,
         (matrix[0][2] + matrix[2][0]) / s, (matrix[2][1] - matrix[1][2]) / s};
  } else if (matrix[1][1] > matrix[2][2]) {
    const double s = std::sqrt(1.0 + matrix[1][1] - matrix[0][0] - matrix[2][2]) * 2.0;
    q = {(matrix[0][1] + matrix[1][0]) / s, 0.25 * s,
         (matrix[1][2] + matrix[2][1]) / s, (matrix[0][2] - matrix[2][0]) / s};
  } else {
    const double s = std::sqrt(1.0 + matrix[2][2] - matrix[0][0] - matrix[1][1]) * 2.0;
    q = {(matrix[0][2] + matrix[2][0]) / s, (matrix[1][2] + matrix[2][1]) / s,
         0.25 * s, (matrix[1][0] - matrix[0][1]) / s};
  }
  return normalize(q);
}

Quaternion slerp(Quaternion from, Quaternion to, double alpha) {
  if (!std::isfinite(alpha) || alpha < 0.0 || alpha > 1.0) {
    throw std::invalid_argument("SLERP alpha must be in [0, 1]");
  }
  from = normalize(from);
  to = normalize(to);
  double cosine = dot(from, to);
  if (cosine < 0.0) {
    to = scale(to, -1.0);
    cosine = -cosine;
  }
  cosine = std::clamp(cosine, -1.0, 1.0);
  if (cosine > 0.9995) {
    return normalize(add(scale(from, 1.0 - alpha), scale(to, alpha)));
  }
  const double angle = std::acos(cosine);
  const double denominator = std::sin(angle);
  return normalize(add(scale(from, std::sin((1.0 - alpha) * angle) / denominator),
                       scale(to, std::sin(alpha * angle) / denominator)));
}

Transform interpolate(const Transform& from, const Transform& to, double alpha) {
  if (!std::isfinite(alpha) || alpha < 0.0 || alpha > 1.0) {
    throw std::invalid_argument("interpolation alpha must be in [0, 1]");
  }
  return {lerp(from.position, to.position, alpha), slerp(from.rotation, to.rotation, alpha)};
}

Transform compose(const Transform& parent, const Transform& child) {
  return {add(parent.position, rotate(parent.rotation, child.position)),
          normalize(multiply(normalize(parent.rotation), normalize(child.rotation)))};
}

Transform openxr_to_unity(const Transform& value) {
  if (!is_finite(value.position)) {
    throw std::invalid_argument("position must be finite");
  }
  const Quaternion q = normalize(value.rotation);
  return {{value.position.x, value.position.y, -value.position.z},
          normalize({-q.x, -q.y, q.z, q.w})};
}

Transform unity_to_openxr(const Transform& value) { return openxr_to_unity(value); }

double position_distance(Vec3 lhs, Vec3 rhs) noexcept {
  const double x = lhs.x - rhs.x;
  const double y = lhs.y - rhs.y;
  const double z = lhs.z - rhs.z;
  return std::sqrt(x * x + y * y + z * z);
}

double angular_distance_radians(Quaternion lhs, Quaternion rhs) {
  const double cosine = std::abs(dot(normalize(lhs), normalize(rhs)));
  return 2.0 * std::acos(std::clamp(cosine, 0.0, 1.0));
}

}  // namespace xrbridge
