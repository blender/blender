/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_LOGGING_H__
#define __UTIL_LOGGING_H__

#if defined(WITH_CYCLES_LOGGING) && !defined(__KERNEL_GPU__)
#  include <gflags/gflags.h>
#  include <glog/logging.h>
#endif

#include <iostream>

CCL_NAMESPACE_BEGIN

#if !defined(WITH_CYCLES_LOGGING) || defined(__KERNEL_GPU__)
class StubStream {
 public:
  template<class T> StubStream &operator<<(const T &)
  {
    return *this;
  }
};

class LogMessageVoidify {
 public:
  LogMessageVoidify() {}
  void operator&(const StubStream &) {}
};

#  define LOG_SUPPRESS() (true) ? ((void)0) : LogMessageVoidify() & StubStream()
#  define LOG(severity) LOG_SUPPRESS()
#  define VLOG(severity) LOG_SUPPRESS()
#  define VLOG_IF(severity, condition) LOG_SUPPRESS()
#  define VLOG_IS_ON(severity) false

#  define CHECK(expression) LOG_SUPPRESS()

#  define CHECK_NOTNULL(expression) (expression)

#  define CHECK_NEAR(actual, expected, eps) LOG_SUPPRESS()

#  define CHECK_GE(a, b) LOG_SUPPRESS()
#  define CHECK_NE(a, b) LOG_SUPPRESS()
#  define CHECK_EQ(a, b) LOG_SUPPRESS()
#  define CHECK_GT(a, b) LOG_SUPPRESS()
#  define CHECK_LT(a, b) LOG_SUPPRESS()
#  define CHECK_LE(a, b) LOG_SUPPRESS()

#  define DCHECK(expression) LOG_SUPPRESS()

#  define DCHECK_NOTNULL(expression) (expression)

#  define DCHECK_NEAR(actual, expected, eps) LOG_SUPPRESS()

#  define DCHECK_GE(a, b) LOG_SUPPRESS()
#  define DCHECK_NE(a, b) LOG_SUPPRESS()
#  define DCHECK_EQ(a, b) LOG_SUPPRESS()
#  define DCHECK_GT(a, b) LOG_SUPPRESS()
#  define DCHECK_LT(a, b) LOG_SUPPRESS()
#  define DCHECK_LE(a, b) LOG_SUPPRESS()

#  define LOG_ASSERT(expression) LOG_SUPPRESS()
#endif

/* Verbose logging categories. */

/* Warnings. */
#define VLOG_WARNING VLOG(1)
/* Info about devices, scene contents and features used. */
#define VLOG_INFO VLOG(2)
#define VLOG_INFO_IS_ON VLOG_IS_ON(2)
/* Work being performed and timing/memory stats about that work. */
#define VLOG_WORK VLOG(3)
#define VLOG_WORK_IS_ON VLOG_IS_ON(3)
/* Detailed device timing stats. */
#define VLOG_DEVICE_STATS VLOG(4)
#define VLOG_DEVICE_STATS_IS_ON VLOG_IS_ON(4)
/* Verbose debug messages. */
#define VLOG_DEBUG VLOG(5)
#define VLOG_DEBUG_IS_ON VLOG_IS_ON(5)

struct int2;
struct float3;

void util_logging_init(const char *argv0);
void util_logging_start();
void util_logging_verbosity_set(int verbosity);

std::ostream &operator<<(std::ostream &os, const int2 &value);
std::ostream &operator<<(std::ostream &os, const float3 &value);

CCL_NAMESPACE_END

#endif /* __UTIL_LOGGING_H__ */
