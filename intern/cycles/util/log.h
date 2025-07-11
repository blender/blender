/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"
#include "util/string.h"
#include "util/types.h"

#include <sstream>

CCL_NAMESPACE_BEGIN

/* Log Levels */

enum LogLevel {
  LOG_LEVEL_FATAL = 0,          /* Fatal error, application will abort */
  LOG_LEVEL_DFATAL = 1,         /* Fatal error in debug build only */
  LOG_LEVEL_ERROR = 2,          /* Error */
  LOG_LEVEL_DERROR = 3,         /* Error in debug build only */
  LOG_LEVEL_WARNING = 4,        /* Warning */
  LOG_LEVEL_DWARNING = 5,       /* Warning in debug build only */
  LOG_LEVEL_INFO_IMPORTANT = 6, /* Important info that is printed by default */
  LOG_LEVEL_INFO = 7,           /* Info about devices, scene contents and features used. */
  LOG_LEVEL_WORK = 8,           /* Work being performed and timing/memory stats about that work. */
  LOG_LEVEL_STATS = 9,          /* Detailed device timing stats. */
  LOG_LEVEL_DEBUG = 10,         /* Verbose debug messages. */
  LOG_LEVEL_UNKNOWN = -1,
};

const char *log_level_to_string(const LogLevel level);
LogLevel log_string_to_level(const string &str);

/* Log Configuration API
 *
 * By default will print to stdout and stderr, but a custom log function can be set
 * to change this behavior. */

using LogFunction = void (*)(const LogLevel level,
                             const char *file_line,
                             const char *func,
                             const char *msg);

void log_init(const LogFunction func = nullptr);
void log_level_set(const LogLevel level);
void log_level_set(const string &level);

/* Internal logging API */

void _log_message(const LogLevel level, const char *file_line, const char *func, const char *msg);

class LogMessage {
 public:
  LogMessage(enum LogLevel level, const char *file_line, const char *func)
      : level_(level), file_line_(file_line), func_(func)
  {
  }

  ~LogMessage()
  {
    _log_message(level_, file_line_, func_, stream_.str().c_str());
  }

  std::ostream &stream()
  {
    return stream_;
  }

 protected:
  LogLevel level_;
  const char *file_line_;
  const char *func_;
  std::stringstream stream_;
};

extern LogLevel LOG_LEVEL;

#define LOG_STRINGIFY_APPEND(a, b) "" a #b
#define LOG_STRINGIFY(x) LOG_STRINGIFY_APPEND("", x)

/* Macro to ensure lazy evaluation of both condition and logging text. */
#ifdef NDEBUG
#  define LOG_IF(level, condition) \
    if constexpr (level != LOG_LEVEL_DFATAL && level != LOG_LEVEL_DERROR && \
                  level != LOG_LEVEL_DWARNING) \
      if (UNLIKELY(level <= LOG_LEVEL && (condition))) \
    LogMessage(level, __FILE__ ":" LOG_STRINGIFY(__LINE__), __func__).stream()
#else
#  define LOG_IF(level, condition) \
    if (UNLIKELY(level <= LOG_LEVEL && (condition))) \
    LogMessage(level, __FILE__ ":" LOG_STRINGIFY(__LINE__), __func__).stream()
#endif

/* Log a message at the desired level.
 *
 * Example: LOG_INFO << "This is a log message"; */
#define LOG(level) LOG_IF(level, true)

#define LOG_FATAL LOG(LOG_LEVEL_FATAL)
#define LOG_DFATAL LOG(LOG_LEVEL_DFATAL)
#define LOG_ERROR LOG(LOG_LEVEL_ERROR)
#define LOG_DERROR LOG(LOG_LEVEL_DERROR)
#define LOG_WARNING LOG(LOG_LEVEL_WARNING)
#define LOG_DWARNING LOG(LOG_LEVEL_DWARNING)
#define LOG_INFO_IMPORTANT LOG(LOG_LEVEL_INFO_IMPORTANT)
#define LOG_INFO LOG(LOG_LEVEL_INFO)
#define LOG_WORK LOG(LOG_LEVEL_WORK)
#define LOG_STATS LOG(LOG_LEVEL_STATS)
#define LOG_DEBUG LOG(LOG_LEVEL_DEBUG)

/* Check if logging is enabled, to avoid doing expensive work to compute
 * the logging message. Note that any work to the right of LOG(level) will
 * not be evaulated if logging for that level is disabled. */
#define LOG_IS_ON(level) ((level) <= LOG_LEVEL)

/* Check if expression and conditions hold true, failure will exit the program. */
#define CHECK(expression) LOG_IF(LOG_LEVEL_FATAL, !(expression))
#define CHECK_OP(op, a, b) LOG_IF(LOG_LEVEL_FATAL, !((a)op(b)))
#define CHECK_GE(a, b) CHECK_OP(>=, a, b)
#define CHECK_NE(a, b) CHECK_OP(!=, a, b)
#define CHECK_EQ(a, b) CHECK_OP(==, a, b)
#define CHECK_GT(a, b) CHECK_OP(>, a, b)
#define CHECK_LT(a, b) CHECK_OP(<, a, b)
#define CHECK_LE(a, b) CHECK_OP(<=, a, b)

/* Same checks for expressions and conditions, but only active in debug builds. */
#ifndef NDEBUG
template<typename T> T DCheckNotNull(T &&t, const char *expression)
{
  if (t == nullptr) {
    LOG_FATAL << "Failed " << expression << "is not null";
  }
  return std::forward<T>(t);
}

#  define DCHECK(expression) \
    LOG_IF(LOG_LEVEL_DFATAL, !(expression)) << LOG_STRINGIFY(expression) << " "
#  define DCHECK_NOTNULL(expression) DCheckNotNull(expression, LOG_STRINGIFY(expression))
#  define DCHECK_OP(op, a, b) \
    LOG_IF(LOG_LEVEL_DFATAL, !((a)op(b))) \
        << "Failed " << LOG_STRINGIFY(a) << " (" << a << ") " << LOG_STRINGIFY(op) << " " \
        << LOG_STRINGIFY(b) << " (" << b << ") "
#  define DCHECK_GE(a, b) DCHECK_OP(>=, a, b)
#  define DCHECK_NE(a, b) DCHECK_OP(!=, a, b)
#  define DCHECK_EQ(a, b) DCHECK_OP(==, a, b)
#  define DCHECK_GT(a, b) DCHECK_OP(>, a, b)
#  define DCHECK_LT(a, b) DCHECK_OP(<, a, b)
#  define DCHECK_LE(a, b) DCHECK_OP(<=, a, b)
#else
#  define LOG_SUPPRESS() LOG_IF(LOG_LEVEL_DEBUG, false)
#  define DCHECK(expression) LOG_SUPPRESS()
#  define DCHECK_NOTNULL(expression) (expression)
#  define DCHECK_GE(a, b) LOG_SUPPRESS()
#  define DCHECK_NE(a, b) LOG_SUPPRESS()
#  define DCHECK_EQ(a, b) LOG_SUPPRESS()
#  define DCHECK_GT(a, b) LOG_SUPPRESS()
#  define DCHECK_LT(a, b) LOG_SUPPRESS()
#  define DCHECK_LE(a, b) LOG_SUPPRESS()
#endif

/* Convenient logging of common data structures. */
struct int2;
struct float3;

std::ostream &operator<<(std::ostream &os, const int2 &value);
std::ostream &operator<<(std::ostream &os, const float3 &value);

CCL_NAMESPACE_END
