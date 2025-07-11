/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/CCL_api.h"
#include "util/log.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"cycles"};

void CCL_log_init()
{
  /* Set callback to pass log messages to CLOG. */
  ccl::log_init(
      [](const ccl::LogLevel level, const char *file_line, const char *func, const char *msg) {
        const CLG_LogType *log_type = CLOG_ENSURE(&LOG);
        switch (level) {
          case ccl::LOG_LEVEL_FATAL:
          case ccl::LOG_LEVEL_DFATAL:
            CLG_log_str(log_type, CLG_LEVEL_FATAL, file_line, func, msg);
            return;
          case ccl::LOG_LEVEL_ERROR:
          case ccl::LOG_LEVEL_DERROR:
            CLG_log_str(log_type, CLG_LEVEL_ERROR, file_line, func, msg);
            return;
          case ccl::LOG_LEVEL_WARNING:
          case ccl::LOG_LEVEL_DWARNING:
            CLG_log_str(log_type, CLG_LEVEL_WARN, file_line, func, msg);
            return;
          case ccl::LOG_LEVEL_INFO:
          case ccl::LOG_LEVEL_INFO_IMPORTANT:
          case ccl::LOG_LEVEL_WORK:
          case ccl::LOG_LEVEL_STATS:
          case ccl::LOG_LEVEL_DEBUG:
          case ccl::LOG_LEVEL_UNKNOWN:
            CLG_log_str(log_type, CLG_LEVEL_INFO, file_line, func, msg);
            return;
        }
      });

  /* Map log level from CLOG. */
  const CLG_LogType *log_type = CLOG_ENSURE(&LOG);
  switch (log_type->level) {
    case CLG_LEVEL_FATAL:
      ccl::log_level_set(ccl::LOG_LEVEL_FATAL);
      break;
    case CLG_LEVEL_ERROR:
      ccl::log_level_set(ccl::LOG_LEVEL_ERROR);
      break;
    case CLG_LEVEL_WARN:
      ccl::log_level_set(ccl::LOG_LEVEL_WARNING);
      break;
    case CLG_LEVEL_INFO:
      ccl::log_level_set(ccl::LOG_LEVEL_INFO);
      break;
    case CLG_LEVEL_DEBUG:
      ccl::log_level_set(ccl::LOG_LEVEL_WORK);
      break;
    case CLG_LEVEL_TRACE:
      ccl::log_level_set(ccl::LOG_LEVEL_DEBUG);
      break;
  }
}
