/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#include <Python.h>

#include "BLI_compiler_attrs.h"

namespace blender {

#if PY_VERSION_HEX < 0x030d0000
#  error "Python 3.13 or greater is required, you'll need to update your Python."
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

/**
 * Set the global `bpy.context` for Python and make the GIL and environment ready
 * to call into Python.
 */
extern void bpy_context_set(struct bContext *C, PyGILState_STATE *gilstate) ATTR_NONNULL(1);
/**
 * A version of #bpy_context_set that allows `C` to be null.
 * Use this in rare cases where the context may not be available
 * (e.g. when calling from `BPY_run_string` functions).
 *
 * WARNING: Using this is risky and should be avoided if at all possible
 * it risks the context's `rna_disallow_writes` not be being set for e.g.
 *
 * Callers should note why they are not able to use `bpy_context_set`.
 */
extern void bpy_context_set_allow_null(struct bContext *C, PyGILState_STATE *gilstate);
/**
 * Context should be used but not now because it causes some bugs.
 */
extern void bpy_context_clear(struct bContext *C, const PyGILState_STATE *gilstate);

}  // namespace blender
