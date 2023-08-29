/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_string_ref.hh"

#include "GPU_platform.h"

#include "mtl_shader_log.hh"

namespace blender::gpu {

const char *MTLLogParser::parse_line(const char *log_line, GPULogItem &log_item)
{
  log_line = skip_name(log_line);
  log_line = skip_separators(log_line, ":");

  /* Parse error line & char numbers. */
  if (at_number(log_line)) {
    const char *error_line_number_end;

    log_item.cursor.row = parse_number(log_line, &error_line_number_end);
    log_line = error_line_number_end;
    log_line = skip_separators(log_line, ": ");
    log_item.cursor.column = parse_number(log_line, &error_line_number_end);
    log_line = error_line_number_end;
  }
  log_line = skip_separators(log_line, ": ");

  /* Skip to message. Avoid redundant info. */
  log_line = skip_severity_keyword(log_line, log_item);
  log_line = skip_separators(log_line, ": ");

  return log_line;
}

const char *MTLLogParser::skip_name(const char *log_line)
{
  return skip_until(log_line, ':');
}

const char *MTLLogParser::skip_severity_keyword(const char *log_line, GPULogItem &log_item)
{
  return skip_severity(log_line, log_item, "error", "warning", "note");
}

}  // namespace blender::gpu
