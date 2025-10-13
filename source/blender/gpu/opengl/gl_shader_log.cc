/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gl_shader.hh"

#include "GPU_platform.hh"

namespace blender::gpu {

const char *GLLogParser::parse_line(const char *source_combined,
                                    const char *log_line,
                                    GPULogItem &log_item)
{
  /* Skip ERROR: or WARNING:. */
  log_line = skip_severity_prefix(log_line, log_item);
  log_line = skip_separators(log_line, "(: ");

  /* Parse error line & char numbers. */
  if (at_number(log_line)) {
    const char *error_line_number_end;
    log_item.cursor.row = parse_number(log_line, &error_line_number_end);
    /* Try to fetch the error character (not always available). */
    if (at_any(error_line_number_end, "(:") && at_number(&error_line_number_end[1])) {
      log_item.cursor.column = parse_number(error_line_number_end + 1, &log_line);
    }
    else {
      log_line = error_line_number_end;
    }
    /* There can be a 3rd number (case of mesa driver). */
    if (at_any(log_line, "(:") && at_number(&log_line[1])) {
      log_item.cursor.source = log_item.cursor.row;
      log_item.cursor.row = log_item.cursor.column;
      log_item.cursor.column = parse_number(log_line + 1, &error_line_number_end);
      log_line = error_line_number_end;
    }
  }

  if ((log_item.cursor.row != -1) && (log_item.cursor.column != -1)) {
    if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_OFFICIAL)) {
      /* source:row */
      log_item.cursor.source = log_item.cursor.row;
      log_item.cursor.row = log_item.cursor.column;
      log_item.cursor.column = -1;
    }
    else if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OFFICIAL)) {
      /* source:row */
      log_item.cursor.source = log_item.cursor.row;
      log_item.cursor.row = log_item.cursor.column;
      log_item.cursor.column = -1;
    }
    else {
      /* line:char */
    }
  }

  if (log_item.cursor.row != -1) {
    /* Get to the wanted line. */
    size_t line_start_character = line_start_get(source_combined, log_item.cursor.row);
    if (line_start_character != -1) {
      StringRef filename = filename_get(source_combined, line_start_character);
      size_t line_number = source_line_get(source_combined, line_start_character);
      log_item.cursor.file_name_and_error_line = std::string(filename) + ':' +
                                                 std::to_string(line_number);
      if (log_item.cursor.column != -1) {
        log_item.cursor.file_name_and_error_line += ':' +
                                                    std::to_string(log_item.cursor.column + 1);
      }
    }
  }

  log_line = skip_separators(log_line, ":) ");

  /* Skip to message. Avoid redundant info. */
  log_line = skip_severity_keyword(log_line, log_item);
  log_line = skip_separators(log_line, ":) ");

  return log_line;
}

const char *GLLogParser::skip_severity_prefix(const char *log_line, GPULogItem &log_item)
{
  return skip_severity(log_line, log_item, "ERROR", "WARNING", "NOTE");
}

const char *GLLogParser::skip_severity_keyword(const char *log_line, GPULogItem &log_item)
{
  return skip_severity(log_line, log_item, "error", "warning", "note");
}

}  // namespace blender::gpu
