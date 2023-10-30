/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#if PY_VERSION_HEX < 0x030a0000
#  error "Python 3.10 or greater is required, you'll need to update your Python."
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct ReportList;

/* error reporting */
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
 * \param reports: When set, an error will be added to this report, when NULL, print the error.
 *
 * \note Unless the caller handles printing the reports (or reports is NULL) it's best to ensure
 * the output is printed to the `stdout/stderr`:
 * \code{.cc}
 * BPy_errors_to_report(reports);
 * if (!BKE_reports_print_test(reports)) {
 *   BKE_reports_print(reports);
 * }
 * \endcode
 *
 * \note The caller is responsible for clearing the error (see #PyErr_Clear).
 */
bool BPy_errors_to_report(struct ReportList *reports);

struct bContext *BPY_context_get(void);

extern void bpy_context_set(struct bContext *C, PyGILState_STATE *gilstate);
/**
 * Context should be used but not now because it causes some bugs.
 */
extern void bpy_context_clear(struct bContext *C, const PyGILState_STATE *gilstate);

#ifdef __cplusplus
}
#endif
