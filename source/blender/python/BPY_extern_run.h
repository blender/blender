/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup python
 *
 * \subsection common_args Common Arguments
 *
 * - `C` the #bContext (never NULL).
 *
 * - `imports`: This is simply supported for convenience since imports can make constructing
 *   strings more cumbersome as otherwise small expressions become multi-line code-blocks.
 *   Optional (ignored when NULL), otherwise this is a NULL terminated array of module names.
 *
 *   Failure to import any modules prevents any further execution.
 *
 * - `err_info` #BPy_RunErrInfo is passed to some functions so errors can be forwarded to the UI.
 *   Option (when NULL errors are printed to the `stdout` and cleared).
 *   However this should be used in any case the error would be useful to show to the user.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_sys_types.h"

#include "BLI_compiler_attrs.h"

struct ReportList;
struct Text;
struct bContext;

/* `bpy_interface_run.cc` */

/* -------------------------------------------------------------------- */
/** \name Run File/Text as a Script
 *
 * \note #BPY_run_filepath and #BPY_run_filepath have almost identical behavior
 * one operates on a file-path, the other on a blender text-block.
 * \{ */

/**
 * Execute `filepath` as a Python script.
 *
 * Wrapper for `PyRun_File` (similar to calling python with a script argument).
 * Used for the `--python` command line argument.
 *
 * \param C: The context (never NULL).
 * \param filepath: The file path to execute.
 * \param reports: Failure to execute the script will report the exception here (may be NULL).
 * \return true on success, otherwise false with an error reported to `reports`.
 *
 * \note Python scripts could consider `bpy.utils.execfile`, which has the advantage of returning
 * the object as a module for data access & caching `pyc` file for faster re-execution.
 */
bool BPY_run_filepath(struct bContext *C, const char *filepath, struct ReportList *reports)
    ATTR_NONNULL(1, 2);
/**
 * Execute a Blender `text` block as a Python script.
 *
 * Wrapper for `Py_CompileStringObject` & `PyEval_EvalCode`.
 * Used for the `--python-text` command line argument.
 *
 * \param C: The context (never NULL).
 * \param text: The text-block to execute.
 * \param reports: Failure to execute the script will report the exception here (may be NULL).
 * \param do_jump: When true, any error moves the cursor to the location of that error.
 * Useful for executing scripts interactively from the text editor.
 * \return true on success, otherwise false with an error reported to `reports`.
 *
 * \note The `__file__` is constructed by joining the blend file-path to the name of the text.
 * This is done so error messages give useful output however there are rare cases causes problems
 * with introspection tools which attempt to load `__file__`.
 */
bool BPY_run_text(struct bContext *C, struct Text *text, struct ReportList *reports, bool do_jump)
    ATTR_NONNULL(1, 2);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Run a String as a Script
 *
 * - Use 'eval' for simple single-line expressions.
 * - Use 'exec' for full multi-line scripts.
 * \{ */

/**
 * Run an entire script, matches: `exec(compile(..., "exec"))`
 */
bool BPY_run_string_exec(struct bContext *C, const char *imports[], const char *expr);
/**
 * Run an expression, matches: `exec(compile(..., "eval"))`.
 */
bool BPY_run_string_eval(struct bContext *C, const char *imports[], const char *expr);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Run a String as a Script & Return the Result
 *
 * Convenience functions for executing a script and returning the result as an expected type.
 * \{ */

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

/**
 * Evaluate `expr` as a number (double).
 *
 * \param C: See \ref common_args.
 * \param imports: See \ref common_args.
 * \param expr: The expression to evaluate.
 * \param err_info: See \ref common_args.
 * \param r_value: The resulting value.
 * \return Success.
 */
bool BPY_run_string_as_number(struct bContext *C,
                              const char *imports[],
                              const char *expr,
                              struct BPy_RunErrInfo *err_info,
                              double *r_value) ATTR_NONNULL(1, 3, 5);
/**
 * Evaluate `expr` as an integer or pointer.
 *
 * \note Support both int and pointers.
 *
 * \param C: See \ref common_args.
 * \param imports: See \ref common_args.
 * \param expr: The expression to evaluate.
 * \param err_info: See \ref common_args.
 * \param r_value: The resulting value.
 * \return Success.
 */
bool BPY_run_string_as_intptr(struct bContext *C,
                              const char *imports[],
                              const char *expr,
                              struct BPy_RunErrInfo *err_info,
                              intptr_t *r_value) ATTR_NONNULL(1, 3, 5);
/**
 * Evaluate `expr` as a string.
 *
 * \param C: See \ref common_args.
 * \param imports: See \ref common_args.
 * \param expr: The expression to evaluate.
 * \param err_info: See \ref common_args.
 * \param r_value: The resulting value.
 * \return Success.
 */
bool BPY_run_string_as_string_and_len(struct bContext *C,
                                      const char *imports[],
                                      const char *expr,
                                      struct BPy_RunErrInfo *err_info,
                                      char **r_value,
                                      size_t *r_value_len) ATTR_NONNULL(1, 3, 5, 6);

/** See #BPY_run_string_as_string_and_len */
bool BPY_run_string_as_string(struct bContext *C,
                              const char *imports[],
                              const char *expr,
                              struct BPy_RunErrInfo *err_info,
                              char **r_value) ATTR_NONNULL(1, 3, 5);

/** \} */

#ifdef __cplusplus
} /* extern "C" */
#endif
