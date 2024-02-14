/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include <cstdio>

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_text.h"

#include "DNA_text_types.h"

#include "BPY_extern_run.h"

#include "bpy_capi_utils.h"
#include "bpy_intern_string.h"
#include "bpy_traceback.h"

#include "../generic/py_capi_utils.h"

/* -------------------------------------------------------------------- */
/** \name Private Utilities
 * \{ */

static void python_script_error_jump_text(Text *text, const char *filepath)
{
  int lineno, lineno_end;
  int offset, offset_end;
  if (python_script_error_jump(filepath, &lineno, &offset, &lineno_end, &offset_end)) {
    /* Start at the end so cursor motion that looses the selection,
     * leaves the cursor from the most useful place.
     * Also, the end can't always be set, so don't give it priority. */
    txt_move_to(text, lineno_end - 1, offset_end - 1, false);
    txt_move_to(text, lineno - 1, offset - 1, true);
  }
}

/**
 * Generate a `filepath` from a text-block so we can tell what file a text block comes from.
 */
static void bpy_text_filepath_get(char *filepath,
                                  const size_t filepath_maxncpy,
                                  const Main *bmain,
                                  const Text *text)
{
  BLI_snprintf(filepath,
               filepath_maxncpy,
               "%s%c%s",
               ID_BLEND_PATH(bmain, &text->id),
               SEP,
               text->id.name + 2);
}

/* Very annoying! Undo #_PyModule_Clear(), see #23871. */
#define PYMODULE_CLEAR_WORKAROUND

#ifdef PYMODULE_CLEAR_WORKAROUND
/* bad!, we should never do this, but currently only safe way I could find to keep namespace.
 * from being cleared. - campbell */
struct PyModuleObject {
  PyObject_HEAD
  PyObject *md_dict;
  /* omit other values, we only want the dict. */
};
#endif

/**
 * Compatibility wrapper for #PyRun_FileExFlags.
 */
static PyObject *python_compat_wrapper_PyRun_FileExFlags(FILE *fp,
                                                         const char *filepath,
                                                         const int start,
                                                         PyObject *globals,
                                                         PyObject *locals,
                                                         const int closeit,
                                                         PyCompilerFlags *flags)
{
  /* Previously we used #PyRun_File to run directly the code on a FILE
   * object, but as written in the Python/C API Ref Manual, chapter 2,
   * 'FILE structs for different C libraries can be different and incompatible'.
   * So now we load the script file data to a buffer on MS-Windows. */
#ifdef _WIN32
  bool use_file_handle_workaround = true;
#else
  bool use_file_handle_workaround = false;
#endif

  if (!use_file_handle_workaround) {
    return PyRun_FileExFlags(fp, filepath, start, globals, locals, closeit, flags);
  }

  PyObject *py_result = nullptr;
  size_t buf_len;
  char *buf = static_cast<char *>(BLI_file_read_data_as_mem_from_handle(fp, false, 1, &buf_len));
  if (closeit) {
    fclose(fp);
  }

  if (UNLIKELY(buf == nullptr)) {
    PyErr_Format(PyExc_IOError, "Python file \"%s\" could not read buffer", filepath);
  }
  else {
    buf[buf_len] = '\0';
    PyObject *filepath_py = PyC_UnicodeFromBytes(filepath);
    PyObject *compiled = Py_CompileStringObject(buf, filepath_py, Py_file_input, flags, -1);
    MEM_freeN(buf);
    Py_DECREF(filepath_py);

    if (compiled == nullptr) {
      /* Based on Python's internal usage, an error must always be set. */
      BLI_assert(PyErr_Occurred());
    }
    else {
      py_result = PyEval_EvalCode(compiled, globals, locals);
      Py_DECREF(compiled);
    }
  }
  return py_result;
}

/**
 * Execute a file-path or text-block.
 *
 * \param reports: Report exceptions as errors (may be nullptr).
 * \param do_jump: See #BPY_run_text.
 *
 * \note Share a function for this since setup/cleanup logic is the same.
 */
static bool python_script_exec(
    bContext *C, const char *filepath, Text *text, ReportList *reports, const bool do_jump)
{
  Main *bmain_old = CTX_data_main(C);
  PyObject *main_mod = nullptr;
  PyObject *py_dict = nullptr, *py_result = nullptr;
  PyGILState_STATE gilstate;

  char filepath_dummy[FILE_MAX];
  /** The `__file__` added into the name-space. */
  const char *filepath_namespace = nullptr;

  BLI_assert(filepath || text);

  if (filepath == nullptr && text == nullptr) {
    return false;
  }

  bpy_context_set(C, &gilstate);

  PyC_MainModule_Backup(&main_mod);

  if (text) {
    bpy_text_filepath_get(filepath_dummy, sizeof(filepath_dummy), bmain_old, text);
    filepath_namespace = filepath_dummy;

    if (text->compiled == nullptr) { /* if it wasn't already compiled, do it now */
      PyObject *filepath_dummy_py = PyC_UnicodeFromBytes(filepath_dummy);
      size_t buf_len_dummy;
      char *buf = txt_to_buf(text, &buf_len_dummy);
      text->compiled = Py_CompileStringObject(buf, filepath_dummy_py, Py_file_input, nullptr, -1);
      MEM_freeN(buf);
      Py_DECREF(filepath_dummy_py);
    }

    if (text->compiled) {
      py_dict = PyC_DefaultNameSpace(filepath_dummy);
      py_result = PyEval_EvalCode(static_cast<PyObject *>(text->compiled), py_dict, py_dict);
    }
  }
  else {
    FILE *fp = BLI_fopen(filepath, "rb");
    filepath_namespace = filepath;

    if (fp) {
      /* Matches behavior of running Python with a directory argument.
       * Without the `fstat`, the directory will execute & return None. */
      BLI_stat_t st;
      if (BLI_fstat(fileno(fp), &st) == 0 && S_ISDIR(st.st_mode)) {
        PyErr_Format(PyExc_IsADirectoryError, "Python file \"%s\" is a directory", filepath);
        BLI_assert(py_result == nullptr);
        fclose(fp);
      }
      else {
        /* Calls `fclose(fp)`, run the script with one fewer open files. */
        const int closeit = 1;
        py_dict = PyC_DefaultNameSpace(filepath);
        py_result = python_compat_wrapper_PyRun_FileExFlags(
            fp, filepath, Py_file_input, py_dict, py_dict, closeit, nullptr);
      }
    }
    else {
      PyErr_Format(
          PyExc_IOError, "Python file \"%s\" could not be opened: %s", filepath, strerror(errno));
      BLI_assert(py_result == nullptr);
    }
  }

  if (!py_result) {
    if (reports) {
      BPy_errors_to_report(reports);
    }
    if (text) {
      if (do_jump) {
        /* ensure text is valid before use, the script may have freed itself */
        Main *bmain_new = CTX_data_main(C);
        if ((bmain_old == bmain_new) && (BLI_findindex(&bmain_new->texts, text) != -1)) {
          python_script_error_jump_text(text, filepath_namespace);
        }
      }
    }
    if (!reports) {
      PyErr_Print();
    }
    PyErr_Clear();
  }
  else {
    Py_DECREF(py_result);
  }

  if (py_dict) {
#ifdef PYMODULE_CLEAR_WORKAROUND
    PyModuleObject *mmod = (PyModuleObject *)PyDict_GetItem(PyImport_GetModuleDict(),
                                                            bpy_intern_str___main__);
    PyObject *dict_back = mmod->md_dict;
    /* freeing the module will clear the namespace,
     * gives problems running classes defined in this namespace being used later. */
    mmod->md_dict = nullptr;
    Py_DECREF(dict_back);
#endif

#undef PYMODULE_CLEAR_WORKAROUND
  }

  PyC_MainModule_Restore(main_mod);

  bpy_context_clear(C, &gilstate);

  return (py_result != nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Run Text / Filename / String
 * \{ */

bool BPY_run_filepath(bContext *C, const char *filepath, ReportList *reports)
{
  return python_script_exec(C, filepath, nullptr, reports, false);
}

bool BPY_run_text(bContext *C, Text *text, ReportList *reports, const bool do_jump)
{
  return python_script_exec(C, nullptr, text, reports, do_jump);
}

/**
 * \param mode: Passed to #PyRun_String, matches Python's `compile` functions mode argument.
 * #Py_eval_input for `eval`, #Py_file_input for `exec`.
 */
static bool bpy_run_string_impl(bContext *C,
                                const char *imports[],
                                const char *expr,
                                const int mode)
{
  BLI_assert(expr);
  PyGILState_STATE gilstate;
  PyObject *main_mod = nullptr;
  PyObject *py_dict, *retval;
  bool ok = true;

  if (expr[0] == '\0') {
    return ok;
  }

  bpy_context_set(C, &gilstate);

  PyC_MainModule_Backup(&main_mod);

  py_dict = PyC_DefaultNameSpace("<blender string>");

  if (imports && !PyC_NameSpace_ImportArray(py_dict, imports)) {
    Py_DECREF(py_dict);
    retval = nullptr;
  }
  else {
    retval = PyRun_String(expr, mode, py_dict, py_dict);
  }

  if (retval == nullptr) {
    ok = false;
    if (ReportList *wm_reports = CTX_wm_reports(C)) {
      BPy_errors_to_report(wm_reports);
    }
    PyErr_Print();
    PyErr_Clear();
  }
  else {
    Py_DECREF(retval);
  }

  PyC_MainModule_Restore(main_mod);

  bpy_context_clear(C, &gilstate);

  return ok;
}

bool BPY_run_string_eval(bContext *C, const char *imports[], const char *expr)
{
  return bpy_run_string_impl(C, imports, expr, Py_eval_input);
}

bool BPY_run_string_exec(bContext *C, const char *imports[], const char *expr)
{
  return bpy_run_string_impl(C, imports, expr, Py_file_input);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Run Python & Evaluate Utilities
 *
 * Return values as plain C types, useful to run Python scripts
 * in code that doesn't deal with Python data-types.
 * \{ */

static void run_string_handle_error(BPy_RunErrInfo *err_info)
{
  BLI_assert(PyErr_Occurred());

  if (err_info == nullptr) {
    PyErr_Print();
    PyErr_Clear();
    return;
  }

  /* Signal to do nothing. */
  if (!(err_info->reports || err_info->r_string)) {
    PyErr_Clear();
    return;
  }

  PyObject *py_err_str = err_info->use_single_line_error ? PyC_ExceptionBuffer_Simple() :
                                                           PyC_ExceptionBuffer();
  const char *err_str = PyUnicode_AsUTF8(py_err_str);
  PyErr_Clear();

  if (err_info->reports != nullptr) {
    if (err_info->report_prefix) {
      BKE_reportf(err_info->reports, RPT_ERROR, "%s: %s", err_info->report_prefix, err_str);
    }
    else {
      BKE_report(err_info->reports, RPT_ERROR, err_str);
    }
  }

  /* Print the reports if they were not printed already. */
  if ((err_info->reports == nullptr) || !BKE_reports_print_test(err_info->reports, RPT_ERROR)) {
    if (err_info->report_prefix) {
      fprintf(stderr, "%s: ", err_info->report_prefix);
    }
    fprintf(stderr, "%s\n", err_str);
  }

  if (err_info->r_string != nullptr) {
    *err_info->r_string = BLI_strdup(err_str);
  }

  Py_XDECREF(py_err_str);
}

bool BPY_run_string_as_number(bContext *C,
                              const char *imports[],
                              const char *expr,
                              BPy_RunErrInfo *err_info,
                              double *r_value)
{
  PyGILState_STATE gilstate;
  bool ok = true;

  if (expr[0] == '\0') {
    *r_value = 0.0;
    return ok;
  }

  bpy_context_set(C, &gilstate);

  ok = PyC_RunString_AsNumber(imports, expr, "<expr as number>", r_value);

  if (ok == false) {
    run_string_handle_error(err_info);
  }

  bpy_context_clear(C, &gilstate);

  return ok;
}

bool BPY_run_string_as_string_and_len(bContext *C,
                                      const char *imports[],
                                      const char *expr,
                                      BPy_RunErrInfo *err_info,
                                      char **r_value,
                                      size_t *r_value_len)
{
  PyGILState_STATE gilstate;
  bool ok = true;

  if (expr[0] == '\0') {
    *r_value = nullptr;
    return ok;
  }

  bpy_context_set(C, &gilstate);

  ok = PyC_RunString_AsStringAndSize(imports, expr, "<expr as str>", r_value, r_value_len);

  if (ok == false) {
    run_string_handle_error(err_info);
  }

  bpy_context_clear(C, &gilstate);

  return ok;
}

bool BPY_run_string_as_string(
    bContext *C, const char *imports[], const char *expr, BPy_RunErrInfo *err_info, char **r_value)
{
  size_t value_dummy_len;
  return BPY_run_string_as_string_and_len(C, imports, expr, err_info, r_value, &value_dummy_len);
}

bool BPY_run_string_as_intptr(bContext *C,
                              const char *imports[],
                              const char *expr,
                              BPy_RunErrInfo *err_info,
                              intptr_t *r_value)
{
  PyGILState_STATE gilstate;
  bool ok = true;

  if (expr[0] == '\0') {
    *r_value = 0;
    return ok;
  }

  bpy_context_set(C, &gilstate);

  ok = PyC_RunString_AsIntPtr(imports, expr, "<expr as intptr>", r_value);

  if (ok == false) {
    run_string_handle_error(err_info);
  }

  bpy_context_clear(C, &gilstate);

  return ok;
}

/** \} */
