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

bool BPY_run_string_as_number(struct bContext *C,
                              const char *imports[],
                              const char *expr,
                              const char *report_prefix,
                              double *r_value);
bool BPY_run_string_as_intptr(struct bContext *C,
                              const char *imports[],
                              const char *expr,
                              const char *report_prefix,
                              intptr_t *r_value);
bool BPY_run_string_as_string_and_size(struct bContext *C,
                                       const char *imports[],
                                       const char *expr,
                                       const char *report_prefix,
                                       char **r_value,
                                       size_t *r_value_size);
bool BPY_run_string_as_string(struct bContext *C,
                              const char *imports[],
                              const char *expr,
                              const char *report_prefix,
                              char **r_value);

bool BPY_run_string_ex(struct bContext *C, const char *imports[], const char *expr, bool use_eval);

bool BPY_run_string(struct bContext *C, const char *imports[], const char *expr);

#ifdef __cplusplus
} /* extern "C" */
#endif
