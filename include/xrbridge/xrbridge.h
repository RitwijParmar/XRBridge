#pragma once

#include <stdint.h>

#if defined(_WIN32)
#  if defined(XRBRIDGE_BUILDING_LIBRARY)
#    define XRBRIDGE_API __declspec(dllexport)
#  else
#    define XRBRIDGE_API __declspec(dllimport)
#  endif
#else
#  define XRBRIDGE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum xrbridge_result {
  XRBRIDGE_OK = 0,
  XRBRIDGE_NOT_INITIALIZED = 1,
  XRBRIDGE_INVALID_ARGUMENT = 2,
  XRBRIDGE_INVALID_QUATERNION = 3,
  XRBRIDGE_OUT_OF_ORDER = 4,
  XRBRIDGE_DUPLICATE = 5,
  XRBRIDGE_UNAVAILABLE = 6,
  XRBRIDGE_STALE = 7,
  XRBRIDGE_INTERNAL_ERROR = 8
} xrbridge_result;

typedef enum xrbridge_device {
  XRBRIDGE_DEVICE_HEAD = 0,
  XRBRIDGE_DEVICE_LEFT_CONTROLLER = 1,
  XRBRIDGE_DEVICE_RIGHT_CONTROLLER = 2
} xrbridge_device;

typedef struct xrbridge_vec3 {
  double x;
  double y;
  double z;
} xrbridge_vec3;

typedef struct xrbridge_quaternion {
  double x;
  double y;
  double z;
  double w;
} xrbridge_quaternion;

typedef struct xrbridge_pose {
  xrbridge_vec3 position;
  xrbridge_quaternion rotation;
  int64_t timestamp_ns;
} xrbridge_pose;

typedef struct xrbridge_config {
  uint32_t capacity_per_device;
  int64_t max_sample_age_ns;
} xrbridge_config;

typedef struct xrbridge_stats {
  uint64_t submitted;
  uint64_t queries;
  uint64_t interpolated;
  uint64_t rejected_invalid;
  uint64_t rejected_out_of_order;
  uint64_t rejected_duplicate;
  uint64_t dropped_overflow;
  uint64_t stale_queries;
  uint64_t unavailable_queries;
} xrbridge_stats;

XRBRIDGE_API xrbridge_result xrbridge_initialize(const xrbridge_config* config);
XRBRIDGE_API xrbridge_result xrbridge_shutdown(void);
XRBRIDGE_API xrbridge_result xrbridge_submit_openxr_pose(xrbridge_device device,
                                                          const xrbridge_pose* pose);
XRBRIDGE_API xrbridge_result xrbridge_query_unity_pose(xrbridge_device device,
                                                        int64_t timestamp_ns,
                                                        xrbridge_pose* output);
XRBRIDGE_API xrbridge_result xrbridge_get_stats(xrbridge_stats* output);
XRBRIDGE_API const char* xrbridge_result_string(xrbridge_result result);
XRBRIDGE_API const char* xrbridge_last_error(void);

#ifdef __cplusplus
}
#endif
