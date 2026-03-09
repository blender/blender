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
    log_line = error_line_number_end;
    /* Simply copy the start of the error line since it is already in the format we want. */
    log_item.cursor.file_name_and_error_line = StringRef(name_start, error_line_number_end);

    StringRef source_name(name_start, name_end);
    if (log_item.cursor.row != -1) {
      /* Get to the wanted line. */
      size_t line_start_character = line_start_get(source_combined, log_item.cursor.row);
      StringRef filename = filename_get(source_combined, line_start_character);
      size_t line_number = source_line_get(source_combined, line_start_character);
      log_item.cursor.file_name_and_error_line = std::string(filename) + ':' +
                                                 std::to_string(line_number);
      if (log_item.cursor.column != -1) {
        log_item.cursor.file_name_and_error_line += ':' + std::to_string(log_item.cursor.column);
        log_item.cursor.column -= 1; /* Caret printing expect 0 based index. */
      }
      parsed_error_ = true;
    }
    return log_line;
  }

  if (parsed_error_) {
    /* Skip the redundant lines that we be outputted above the error. */
    return skip_line(log_line);
  }

  if (wrapper_error_) {
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
