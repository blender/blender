/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "BLI_dynstr.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "gpu_shader_private.hh"

#include "GPU_platform.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"gpu.shader"};

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Debug functions
 * \{ */

void Shader::print_log(Span<const char *> sources,
                       char *log,
                       const char *stage,
                       const bool error,
                       GPULogParser *parser)
{
  const char line_prefix[] = "      | ";
  char err_col[] = "\033[31;1m";
  char warn_col[] = "\033[33;1m";
  char info_col[] = "\033[0;2m";
  char reset_col[] = "\033[0;0m";
  char *sources_combined = BLI_string_join_arrayN((const char **)sources.data(), sources.size());
  DynStr *dynstr = BLI_dynstr_new();

  if (!CLG_color_support_get(&LOG)) {
    err_col[0] = warn_col[0] = info_col[0] = reset_col[0] = '\0';
  }

  BLI_dynstr_appendf(dynstr, "\n");

  char *log_line = log, *line_end;

  LogCursor previous_location;

  bool found_line_id = false;
  while ((line_end = strchr(log_line, '\n'))) {
    /* Skip empty lines. */
    if (line_end == log_line) {
      log_line++;
      continue;
    }

    GPULogItem log_item;
    log_line = parser->parse_line(log_line, log_item);

    if (log_item.cursor.row == -1) {
      found_line_id = false;
    }

    const char *src_line = sources_combined;

    /* Separate from previous block. */
    if (previous_location.source != log_item.cursor.source ||
        previous_location.row != log_item.cursor.row) {
      BLI_dynstr_appendf(dynstr, "%s%s%s\n", info_col, line_prefix, reset_col);
    }
    else if (log_item.cursor.column != previous_location.column) {
      BLI_dynstr_appendf(dynstr, "%s\n", line_prefix);
    }
    /* Print line from the source file that is producing the error. */
    if ((log_item.cursor.row != -1) && (log_item.cursor.row != previous_location.row ||
                                        log_item.cursor.column != previous_location.column)) {
      const char *src_line_end;
      found_line_id = false;
      /* error_line is 1 based in this case. */
      int src_line_index = 1;
      while ((src_line_end = strchr(src_line, '\n'))) {
        if (src_line_index == log_item.cursor.row) {
          found_line_id = true;
          break;
        }
/* TODO(fclem) Make this an option to display N lines before error. */
#if 0 /* Uncomment to print shader file up to the error line to have more context. */
        BLI_dynstr_appendf(dynstr, "%5d | ", src_line_index);
        BLI_dynstr_nappend(dynstr, src_line, (src_line_end + 1) - src_line);
#endif
        /* Continue to next line. */
        src_line = src_line_end + 1;
        src_line_index++;
      }
      /* Print error source. */
      if (found_line_id) {
        if (log_item.cursor.row != previous_location.row) {
          BLI_dynstr_appendf(dynstr, "%5d | ", src_line_index);
        }
        else {
          BLI_dynstr_appendf(dynstr, line_prefix);
        }
        BLI_dynstr_nappend(dynstr, src_line, (src_line_end + 1) - src_line);
        /* Print char offset. */
        BLI_dynstr_appendf(dynstr, line_prefix);
        if (log_item.cursor.column != -1) {
          for (int i = 0; i < log_item.cursor.column; i++) {
            BLI_dynstr_appendf(dynstr, " ");
          }
          BLI_dynstr_appendf(dynstr, "^");
        }
        BLI_dynstr_appendf(dynstr, "\n");
      }
    }
    BLI_dynstr_appendf(dynstr, line_prefix);

    if (log_item.severity == Severity::Error) {
      BLI_dynstr_appendf(dynstr, "%s%s%s: ", err_col, "Error", info_col);
    }
    else if (log_item.severity == Severity::Error) {
      BLI_dynstr_appendf(dynstr, "%s%s%s: ", warn_col, "Warning", info_col);
    }
    /* Print the error itself. */
    BLI_dynstr_append(dynstr, info_col);
    BLI_dynstr_nappend(dynstr, log_line, (line_end + 1) - log_line);
    BLI_dynstr_append(dynstr, reset_col);
    /* Continue to next line. */
    log_line = line_end + 1;
    previous_location = log_item.cursor;
  }
  MEM_freeN(sources_combined);

  CLG_Severity severity = error ? CLG_SEVERITY_ERROR : CLG_SEVERITY_WARN;

  if (((LOG.type->flag & CLG_FLAG_USE) && (LOG.type->level >= 0)) ||
      (severity >= CLG_SEVERITY_WARN)) {
    const char *_str = BLI_dynstr_get_cstring(dynstr);
    CLG_log_str(LOG.type, severity, this->name, stage, _str);
    MEM_freeN((void *)_str);
  }

  BLI_dynstr_free(dynstr);
}

char *GPULogParser::skip_severity(char *log_line,
                                  GPULogItem &log_item,
                                  const char *error_msg,
                                  const char *warning_msg) const
{
  if (STREQLEN(log_line, error_msg, strlen(error_msg))) {
    log_line += strlen(error_msg);
    log_item.severity = Severity::Error;
  }
  else if (STREQLEN(log_line, warning_msg, strlen(warning_msg))) {
    log_line += strlen(warning_msg);
    log_item.severity = Severity::Warning;
  }
  return log_line;
}

char *GPULogParser::skip_separators(char *log_line, const StringRef separators) const
{
  while (at_any(log_line, separators)) {
    log_line++;
  }
  return log_line;
}

char *GPULogParser::skip_until(char *log_line, char stop_char) const
{
  char *cursor = log_line;
  while (!ELEM(cursor[0], '\n', '\0')) {
    if (cursor[0] == stop_char) {
      return cursor;
    }
    cursor++;
  }
  return log_line;
}

bool GPULogParser::at_number(const char *log_line) const
{
  return log_line[0] >= '0' && log_line[0] <= '9';
}

bool GPULogParser::at_any(const char *log_line, const StringRef chars) const
{
  return chars.find(log_line[0]) != StringRef::not_found;
}

int GPULogParser::parse_number(const char *log_line, char **r_new_position) const
{
  return (int)strtol(log_line, r_new_position, 10);
}

/** \} */

}  // namespace blender::gpu
