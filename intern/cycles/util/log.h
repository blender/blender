/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"
#include "util/string.h"
#include "util/types.h"

#include <sstream>

CCL_NAMESPACE_BEGIN

enum LogLevel {
  FATAL = 0,
  DFATAL = 1,
  ERROR = 2,
  DERROR = 3,
  WARNING = 4,
  DWARNING = 5,
  INFO = 6,
  WORK = 7,
  STATS = 8,
  DEBUG = 9,
  UNKNOWN = -1,
};

const char *log_level_to_string(const LogLevel level);
LogLevel log_string_to_level(const string &str);

using LogFunction = void (*)(const LogLevel level,
                             const char *file_line,
                             const char *func,
                             const char *msg);

void log_init(const LogFunction func = nullptr);
void log_level_set(const LogLevel level);
void log_level_set(const string &level);

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

#ifdef NDEBUG
#  define LOG_IF(level, condition) \
    if constexpr (level != DFATAL && level != DERROR && level != DWARNING) \
      if (LIKELY(!(level <= LOG_LEVEL && (condition)))) \
        ; \
      else \
        LogMessage(level, __FILE__ ":" LOG_STRINGIFY(__LINE__), __func__).stream()
#else
#  define LOG_IF(level, condition) \
    if (LIKELY(!(level <= LOG_LEVEL && (condition)))) \
      ; \
    else \
      LogMessage(level, __FILE__ ":" LOG_STRINGIFY(__LINE__), __func__).stream()
#endif

#define LOG(level) LOG_IF(level, true)

#define LOG_IS_ON(level) ((level) <= LOG_LEVEL)

#define CHECK(expression) LOG_IF(FATAL, !(expression))
#define CHECK_OP(op, a, b) LOG_IF(FATAL, !((a)op(b)))
#define CHECK_GE(a, b) CHECK_OP(>=, a, b)
#define CHECK_NE(a, b) CHECK_OP(!=, a, b)
#define CHECK_EQ(a, b) CHECK_OP(==, a, b)
#define CHECK_GT(a, b) CHECK_OP(>, a, b)
#define CHECK_LT(a, b) CHECK_OP(<, a, b)
#define CHECK_LE(a, b) CHECK_OP(<=, a, b)

#ifndef NDEBUG
template<typename T> T DCheckNotNull(T &&t, const char *expression)
{
  if (t == nullptr) {
    LOG(FATAL) << "Failed " << expression << "is not null";
  }
  return std::forward<T>(t);
}

#  define DCHECK(expression) LOG_IF(DFATAL, !(expression)) << LOG_STRINGIFY(expression) << " "
#  define DCHECK_NOTNULL(expression) DCheckNotNull(expression, LOG_STRINGIFY(expression))
#  define DCHECK_OP(op, a, b) \
    LOG_IF(DFATAL, !((a)op(b))) << "Failed " << LOG_STRINGIFY(a) << " (" << a << ") " \
                                << LOG_STRINGIFY(op) << " " << LOG_STRINGIFY(b) << " (" << b \
                                << ") "
#  define DCHECK_GE(a, b) DCHECK_OP(>=, a, b)
#  define DCHECK_NE(a, b) DCHECK_OP(!=, a, b)
#  define DCHECK_EQ(a, b) DCHECK_OP(==, a, b)
#  define DCHECK_GT(a, b) DCHECK_OP(>, a, b)
#  define DCHECK_LT(a, b) DCHECK_OP(<, a, b)
#  define DCHECK_LE(a, b) DCHECK_OP(<=, a, b)
#else
#  define LOG_SUPPRESS() LOG_IF(DEBUG, false)
#  define DCHECK(expression) LOG_SUPPRESS()
#  define DCHECK_NOTNULL(expression) (expression)
#  define DCHECK_GE(a, b) LOG_SUPPRESS()
#  define DCHECK_NE(a, b) LOG_SUPPRESS()
#  define DCHECK_EQ(a, b) LOG_SUPPRESS()
#  define DCHECK_GT(a, b) LOG_SUPPRESS()
#  define DCHECK_LT(a, b) LOG_SUPPRESS()
#  define DCHECK_LE(a, b) LOG_SUPPRESS()
#endif

struct int2;
struct float3;

std::ostream &operator<<(std::ostream &os, const int2 &value);
std::ostream &operator<<(std::ostream &os, const float3 &value);

CCL_NAMESPACE_END
