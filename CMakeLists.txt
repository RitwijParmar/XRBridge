cmake_minimum_required(VERSION 3.24)
project(XRBridge VERSION 1.0.0 LANGUAGES CXX)

option(XRBRIDGE_BUILD_TESTS "Build GoogleTest test suite" ON)
option(XRBRIDGE_BUILD_BENCHMARK "Build deterministic benchmark" ON)
option(XRBRIDGE_BUILD_DEMO "Build the live native runtime demo" ON)
option(XRBRIDGE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(XRBRIDGE_WITH_OPENXR "Compile the official OpenXR header adapter" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(xrbridge_core STATIC src/math.cpp src/pose_buffer.cpp)
add_library(XRBridge::core ALIAS xrbridge_core)
set_target_properties(xrbridge_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(xrbridge_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

add_library(xrbridge SHARED src/c_api.cpp)
add_library(XRBridge::xrbridge ALIAS xrbridge)
target_include_directories(xrbridge PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_definitions(xrbridge PRIVATE XRBRIDGE_BUILDING_LIBRARY)
target_link_libraries(xrbridge PRIVATE XRBridge::core)
if(WIN32)
  set_target_properties(xrbridge PROPERTIES PREFIX "")
endif()

if(MSVC)
  target_compile_options(xrbridge_core PRIVATE /W4 /permissive-)
  target_compile_options(xrbridge PRIVATE /W4 /permissive-)
else()
  target_compile_options(xrbridge_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  target_compile_options(xrbridge PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(XRBRIDGE_WITH_OPENXR)
  find_package(OpenXR CONFIG REQUIRED)
  target_compile_definitions(xrbridge_core PUBLIC XRBRIDGE_WITH_OPENXR)
  target_link_libraries(xrbridge_core PUBLIC OpenXR::headers)
endif()

if(XRBRIDGE_ENABLE_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT WIN32)
    target_compile_options(xrbridge_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(xrbridge PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(xrbridge_core INTERFACE -fsanitize=address,undefined)
    target_link_options(xrbridge PUBLIC -fsanitize=address,undefined)
  else()
    message(FATAL_ERROR "XRBRIDGE_ENABLE_SANITIZERS requires Clang or GCC on a non-Windows host")
  endif()
endif()

if(XRBRIDGE_BUILD_BENCHMARK)
  add_executable(xrbridge_benchmark bench/benchmark.cpp)
  target_link_libraries(xrbridge_benchmark PRIVATE XRBridge::core)
endif()

if(XRBRIDGE_BUILD_DEMO)
  add_executable(xrbridge_demo demo/xrbridge_demo_server.cpp)
  target_link_libraries(xrbridge_demo PRIVATE XRBridge::xrbridge)
  target_compile_definitions(xrbridge_demo PRIVATE
    XRBRIDGE_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo/web")
  if(WIN32)
    target_link_libraries(xrbridge_demo PRIVATE ws2_32)
  elseif(UNIX)
    target_link_libraries(xrbridge_demo PRIVATE pthread)
  endif()
  if(MSVC)
    target_compile_options(xrbridge_demo PRIVATE /W4 /permissive-)
  else()
    target_compile_options(xrbridge_demo PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  endif()
endif()

if(XRBRIDGE_BUILD_TESTS)
  include(CTest)
  include(FetchContent)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926)
  FetchContent_MakeAvailable(googletest)
  add_executable(xrbridge_tests tests/xrbridge_tests.cpp)
  target_link_libraries(xrbridge_tests PRIVATE XRBridge::core XRBridge::xrbridge GTest::gtest_main)
  if(UNIX)
    target_link_libraries(xrbridge_tests PRIVATE pthread)
  endif()
  include(GoogleTest)
  gtest_discover_tests(xrbridge_tests)
  if(XRBRIDGE_BUILD_DEMO)
    add_test(NAME demo_self_test COMMAND xrbridge_demo --self-test)
  endif()
endif()

include(GNUInstallDirs)
install(TARGETS xrbridge xrbridge_core EXPORT XRBridgeTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
