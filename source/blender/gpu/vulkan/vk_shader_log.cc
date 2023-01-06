/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_shader_log.hh"

#include "GPU_platform.h"

namespace blender::gpu {

char *VKLogParser::parse_line(char *log_line, GPULogItem &log_item)
{
  log_line = skip_name(log_line);
  log_line = skip_separators(log_line, ":");

  /* Parse error line & char numbers. */
  if (at_number(log_line)) {
    char *error_line_number_end;
    log_item.cursor.row = parse_number(log_line, &error_line_number_end);
    log_line = error_line_number_end;
  }
  log_line = skip_separators(log_line, ": ");

  /* Skip to message. Avoid redundant info. */
  log_line = skip_severity_keyword(log_line, log_item);
  log_line = skip_separators(log_line, ": ");

  return log_line;
}

char *VKLogParser::skip_name(char *log_line)
{
  return skip_until(log_line, ':');
}

char *VKLogParser::skip_severity_keyword(char *log_line, GPULogItem &log_item)
{
  return skip_severity(log_line, log_item, "error", "warning");
}

}  // namespace blender::gpu
