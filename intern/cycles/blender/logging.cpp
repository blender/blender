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
          case ccl::FATAL:
          case ccl::DFATAL:
            CLG_log_str(log_type, CLG_LEVEL_FATAL, file_line, func, msg);
            return;
          case ccl::ERROR:
          case ccl::DERROR:
            CLG_log_str(log_type, CLG_LEVEL_ERROR, file_line, func, msg);
            return;
          case ccl::WARNING:
          case ccl::DWARNING:
            CLG_log_str(log_type, CLG_LEVEL_WARN, file_line, func, msg);
            return;
          case ccl::INFO:
          case ccl::INFO_IMPORTANT:
          case ccl::WORK:
          case ccl::STATS:
          case ccl::DEBUG:
          case ccl::UNKNOWN:
            CLG_log_str(log_type, CLG_LEVEL_INFO, file_line, func, msg);
            return;
        }
      });

  /* Map log level from CLOG. */
  const CLG_LogType *log_type = CLOG_ENSURE(&LOG);
  switch (log_type->level) {
    case CLG_LEVEL_FATAL:
      ccl::log_level_set(ccl::FATAL);
      break;
    case CLG_LEVEL_ERROR:
      ccl::log_level_set(ccl::ERROR);
      break;
    case CLG_LEVEL_WARN:
      ccl::log_level_set(ccl::WARNING);
      break;
    case CLG_LEVEL_INFO:
      ccl::log_level_set(ccl::INFO);
      break;
    case CLG_LEVEL_DEBUG:
      ccl::log_level_set(ccl::WORK);
      break;
    case CLG_LEVEL_TRACE:
      ccl::log_level_set(ccl::DEBUG);
      break;
  }
}
