#include "xrbridge/xrbridge.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#  define NOMINMAX
#  include <winsock2.h>
#  include <ws2tcpip.h>
using socket_handle = SOCKET;
constexpr socket_handle invalid_socket = INVALID_SOCKET;
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using socket_handle = int;
constexpr socket_handle invalid_socket = -1;
#endif

namespace {

using Clock = std::chrono::steady_clock;
std::atomic<bool> running{true};
std::atomic<bool> paused{false};
std::atomic<std::int64_t> last_timestamp_ns{0};
std::atomic<std::uint64_t> event_sequence{0};
std::mutex event_mutex;
std::string last_event = "native runtime initialized";

std::int64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch())
      .count();
}

void set_event(std::string event) {
  std::scoped_lock lock(event_mutex);
  last_event = std::move(event);
  ++event_sequence;
}

std::string current_event() {
  std::scoped_lock lock(event_mutex);
  return last_event;
}

xrbridge_quaternion euler(double pitch, double yaw, double roll) {
  const double cy = std::cos(yaw * 0.5);
  const double sy = std::sin(yaw * 0.5);
  const double cp = std::cos(pitch * 0.5);
  const double sp = std::sin(pitch * 0.5);
  const double cr = std::cos(roll * 0.5);
  const double sr = std::sin(roll * 0.5);
  return {sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy,
          cr * cp * sy - sr * sp * cy, cr * cp * cy + sr * sp * sy};
}

xrbridge_pose synthetic_pose(xrbridge_device device, double seconds, std::int64_t timestamp) {
  const double phase = device == XRBRIDGE_DEVICE_LEFT_CONTROLLER
                           ? -0.85
                           : (device == XRBRIDGE_DEVICE_RIGHT_CONTROLLER ? 0.85 : 0.0);
  const double side = device == XRBRIDGE_DEVICE_LEFT_CONTROLLER
                          ? -0.34
                          : (device == XRBRIDGE_DEVICE_RIGHT_CONTROLLER ? 0.34 : 0.0);
  const double is_head = device == XRBRIDGE_DEVICE_HEAD ? 1.0 : 0.0;
  xrbridge_pose pose{};
  pose.position = {side + std::sin(seconds * 1.7 + phase) * (0.05 + is_head * 0.03),
                   1.18 + is_head * 0.48 + std::sin(seconds * 2.1 + phase) * 0.055,
                   -0.48 + is_head * 0.28 + std::cos(seconds * 1.35 + phase) * 0.07};
  pose.rotation = euler(std::sin(seconds * 1.2 + phase) * 0.18,
                        std::sin(seconds * 0.75 + phase) * 0.34,
                        std::cos(seconds * 1.55 + phase) * 0.12);
  pose.timestamp_ns = timestamp;
  return pose;
}

void producer_loop() {
  const auto started = Clock::now();
  auto next = Clock::now();
  while (running.load()) {
    next += std::chrono::microseconds(11111);
    if (!paused.load()) {
      auto timestamp = now_ns();
      const auto previous = last_timestamp_ns.load();
      if (timestamp <= previous) timestamp = previous + 1;
      const double seconds = std::chrono::duration<double>(Clock::now() - started).count();
      for (int value = XRBRIDGE_DEVICE_HEAD; value <= XRBRIDGE_DEVICE_RIGHT_CONTROLLER; ++value) {
        const auto device = static_cast<xrbridge_device>(value);
        const auto pose = synthetic_pose(device, seconds, timestamp);
        (void)xrbridge_submit_openxr_pose(device, &pose);
      }
      last_timestamp_ns.store(timestamp);
    }
    std::this_thread::sleep_until(next);
  }
}

std::string json_escape(std::string_view input) {
  std::string output;
  for (const char character : input) {
    if (character == '"' || character == '\\') output.push_back('\\');
    if (character == '\n') {
      output += "\\n";
    } else {
      output.push_back(character);
    }
  }
  return output;
}

std::string pose_json(xrbridge_device device, std::int64_t query_timestamp) {
  xrbridge_pose pose{};
  const auto result = xrbridge_query_unity_pose(device, query_timestamp, &pose);
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(6) << "{\"result\":\""
         << xrbridge_result_string(result) << "\",\"timestamp_ns\":" << pose.timestamp_ns
         << ",\"position\":[" << pose.position.x << ',' << pose.position.y << ','
         << pose.position.z << "],\"rotation\":[" << pose.rotation.x << ',' << pose.rotation.y
         << ',' << pose.rotation.z << ',' << pose.rotation.w << "]}";
  return stream.str();
}

std::string frame_json() {
  const auto now = now_ns();
  const auto query = now - 6'000'000;
  const auto latest = last_timestamp_ns.load();
  xrbridge_stats stats{};
  (void)xrbridge_get_stats(&stats);
  std::ostringstream stream;
  stream << "{\"runtime\":\"XRBridge C++20 / C ABI\",\"producer_hz\":90,\"paused\":"
         << (paused.load() ? "true" : "false") << ",\"sample_age_ms\":" << std::fixed
         << std::setprecision(2)
         << (latest == 0 ? 0.0 : static_cast<double>(now - latest) / 1'000'000.0)
         << ",\"event_sequence\":" << event_sequence.load() << ",\"last_event\":\""
         << json_escape(current_event()) << "\",\"stats\":{\"submitted\":" << stats.submitted
         << ",\"queries\":" << stats.queries << ",\"interpolated\":" << stats.interpolated
         << ",\"rejected_invalid\":" << stats.rejected_invalid
         << ",\"rejected_out_of_order\":" << stats.rejected_out_of_order
         << ",\"rejected_duplicate\":" << stats.rejected_duplicate
         << ",\"dropped_overflow\":" << stats.dropped_overflow
         << ",\"stale_queries\":" << stats.stale_queries
         << ",\"unavailable_queries\":" << stats.unavailable_queries << "},\"poses\":["
         << pose_json(XRBRIDGE_DEVICE_HEAD, query) << ','
         << pose_json(XRBRIDGE_DEVICE_LEFT_CONTROLLER, query) << ','
         << pose_json(XRBRIDGE_DEVICE_RIGHT_CONTROLLER, query) << "]}";
  return stream.str();
}

std::string inject(std::string_view kind) {
  const auto latest = last_timestamp_ns.load();
  auto pose = synthetic_pose(XRBRIDGE_DEVICE_HEAD, 0.0, latest);
  xrbridge_result result = XRBRIDGE_INVALID_ARGUMENT;
  if (kind == "duplicate") {
    result = xrbridge_submit_openxr_pose(XRBRIDGE_DEVICE_HEAD, &pose);
  } else if (kind == "out-of-order") {
    pose.timestamp_ns = latest - 1;
    result = xrbridge_submit_openxr_pose(XRBRIDGE_DEVICE_HEAD, &pose);
  } else if (kind == "invalid") {
    pose.timestamp_ns = latest + 1;
    pose.rotation = {0.0, 0.0, 0.0, 0.0};
    result = xrbridge_submit_openxr_pose(XRBRIDGE_DEVICE_HEAD, &pose);
  } else if (kind == "overflow") {
    auto timestamp = latest;
    for (int sample = 0; sample < 420; ++sample) {
      ++timestamp;
      const auto burst_pose = synthetic_pose(XRBRIDGE_DEVICE_HEAD, sample * 0.001, timestamp);
      result = xrbridge_submit_openxr_pose(XRBRIDGE_DEVICE_HEAD, &burst_pose);
    }
    last_timestamp_ns.store(timestamp);
  }
  const std::string message = std::string(kind) + " -> " + xrbridge_result_string(result);
  set_event(message);
  return "{\"action\":\"" + std::string(kind) + "\",\"result\":\"" +
         xrbridge_result_string(result) + "\"}";
}

std::string read_file(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void close_socket(socket_handle socket) {
#if defined(_WIN32)
  closesocket(socket);
#else
  close(socket);
#endif
}

bool send_all(socket_handle client, const std::string& payload) {
  std::size_t sent = 0;
  while (sent < payload.size()) {
#if defined(_WIN32)
    const int count = send(client, payload.data() + sent,
                           static_cast<int>(payload.size() - sent), 0);
#else
    const auto count = send(client, payload.data() + sent, payload.size() - sent, 0);
#endif
    if (count <= 0) return false;
    sent += static_cast<std::size_t>(count);
  }
  return true;
}

void respond(socket_handle client, std::string_view status, std::string_view content_type,
             const std::string& body) {
  std::ostringstream headers;
  headers << "HTTP/1.1 " << status << "\r\nContent-Type: " << content_type
          << "\r\nContent-Length: " << body.size()
          << "\r\nCache-Control: no-store\r\nConnection: close\r\n"
             "X-Content-Type-Options: nosniff\r\n\r\n";
  (void)send_all(client, headers.str() + body);
}

void handle_client(socket_handle client, const std::string& dashboard) {
  std::array<char, 8192> buffer{};
#if defined(_WIN32)
  const int count = recv(client, buffer.data(), static_cast<int>(buffer.size() - 1), 0);
#else
  const auto count = recv(client, buffer.data(), buffer.size() - 1, 0);
#endif
  if (count <= 0) return;
  const std::string request(buffer.data(), static_cast<std::size_t>(count));
  const auto first_space = request.find(' ');
  const auto second_space = request.find(' ', first_space + 1);
  if (first_space == std::string::npos || second_space == std::string::npos) {
    respond(client, "400 Bad Request", "text/plain", "bad request");
    return;
  }
  const auto method = request.substr(0, first_space);
  auto path = request.substr(first_space + 1, second_space - first_space - 1);
  if (const auto query = path.find('?'); query != std::string::npos) path.resize(query);
  if (method == "GET" && path == "/") {
    respond(client, "200 OK", "text/html; charset=utf-8", dashboard);
  } else if (method == "GET" && path == "/api/frame") {
    respond(client, "200 OK", "application/json", frame_json());
  } else if (method == "GET" && path == "/healthz") {
    respond(client, "200 OK", "application/json", "{\"status\":\"ok\"}");
  } else if (method == "POST" && path == "/api/control/pause") {
    const bool state = !paused.load();
    paused.store(state);
    set_event(state ? "90 Hz producer paused; watch sample age become stale"
                    : "90 Hz producer resumed");
    respond(client, "200 OK", "application/json",
            std::string("{\"paused\":") + (state ? "true}" : "false}"));
  } else if (method == "POST" && path.starts_with("/api/inject/")) {
    const auto kind = path.substr(std::string("/api/inject/").size());
    if (kind == "duplicate" || kind == "out-of-order" || kind == "invalid" ||
        kind == "overflow") {
      respond(client, "200 OK", "application/json", inject(kind));
    } else {
      respond(client, "404 Not Found", "application/json", "{\"error\":\"unknown fault\"}");
    }
  } else {
    respond(client, "404 Not Found", "text/plain", "not found");
  }
}

int run_self_test() {
  const xrbridge_config config{8, 250'000'000};
  if (xrbridge_initialize(&config) != XRBRIDGE_OK) return 1;
  const auto base = now_ns();
  for (int index = 0; index < 20; ++index) {
    for (int value = XRBRIDGE_DEVICE_HEAD; value <= XRBRIDGE_DEVICE_RIGHT_CONTROLLER; ++value) {
      const auto pose = synthetic_pose(static_cast<xrbridge_device>(value), index * 0.01,
                                       base + index * 10'000'000);
      if (xrbridge_submit_openxr_pose(static_cast<xrbridge_device>(value), &pose) != XRBRIDGE_OK)
        return 2;
    }
  }
  xrbridge_pose output{};
  if (xrbridge_query_unity_pose(XRBRIDGE_DEVICE_HEAD, base + 185'000'000, &output) != XRBRIDGE_OK)
    return 3;
  xrbridge_stats stats{};
  if (xrbridge_get_stats(&stats) != XRBRIDGE_OK || stats.interpolated != 1 ||
      stats.dropped_overflow != 36)
    return 4;
  return xrbridge_shutdown() == XRBRIDGE_OK ? 0 : 5;
}

}  // namespace

int main(int argc, char** argv) {
  int port = 8080;
  std::string assets = XRBRIDGE_DEMO_ASSET_DIR;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--self-test") return run_self_test();
    if (argument == "--port" && index + 1 < argc) port = std::stoi(argv[++index]);
    if (argument == "--assets" && index + 1 < argc) assets = argv[++index];
  }
  if (const char* environment_port = std::getenv("PORT")) port = std::stoi(environment_port);
  const auto dashboard = read_file(assets + "/index.html");
  if (dashboard.empty()) {
    std::cerr << "Could not load " << assets << "/index.html\n";
    return 1;
  }
  const xrbridge_config config{128, 250'000'000};
  if (xrbridge_initialize(&config) != XRBRIDGE_OK) {
    std::cerr << "XRBridge initialization failed: " << xrbridge_last_error() << '\n';
    return 1;
  }
#if defined(_WIN32)
  WSADATA winsock{};
  if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) return 1;
#endif
  const socket_handle server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == invalid_socket) return 1;
  int reuse = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(static_cast<std::uint16_t>(port));
  if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
      listen(server, 32) != 0) {
    std::cerr << "Could not listen on port " << port << '\n';
    close_socket(server);
    return 1;
  }
  std::thread producer(producer_loop);
  std::cout << "XRBridge live native demo: http://0.0.0.0:" << port << '\n';
  while (running.load()) {
    sockaddr_in client_address{};
#if defined(_WIN32)
    int length = sizeof(client_address);
#else
    socklen_t length = sizeof(client_address);
#endif
    const auto client = accept(server, reinterpret_cast<sockaddr*>(&client_address), &length);
    if (client == invalid_socket) continue;
    handle_client(client, dashboard);
    close_socket(client);
  }
  producer.join();
  close_socket(server);
  (void)xrbridge_shutdown();
#if defined(_WIN32)
  WSACleanup();
#endif
  return 0;
}
