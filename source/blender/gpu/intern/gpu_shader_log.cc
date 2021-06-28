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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
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

void Shader::print_log(Span<const char *> sources, char *log, const char *stage, const bool error)
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
  char *error_line_number_end;
  int error_line, error_char, last_error_line = -2, last_error_char = -1;
  bool found_line_id = false;
  while ((line_end = strchr(log_line, '\n'))) {
    /* Skip empty lines. */
    if (line_end == log_line) {
      log_line++;
      continue;
    }
    /* 0 = error, 1 = warning. */
    int type = -1;
    /* Skip ERROR: or WARNING:. */
    const char *prefix[] = {"ERROR", "WARNING"};
    for (int i = 0; i < ARRAY_SIZE(prefix); i++) {
      if (STREQLEN(log_line, prefix[i], strlen(prefix[i]))) {
        log_line += strlen(prefix[i]);
        type = i;
        break;
      }
    }
    /* Skip whitespaces and separators. */
    while (ELEM(log_line[0], ':', '(', ' ')) {
      log_line++;
    }
    /* Parse error line & char numbers. */
    error_line = error_char = -1;
    if (log_line[0] >= '0' && log_line[0] <= '9') {
      error_line = (int)strtol(log_line, &error_line_number_end, 10);
      /* Try to fetch the error character (not always available). */
      if (ELEM(error_line_number_end[0], '(', ':') && error_line_number_end[1] != ' ') {
        error_char = (int)strtol(error_line_number_end + 1, &log_line, 10);
      }
      else {
        log_line = error_line_number_end;
      }
      /* There can be a 3rd number (case of mesa driver). */
      if (ELEM(log_line[0], '(', ':') && log_line[1] >= '0' && log_line[1] <= '9') {
        error_line = error_char;
        error_char = (int)strtol(log_line + 1, &error_line_number_end, 10);
        log_line = error_line_number_end;
      }
    }
    /* Skip whitespaces and separators. */
    while (ELEM(log_line[0], ':', ')', ' ')) {
      log_line++;
    }
    if (error_line == -1) {
      found_line_id = false;
    }
    const char *src_line = sources_combined;
    if ((error_line != -1) && (error_char != -1)) {
      if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OFFICIAL)) {
        /* source:line */
        int error_source = error_line;
        if (error_source < sources.size()) {
          src_line = sources[error_source];
          error_line = error_char;
          error_char = -1;
        }
      }
      else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) ||
               GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_MAC, GPU_DRIVER_OFFICIAL)) {
        /* 0:line */
        error_line = error_char;
        error_char = -1;
      }
      else {
        /* line:char */
      }
    }
    /* Separate from previous block. */
    if (last_error_line != error_line) {
      BLI_dynstr_appendf(dynstr, "%s%s%s\n", info_col, line_prefix, reset_col);
    }
    else if (error_char != last_error_char) {
      BLI_dynstr_appendf(dynstr, "%s\n", line_prefix);
    }
    /* Print line from the source file that is producing the error. */
    if ((error_line != -1) && (error_line != last_error_line || error_char != last_error_char)) {
      const char *src_line_end;
      found_line_id = false;
      /* error_line is 1 based in this case. */
      int src_line_index = 1;
      while ((src_line_end = strchr(src_line, '\n'))) {
        if (src_line_index == error_line) {
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
        if (error_line != last_error_line) {
          BLI_dynstr_appendf(dynstr, "%5d | ", src_line_index);
        }
        else {
          BLI_dynstr_appendf(dynstr, line_prefix);
        }
        BLI_dynstr_nappend(dynstr, src_line, (src_line_end + 1) - src_line);
        /* Print char offset. */
        BLI_dynstr_appendf(dynstr, line_prefix);
        if (error_char != -1) {
          for (int i = 0; i < error_char; i++) {
            BLI_dynstr_appendf(dynstr, " ");
          }
          BLI_dynstr_appendf(dynstr, "^");
        }
        BLI_dynstr_appendf(dynstr, "\n");
      }
    }
    BLI_dynstr_appendf(dynstr, line_prefix);
    /* Skip to message. Avoid redundant info. */
    const char *keywords[] = {"error", "warning"};
    for (int i = 0; i < ARRAY_SIZE(prefix); i++) {
      if (STREQLEN(log_line, keywords[i], strlen(keywords[i]))) {
        log_line += strlen(keywords[i]);
        type = i;
        break;
      }
    }
    /* Skip and separators. */
    while (ELEM(log_line[0], ':', ')')) {
      log_line++;
    }
    if (type == 0) {
      BLI_dynstr_appendf(dynstr, "%s%s%s: ", err_col, "Error", info_col);
    }
    else if (type == 1) {
      BLI_dynstr_appendf(dynstr, "%s%s%s: ", warn_col, "Warning", info_col);
    }
    /* Print the error itself. */
    BLI_dynstr_append(dynstr, info_col);
    BLI_dynstr_nappend(dynstr, log_line, (line_end + 1) - log_line);
    BLI_dynstr_append(dynstr, reset_col);
    /* Continue to next line. */
    log_line = line_end + 1;
    last_error_line = error_line;
    last_error_char = error_char;
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

/** \} */

}  // namespace blender::gpu
