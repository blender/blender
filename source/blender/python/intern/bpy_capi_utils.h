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
 * \ingroup pythonintern
 */

#pragma once

#if PY_VERSION_HEX < 0x03070000
#  error "Python 3.7 or greater is required, you'll need to update your Python."
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct EnumPropertyItem;
struct ReportList;

char *BPy_enum_as_string(const struct EnumPropertyItem *item);

#define BLANK_PYTHON_TYPE \
  { \
    PyVarObject_HEAD_INIT(NULL, 0) NULL \
  }

/* error reporting */
short BPy_reports_to_error(struct ReportList *reports, PyObject *exception, const bool clear);
void BPy_reports_write_stdout(const struct ReportList *reports, const char *header);
bool BPy_errors_to_report_ex(struct ReportList *reports,
                             const char *error_prefix,
                             const bool use_full,
                             const bool use_location);
bool BPy_errors_to_report_brief_with_prefix(struct ReportList *reports, const char *error_prefix);
bool BPy_errors_to_report(struct ReportList *reports);

/* TODO - find a better solution! */
struct bContext *BPy_GetContext(void);
void BPy_SetContext(struct bContext *C);

extern void bpy_context_set(struct bContext *C, PyGILState_STATE *gilstate);
extern void bpy_context_clear(struct bContext *C, const PyGILState_STATE *gilstate);

#ifdef __cplusplus
}
#endif
