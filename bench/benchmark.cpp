#include "xrbridge/pose_buffer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/utsname.h>
#else
#include <sys/utsname.h>
#include <fstream>
#endif

namespace {

using Clock = std::chrono::steady_clock;

std::string os_description() {
#if defined(_WIN32)
  return "Windows";
#else
  utsname value{};
  if (uname(&value) == 0) {
    return std::string(value.sysname) + " " + value.release + " " + value.machine;
  }
  return "unknown";
#endif
}

std::string cpu_description() {
#if defined(_WIN32)
  char buffer[256]{};
  DWORD size = sizeof(buffer);
  return GetEnvironmentVariableA("PROCESSOR_IDENTIFIER", buffer, size) > 0 ? buffer : "unknown";
#elif defined(__APPLE__)
  std::size_t size = 0;
  if (sysctlbyname("machdep.cpu.brand_string", nullptr, &size, nullptr, 0) != 0) {
    return "unknown";
  }
  std::string result(size, '\0');
  if (sysctlbyname("machdep.cpu.brand_string", result.data(), &size, nullptr, 0) != 0) {
    return "unknown";
  }
  if (!result.empty() && result.back() == '\0') result.pop_back();
  return result;
#else
  std::ifstream input("/proc/cpuinfo");
  std::string line;
  while (std::getline(input, line)) {
    constexpr std::string_view key = "model name";
    if (line.rfind(key, 0) == 0) {
      const auto separator = line.find(':');
      return separator == std::string::npos ? line : line.substr(separator + 2);
    }
  }
  return "unknown";
#endif
}

std::string compiler_description() {
#if defined(__clang__)
  return std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
  return std::string("GCC ") + __VERSION__;
#elif defined(_MSC_VER)
  return std::string("MSVC ") + std::to_string(_MSC_VER);
#else
  return "unknown";
#endif
}

std::size_t percentile_index(std::size_t count, double percentile) {
  return static_cast<std::size_t>(std::ceil(percentile * static_cast<double>(count))) - 1;
}

xrbridge::Transform synthetic_pose(std::size_t index) {
  const double phase = static_cast<double>(index) * 0.001;
  const double half_angle = 0.25 * std::sin(phase) * 0.5;
  return {{std::sin(phase) * 2.0, 1.6 + std::cos(phase * 0.5) * 0.1,
           -std::cos(phase) * 2.0},
          {0.0, std::sin(half_angle), 0.0, std::cos(half_angle)}};
}

std::string json_escape(std::string value) {
  std::string output;
  for (const char character : value) {
    if (character == '\\' || character == '"') output.push_back('\\');
    output.push_back(character);
  }
  return output;
}

}  // namespace

int main(int argc, char** argv) {
  std::size_t sample_count = 100000;
  std::string output_path = "benchmark.json";
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--samples" && index + 1 < argc) {
      sample_count = std::stoull(argv[++index]);
    } else if (argument == "--output" && index + 1 < argc) {
      output_path = argv[++index];
    } else {
      std::cerr << "usage: xrbridge_benchmark [--samples N] [--output FILE]\n";
      return EXIT_FAILURE;
    }
  }
  if (sample_count < 100000) {
    std::cerr << "sample count must be at least 100000\n";
    return EXIT_FAILURE;
  }

  xrbridge::PoseBuffer buffer(sample_count + 1, 20'000'000);
  std::vector<std::int64_t> query_latency_ns;
  query_latency_ns.reserve(sample_count - 1);
  std::uint64_t interpolation_failures = 0;
  double max_position_error = 0.0;
  double max_angular_error = 0.0;
  constexpr std::int64_t step_ns = 11'111'111;

  const auto started = Clock::now();
  for (std::size_t index = 0; index < sample_count; ++index) {
    const auto source = synthetic_pose(index);
    const auto unity = xrbridge::openxr_to_unity(source);
    const auto round_trip = xrbridge::unity_to_openxr(unity);
    max_position_error = std::max(max_position_error,
                                  xrbridge::position_distance(source.position, round_trip.position));
    max_angular_error = std::max(
        max_angular_error, xrbridge::angular_distance_radians(source.rotation, round_trip.rotation));
    const auto result = buffer.submit(xrbridge::Device::Head,
                                      {unity, static_cast<std::int64_t>(index) * step_ns});
    if (result != xrbridge::Result::Ok) {
      std::cerr << "submission failed at sample " << index << '\n';
      return EXIT_FAILURE;
    }
  }

  for (std::size_t index = 0; index + 1 < sample_count; ++index) {
    xrbridge::TimestampedPose output{};
    const auto before = Clock::now();
    const auto result = buffer.query(xrbridge::Device::Head,
                                     static_cast<std::int64_t>(index) * step_ns + step_ns / 2,
                                     output);
    const auto after = Clock::now();
    query_latency_ns.push_back(
        std::chrono::duration_cast<std::chrono::nanoseconds>(after - before).count());
    if (result != xrbridge::Result::Ok) ++interpolation_failures;
  }
  const auto finished = Clock::now();
  const double elapsed_seconds = std::chrono::duration<double>(finished - started).count();
  std::sort(query_latency_ns.begin(), query_latency_ns.end());
  const auto median = query_latency_ns[percentile_index(query_latency_ns.size(), 0.50)];
  const auto p95 = query_latency_ns[percentile_index(query_latency_ns.size(), 0.95)];
  const auto stats = buffer.stats();
  const double throughput = static_cast<double>(sample_count + query_latency_ns.size()) /
                            elapsed_seconds;

#if defined(NDEBUG)
  constexpr std::string_view build_type = "Release";
#else
  constexpr std::string_view build_type = "Debug";
#endif
  const std::string command = "./build-release/xrbridge_benchmark --samples " +
                              std::to_string(sample_count) + " --output " + output_path;
  std::ofstream output(output_path, std::ios::trunc);
  if (!output) {
    std::cerr << "unable to open output file: " << output_path << '\n';
    return EXIT_FAILURE;
  }
  output << std::setprecision(15)
         << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"sample_count\": " << sample_count << ",\n"
         << "  \"query_count\": " << query_latency_ns.size() << ",\n"
         << "  \"elapsed_seconds\": " << elapsed_seconds << ",\n"
         << "  \"throughput_operations_per_second\": " << throughput << ",\n"
         << "  \"median_query_latency_ns\": " << median << ",\n"
         << "  \"p95_query_latency_ns\": " << p95 << ",\n"
         << "  \"dropped_samples\": " << stats.dropped_overflow << ",\n"
         << "  \"interpolation_failures\": " << interpolation_failures << ",\n"
         << "  \"max_position_round_trip_error_m\": " << max_position_error << ",\n"
         << "  \"max_angular_round_trip_error_rad\": " << max_angular_error << ",\n"
         << "  \"environment\": {\n"
         << "    \"cpu\": \"" << json_escape(cpu_description()) << "\",\n"
         << "    \"os\": \"" << json_escape(os_description()) << "\",\n"
         << "    \"compiler\": \"" << json_escape(compiler_description()) << "\",\n"
         << "    \"build_type\": \"" << build_type << "\",\n"
         << "    \"command\": \"" << json_escape(command) << "\"\n"
         << "  }\n"
         << "}\n";
  std::cout << "wrote " << output_path << "\nthroughput=" << throughput
            << " ops/s median=" << median << " ns p95=" << p95 << " ns\n";
  return interpolation_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
