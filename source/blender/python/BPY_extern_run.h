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
 */

/** \file
 * \ingroup python
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_sys_types.h"

struct ReportList;
struct Text;
struct bContext;

/* bpy_interface_run.c */
bool BPY_run_filepath(struct bContext *C, const char *filepath, struct ReportList *reports);
bool BPY_run_text(struct bContext *C,
                  struct Text *text,
                  struct ReportList *reports,
                  const bool do_jump);

/* Use the 'eval' for simple single-line expressions,
 * otherwise 'exec' for full multi-line scripts. */
bool BPY_run_string_exec(struct bContext *C, const char *imports[], const char *expr);
bool BPY_run_string_eval(struct bContext *C, const char *imports[], const char *expr);

/**
 * \note When this struct is passed in as NULL,
 * print errors to the `stdout` and clear.
 */
struct BPy_RunErrInfo {
  /** Brief text, single line (can show this in status bar for e.g.). */
  bool use_single_line_error;

  /** Report with optional prefix (when non-NULL). */
  struct ReportList *reports;
  const char *report_prefix;

  /** Allocated exception text (assign when non-NULL). */
  char **r_string;
};

/* Run, evaluating to fixed type result. */
bool BPY_run_string_as_number(struct bContext *C,
                              const char *imports[],
                              const char *expr,
                              struct BPy_RunErrInfo *err_info,
                              double *r_value);
bool BPY_run_string_as_intptr(struct bContext *C,
                              const char *imports[],
                              const char *expr,
                              struct BPy_RunErrInfo *err_info,
                              intptr_t *r_value);
bool BPY_run_string_as_string_and_size(struct bContext *C,
                                       const char *imports[],
                                       const char *expr,
                                       struct BPy_RunErrInfo *err_info,
                                       char **r_value,
                                       size_t *r_value_size);
bool BPY_run_string_as_string(struct bContext *C,
                              const char *imports[],
                              const char *expr,
                              struct BPy_RunErrInfo *err_info,
                              char **r_value);

#ifdef __cplusplus
} /* extern "C" */
#endif
