/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#include <Python.h>

#if PY_VERSION_HEX < 0x030b0000
#  error "Python 3.11 or greater is required, you'll need to update your Python."
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct ReportList;

/**
 * Error reporting: convert BKE_report (#ReportList) reports into python errors.
 *
 * \param clear: When `true`, #BKE_reports_free is called on the given `reports`, which should
 * then be considered as 'freed' data and not used anymore.
 */
short BPy_reports_to_error(struct ReportList *reports, PyObject *exception, bool clear);
/**
 * A version of #BKE_report_write_file_fp that uses Python's stdout.
 */
void BPy_reports_write_stdout(const struct ReportList *reports, const char *header);
bool BPy_errors_to_report_ex(struct ReportList *reports,
                             const char *error_prefix,
                             bool use_full,
                             bool use_location);
/**
 * \param reports: Any errors will be added to the report list.
 *
 * \note The reports are never printed to the `stdout/stderr`,
 * so you may wish to call either `BKE_reports_print(reports)` or `PyErr_Print()` afterwards.
 * Typically `PyErr_Print()` is preferable as `sys.excepthook` is called.
 *
 * \note The caller is responsible for clearing the error (see #PyErr_Clear).
 */
bool BPy_errors_to_report(struct ReportList *reports);

struct bContext *BPY_context_get();

extern void bpy_context_set(struct bContext *C, PyGILState_STATE *gilstate);
/**
 * Context should be used but not now because it causes some bugs.
 */
extern void bpy_context_clear(struct bContext *C, const PyGILState_STATE *gilstate);

#ifdef __cplusplus
}
#endif
