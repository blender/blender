/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_shader_log.hh"

#include "GPU_platform.hh"

namespace blender::gpu {

const char *VKLogParser::parse_line(const char *source_combined,
                                    const char *log_line,
                                    GPULogItem &log_item)
{
  /* Shader name. */
  log_line = skip_name(log_line);
  log_line = skip_separators(log_line, ":");

  /* Parse error line & char numbers. */
  if (at_number(log_line)) {
    const char *error_line_number_end;
    log_item.cursor.row = parse_number(log_line, &error_line_number_end);
    log_line = error_line_number_end;
    log_line = skip_separators(log_line, ": ");
  }
  if (at_number(log_line)) {
    const char *number_end;
    log_item.cursor.column = parse_number(log_line, &number_end);
    log_line = number_end;
  }
  log_line = skip_separators(log_line, ": ");

  /* Skip to message. Avoid redundant info. */
  log_line = skip_severity_keyword(log_line, log_item);
  log_line = skip_separators(log_line, ": ");

  if (log_item.cursor.row != -1) {
    /* Get to the wanted line. */
    size_t line_start_character = line_start_get(source_combined, log_item.cursor.row);
    StringRef filename = filename_get(source_combined, line_start_character);
    size_t line_number = source_line_get(source_combined, line_start_character);
    log_item.cursor.file_name_and_error_line = std::string(filename) + ':' +
                                               std::to_string(line_number);
    if (log_item.cursor.column != -1) {
      log_item.cursor.file_name_and_error_line += ':' + std::to_string(log_item.cursor.column + 1);
    }
  }

  return log_line;
}

const char *VKLogParser::skip_name(const char *log_line)
{
  return skip_until(log_line, ':');
}

const char *VKLogParser::skip_severity_keyword(const char *log_line, GPULogItem &log_item)
{
  return skip_severity(log_line, log_item, "error", "warning", "note");
}

}  // namespace blender::gpu
