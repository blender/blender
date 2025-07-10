/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/log.h"
#include "util/math.h"
#include "util/string.h"
#include "util/time.h"

#include <cstdio>
#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

CCL_NAMESPACE_BEGIN

LogLevel LOG_LEVEL = LOG_LEVEL_INFO_IMPORTANT;
static LogFunction LOG_FUNCTION;
static double LOG_START_TIME = time_dt();

const char *log_level_to_string(const LogLevel level)
{
  switch (level) {
    case LOG_LEVEL_FATAL:
    case LOG_LEVEL_DFATAL:
      return "FATAL";
    case LOG_LEVEL_ERROR:
    case LOG_LEVEL_DERROR:
      return "ERROR";
    case LOG_LEVEL_WARNING:
    case LOG_LEVEL_DWARNING:
      return "WARNING";
    case LOG_LEVEL_INFO_IMPORTANT:
    case LOG_LEVEL_INFO:
      return "INFO";
    case LOG_LEVEL_WORK:
      return "WORK";
    case LOG_LEVEL_STATS:
      return "STATS";
    case LOG_LEVEL_DEBUG:
      return "DEBUG";
    case LOG_LEVEL_UNKNOWN:
      return "UNKNOWN";
  }

  return "";
}

LogLevel log_string_to_level(const string &str)
{
  const std::string str_lower = string_to_lower(str);

  if (str_lower == "fatal") {
    return LOG_LEVEL_FATAL;
  }
  if (str_lower == "error") {
    return LOG_LEVEL_ERROR;
  }
  if (str_lower == "warning") {
    return LOG_LEVEL_WARNING;
  }
  if (str_lower == "info") {
    return LOG_LEVEL_INFO;
  }
  if (str_lower == "work") {
    return LOG_LEVEL_WORK;
  }
  if (str_lower == "stats") {
    return LOG_LEVEL_STATS;
  }
  if (str_lower == "debug") {
    return LOG_LEVEL_DEBUG;
  }
  return LOG_LEVEL_UNKNOWN;
}

void log_init(const LogFunction func)
{
  LOG_FUNCTION = func;
  LOG_START_TIME = time_dt();
}

void log_level_set(const LogLevel level)
{
  LOG_LEVEL = level;
}

void log_level_set(const std::string &level)
{
  const LogLevel new_level = log_string_to_level(level);
  if (new_level == LOG_LEVEL_UNKNOWN) {
    LOG_ERROR << "Unknown log level specified: " << level;
    return;
  }
  LOG_LEVEL = new_level;
}

static void log_default(const LogLevel level, const std::string &time_str, const char *msg)
{
  if (level >= LOG_LEVEL_INFO) {
    printf("%s | %s\n", time_str.c_str(), msg);
  }
  else {
    fflush(stdout);
    fprintf(stderr, "%s | %s: %s\n", time_str.c_str(), log_level_to_string(level), msg);
  }
}

void _log_message(const LogLevel level, const char *file_line, const char *func, const char *msg)
{
  assert(level <= LOG_LEVEL);

  if (LOG_FUNCTION) {
    LOG_FUNCTION(level, file_line, func, msg);
    return;
  }

  const std::string time_str = time_human_readable_from_seconds(time_dt() - LOG_START_TIME);

  if (strchr(msg, '\n') == nullptr) {
    log_default(level, time_str, msg);
    return;
  }

  vector<string> lines;
  string_split(lines, msg, "\n", false);
  for (const string &line : lines) {
    log_default(level, time_str, line.c_str());
  }

  if (level == LOG_LEVEL_FATAL || level == LOG_LEVEL_DFATAL) {
    abort();
  }
}

std::ostream &operator<<(std::ostream &os, const int2 &value)
{
  os << "(" << value.x << ", " << value.y << ")";
  return os;
}

std::ostream &operator<<(std::ostream &os, const float3 &value)
{
  os << "(" << value.x << ", " << value.y << ", " << value.z << ")";
  return os;
}

CCL_NAMESPACE_END
