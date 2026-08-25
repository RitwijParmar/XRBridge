#include "xrbridge/pose_buffer.hpp"
#include "xrbridge/xrbridge.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <thread>

namespace {

constexpr double tolerance = 1e-10;

xrbridge::Quaternion axis_angle(xrbridge::Vec3 axis, double angle) {
  const double half = angle * 0.5;
  return {axis.x * std::sin(half), axis.y * std::sin(half), axis.z * std::sin(half),
          std::cos(half)};
}

xrbridge::TimestampedPose pose_at(std::int64_t timestamp, double x = 0.0) {
  return {{{x, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}}, timestamp};
}

void expect_vec_near(xrbridge::Vec3 actual, xrbridge::Vec3 expected, double epsilon = tolerance) {
  EXPECT_NEAR(actual.x, expected.x, epsilon);
  EXPECT_NEAR(actual.y, expected.y, epsilon);
  EXPECT_NEAR(actual.z, expected.z, epsilon);
}

class CApiTest : public testing::Test {
 protected:
  void SetUp() override { (void)xrbridge_shutdown(); }
  void TearDown() override { (void)xrbridge_shutdown(); }
};

}  // namespace

TEST(Math, IdentityAndKnownAxisRotations) {
  expect_vec_near(xrbridge::rotate({0.0, 0.0, 0.0, 1.0}, {1.0, 2.0, 3.0}),
                  {1.0, 2.0, 3.0});
  const auto quarter_turn = axis_angle({0.0, 1.0, 0.0}, std::numbers::pi / 2.0);
  expect_vec_near(xrbridge::rotate(quarter_turn, {0.0, 0.0, -1.0}), {-1.0, 0.0, 0.0});
}

TEST(Math, CoordinateConversionReflectsPositionAndPreservesPhysicalRotation) {
  const xrbridge::Transform source{{1.0, 2.0, -3.0},
                                   axis_angle({0.0, 1.0, 0.0}, std::numbers::pi / 2.0)};
  const auto unity = xrbridge::openxr_to_unity(source);
  expect_vec_near(unity.position, {1.0, 2.0, 3.0});
  const auto openxr_forward_rotated = xrbridge::rotate(source.rotation, {0.0, 0.0, -1.0});
  const auto unity_forward_rotated = xrbridge::rotate(unity.rotation, {0.0, 0.0, 1.0});
  expect_vec_near(unity_forward_rotated,
                  {openxr_forward_rotated.x, openxr_forward_rotated.y,
                   -openxr_forward_rotated.z});
}

TEST(Math, QuaternionMatrixRoundTripAndNormalization) {
  const xrbridge::Quaternion input{0.2, -0.4, 0.1, 0.8};
  const auto normalized = xrbridge::normalize(input);
  EXPECT_NEAR(xrbridge::norm(normalized), 1.0, tolerance);
  const auto round_trip = xrbridge::from_rotation_matrix(xrbridge::to_rotation_matrix(input));
  EXPECT_NEAR(xrbridge::angular_distance_radians(normalized, round_trip), 0.0, tolerance);
  EXPECT_THROW((void)xrbridge::normalize({0.0, 0.0, 0.0, 0.0}), std::invalid_argument);
}

TEST(Math, SlerpEndpointsAndMidpoint) {
  const auto identity = axis_angle({0.0, 1.0, 0.0}, 0.0);
  const auto half_turn = axis_angle({0.0, 1.0, 0.0}, std::numbers::pi);
  EXPECT_NEAR(xrbridge::angular_distance_radians(xrbridge::slerp(identity, half_turn, 0.0),
                                                  identity),
              0.0, tolerance);
  EXPECT_NEAR(xrbridge::angular_distance_radians(xrbridge::slerp(identity, half_turn, 1.0),
                                                  half_turn),
              0.0, tolerance);
  const auto midpoint = xrbridge::slerp(identity, half_turn, 0.5);
  expect_vec_near(xrbridge::rotate(midpoint, {0.0, 0.0, -1.0}), {-1.0, 0.0, 0.0});
}

TEST(Math, TransformCompositionAppliesChildInParentSpace) {
  const xrbridge::Transform parent{{1.0, 0.0, 0.0},
                                   axis_angle({0.0, 1.0, 0.0}, std::numbers::pi / 2.0)};
  const xrbridge::Transform child{{0.0, 0.0, -2.0}, {0.0, 0.0, 0.0, 1.0}};
  const auto result = xrbridge::compose(parent, child);
  expect_vec_near(result.position, {-1.0, 0.0, 0.0});
}

TEST(PoseBuffer, InterpolatesTimestampedSamples) {
  xrbridge::PoseBuffer buffer(4, 100);
  ASSERT_EQ(buffer.submit(xrbridge::Device::Head, pose_at(100, 0.0)), xrbridge::Result::Ok);
  ASSERT_EQ(buffer.submit(xrbridge::Device::Head, pose_at(200, 10.0)), xrbridge::Result::Ok);
  xrbridge::TimestampedPose output{};
  ASSERT_EQ(buffer.query(xrbridge::Device::Head, 125, output), xrbridge::Result::Ok);
  EXPECT_DOUBLE_EQ(output.transform.position.x, 2.5);
  EXPECT_EQ(output.timestamp_ns, 125);
  EXPECT_EQ(buffer.stats().interpolated, 1U);
}

TEST(PoseBuffer, RejectsOutOfOrderDuplicatesAndInvalidQuaternion) {
  xrbridge::PoseBuffer buffer(4, 100);
  ASSERT_EQ(buffer.submit(xrbridge::Device::Head, pose_at(100)), xrbridge::Result::Ok);
  EXPECT_EQ(buffer.submit(xrbridge::Device::Head, pose_at(100)), xrbridge::Result::Duplicate);
  EXPECT_EQ(buffer.submit(xrbridge::Device::Head, pose_at(99)), xrbridge::Result::OutOfOrder);
  auto invalid = pose_at(101);
  invalid.transform.rotation = {0.0, 0.0, 0.0, 0.0};
  EXPECT_EQ(buffer.submit(xrbridge::Device::Head, invalid), xrbridge::Result::InvalidQuaternion);
  const auto stats = buffer.stats();
  EXPECT_EQ(stats.rejected_duplicate, 1U);
  EXPECT_EQ(stats.rejected_out_of_order, 1U);
  EXPECT_EQ(stats.rejected_invalid, 1U);
}

TEST(PoseBuffer, BoundsStorageAndReportsStalePoses) {
  xrbridge::PoseBuffer buffer(2, 10);
  ASSERT_EQ(buffer.submit(xrbridge::Device::Head, pose_at(100)), xrbridge::Result::Ok);
  ASSERT_EQ(buffer.submit(xrbridge::Device::Head, pose_at(110)), xrbridge::Result::Ok);
  ASSERT_EQ(buffer.submit(xrbridge::Device::Head, pose_at(120)), xrbridge::Result::Ok);
  EXPECT_EQ(buffer.size(xrbridge::Device::Head), 2U);
  EXPECT_EQ(buffer.stats().dropped_overflow, 1U);
  xrbridge::TimestampedPose output{};
  EXPECT_EQ(buffer.query(xrbridge::Device::Head, 100, output), xrbridge::Result::Unavailable);
  EXPECT_EQ(buffer.query(xrbridge::Device::Head, 131, output), xrbridge::Result::Stale);
}

TEST(PoseBuffer, ConcurrentProducerAndConsumer) {
  constexpr std::int64_t count = 20000;
  xrbridge::PoseBuffer buffer(static_cast<std::size_t>(count), 10);
  std::atomic<std::int64_t> published{-1};
  std::atomic<std::uint64_t> failures{0};
  std::thread producer([&] {
    for (std::int64_t index = 0; index < count; ++index) {
      if (buffer.submit(xrbridge::Device::Head, pose_at(index, static_cast<double>(index))) !=
          xrbridge::Result::Ok) {
        ++failures;
      }
      published.store(index, std::memory_order_release);
    }
  });
  std::thread consumer([&] {
    while (published.load(std::memory_order_acquire) < count - 1) {
      const auto timestamp = published.load(std::memory_order_acquire);
      if (timestamp >= 0) {
        xrbridge::TimestampedPose output{};
        if (buffer.query(xrbridge::Device::Head, timestamp, output) != xrbridge::Result::Ok) {
          ++failures;
        }
      }
    }
  });
  producer.join();
  consumer.join();
  EXPECT_EQ(failures.load(), 0U);
  EXPECT_EQ(buffer.size(xrbridge::Device::Head), static_cast<std::size_t>(count));
}

TEST(PoseBuffer, DeterministicReplayOfOneHundredThousandPoses) {
  constexpr std::int64_t count = 100000;
  xrbridge::PoseBuffer buffer(static_cast<std::size_t>(count), 1000);
  for (std::int64_t index = 0; index < count; ++index) {
    ASSERT_EQ(buffer.submit(xrbridge::Device::Head, pose_at(index * 10, index * 0.001)),
              xrbridge::Result::Ok);
  }
  xrbridge::TimestampedPose output{};
  for (std::int64_t index = 0; index < count - 1; index += 997) {
    ASSERT_EQ(buffer.query(xrbridge::Device::Head, index * 10 + 5, output),
              xrbridge::Result::Ok);
    EXPECT_NEAR(output.transform.position.x, index * 0.001 + 0.0005, tolerance);
  }
  EXPECT_EQ(buffer.stats().dropped_overflow, 0U);
}

TEST_F(CApiTest, LifecycleAndInvalidArguments) {
  EXPECT_EQ(xrbridge_query_unity_pose(XRBRIDGE_DEVICE_HEAD, 0, nullptr),
            XRBRIDGE_NOT_INITIALIZED);
  EXPECT_EQ(xrbridge_initialize(nullptr), XRBRIDGE_INVALID_ARGUMENT);
  const xrbridge_config config{8, 100};
  ASSERT_EQ(xrbridge_initialize(&config), XRBRIDGE_OK);
  EXPECT_EQ(xrbridge_initialize(&config), XRBRIDGE_INVALID_ARGUMENT);
  EXPECT_EQ(xrbridge_submit_openxr_pose(static_cast<xrbridge_device>(99), nullptr),
            XRBRIDGE_INVALID_ARGUMENT);
  EXPECT_EQ(xrbridge_get_stats(nullptr), XRBRIDGE_INVALID_ARGUMENT);
  EXPECT_EQ(xrbridge_shutdown(), XRBRIDGE_OK);
  EXPECT_EQ(xrbridge_shutdown(), XRBRIDGE_NOT_INITIALIZED);
}

TEST_F(CApiTest, ConvertsAndInterpolatesThroughStableAbi) {
  const xrbridge_config config{8, 100};
  ASSERT_EQ(xrbridge_initialize(&config), XRBRIDGE_OK);
  const xrbridge_pose first{{1.0, 2.0, -3.0}, {0.0, 0.0, 0.0, 1.0}, 100};
  const xrbridge_pose second{{3.0, 2.0, -5.0}, {0.0, 0.0, 0.0, 1.0}, 200};
  ASSERT_EQ(xrbridge_submit_openxr_pose(XRBRIDGE_DEVICE_HEAD, &first), XRBRIDGE_OK);
  ASSERT_EQ(xrbridge_submit_openxr_pose(XRBRIDGE_DEVICE_HEAD, &second), XRBRIDGE_OK);
  xrbridge_pose output{};
  ASSERT_EQ(xrbridge_query_unity_pose(XRBRIDGE_DEVICE_HEAD, 150, &output), XRBRIDGE_OK);
  EXPECT_DOUBLE_EQ(output.position.x, 2.0);
  EXPECT_DOUBLE_EQ(output.position.y, 2.0);
  EXPECT_DOUBLE_EQ(output.position.z, 4.0);
  EXPECT_EQ(output.timestamp_ns, 150);
  xrbridge_stats stats{};
  ASSERT_EQ(xrbridge_get_stats(&stats), XRBRIDGE_OK);
  EXPECT_EQ(stats.submitted, 2U);
  EXPECT_EQ(stats.interpolated, 1U);
}

TEST_F(CApiTest, RejectsNonFiniteInputWithoutThrowingAcrossAbi) {
  const xrbridge_config config{8, 100};
  ASSERT_EQ(xrbridge_initialize(&config), XRBRIDGE_OK);
  const xrbridge_pose invalid{{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
                              {0.0, 0.0, 0.0, 1.0}, 1};
  EXPECT_EQ(xrbridge_submit_openxr_pose(XRBRIDGE_DEVICE_HEAD, &invalid),
            XRBRIDGE_INVALID_ARGUMENT);
  const xrbridge_pose invalid_rotation{{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, 2};
  EXPECT_EQ(xrbridge_submit_openxr_pose(XRBRIDGE_DEVICE_HEAD, &invalid_rotation),
            XRBRIDGE_INVALID_QUATERNION);
  EXPECT_STRNE(xrbridge_last_error(), "");
}
