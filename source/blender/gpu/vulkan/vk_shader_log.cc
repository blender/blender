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
  if (at_number(log_line)) {
    /* Parse error file. */
    const char *error_line_number_end;
    log_item.cursor.source = parse_number(log_line, &error_line_number_end);
    log_line = error_line_number_end;
  }

  log_line = skip_separators(log_line, ":");

  /* Parse error line & char numbers. */
  if (at_number(log_line)) {
    const char *error_line_number_end;
    log_item.cursor.row = parse_number(log_line, &error_line_number_end);
    log_line = error_line_number_end;
  }
  log_line = skip_separators(log_line, ": ");

  /* Skip to message. Avoid redundant info. */
  log_line = skip_severity_keyword(log_line, log_item);
  log_line = skip_separators(log_line, ": ");

  /* TODO: Temporary fix for new line directive. Eventually this whole parsing should be done in
   * C++ with regex for simplicity. */
  if (log_item.cursor.source != -1) {
    StringRefNull src(source_combined);
    std::string needle = std::string("#line 1 ") + std::to_string(log_item.cursor.source);

    int64_t file_start = src.find(needle);
    if (file_start == -1) {
      /* Can be generated code or wrapper code outside of the main sources. */
      log_item.cursor.row = -1;
    }
    else {
      StringRef previous_sources(source_combined, file_start);
      for (const char c : previous_sources) {
        if (c == '\n') {
          log_item.cursor.row++;
        }
      }
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
