/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_string_ref.hh"

#include "GPU_platform.hh"

#include "mtl_shader_log.hh"

namespace blender::gpu {

const char *MTLLogParser::parse_line(const char *source_combined,
                                     const char *log_line,
                                     GPULogItem &log_item)
{
  const char *name_start = log_line;
  log_line = skip_name(log_line);
  const char *name_end = log_line;
  log_line = skip_separators(log_line, ":");

  /* Parse error line & char numbers. */
  if (at_number(log_line)) {
    /* Reset skip line if two errors follow. */
    parsed_error_ = false;
    wrapper_error_ = false;

    const char *error_line_number_end;

    log_item.cursor.row = parse_number(log_line, &error_line_number_end);
    log_line = error_line_number_end;
    log_line = skip_separators(log_line, ": ");
    log_item.cursor.column = parse_number(log_line, &error_line_number_end);
    /* For some reason the column is off by one. */
    log_item.cursor.column--;
    log_line = error_line_number_end;
    /* Simply copy the start of the error line since it is already in the format we want. */
    log_item.cursor.file_name_and_error_line = StringRef(name_start, error_line_number_end);

    StringRef source_name(name_start, name_end);

    if (source_name == "msl_wrapper_code") {
      /* In this case the issue is in the wrapper. We cannot access it.
       * So we still display the internal error lines for some more infos. */
      log_item.cursor.row = -1;
      wrapper_error_ = true;
    }
    else if (!source_name.is_empty()) {
      std::string needle = std::string("#line 1 \"") + source_name + "\"";

      StringRefNull src(source_combined);
      int64_t file_start = src.find(needle);
      if (file_start == -1) {
        /* Can be generated code or wrapper code outside of the main sources.
         * But should be already caught by the above case. */
        log_item.cursor.row = -1;
        wrapper_error_ = true;
      }
      else {
        StringRef previous_sources(source_combined, file_start);
        for (const char c : previous_sources) {
          if (c == '\n') {
            log_item.cursor.row++;
          }
        }
        /* Count the needle end of line too. */
        log_item.cursor.row++;
        parsed_error_ = true;
      }
    }
  }
  else if (parsed_error_) {
    /* Skip the redundant lines that we be outputted above the error. */
    return skip_line(log_line);
  }
  else if (wrapper_error_) {
    /* Display full lines of error in case of wrapper (non parsed) errors.
     * Avoids weirdly aligned '^' and underlined suggestions. */
    return name_start;
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

const char *MTLLogParser::skip_line(const char *cursor) const
{
  while (!ELEM(cursor[0], '\n', '\0')) {
    cursor++;
  }
  return cursor;
}

}  // namespace blender::gpu
