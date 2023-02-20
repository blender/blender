/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include <stdio.h>

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_text.h"

#include "DNA_text_types.h"

#include "BPY_extern.h"
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
                                  const size_t filepath_maxlen,
                                  const Main *bmain,
                                  const Text *text)
{
  BLI_snprintf(filepath,
               filepath_maxlen,
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
typedef struct {
  PyObject_HEAD
  PyObject *md_dict;
  /* omit other values, we only want the dict. */
} PyModuleObject;
#endif

/**
 * Execute a file-path or text-block.
 *
 * \param reports: Report exceptions as errors (may be NULL).
 * \param do_jump: See #BPY_run_text.
 *
 * \note Share a function for this since setup/cleanup logic is the same.
 */
static bool python_script_exec(bContext *C,
                               const char *filepath,
                               struct Text *text,
                               struct ReportList *reports,
                               const bool do_jump)
{
  Main *bmain_old = CTX_data_main(C);
  PyObject *main_mod = NULL;
  PyObject *py_dict = NULL, *py_result = NULL;
  PyGILState_STATE gilstate;

  char filepath_dummy[FILE_MAX];
  /** The `__file__` added into the name-space. */
  const char *filepath_namespace = NULL;

  BLI_assert(filepath || text);

  if (filepath == NULL && text == NULL) {
    return 0;
  }

  bpy_context_set(C, &gilstate);

  PyC_MainModule_Backup(&main_mod);

  if (text) {
    bpy_text_filepath_get(filepath_dummy, sizeof(filepath_dummy), bmain_old, text);
    filepath_namespace = filepath_dummy;

    if (text->compiled == NULL) { /* if it wasn't already compiled, do it now */
      char *buf;
      PyObject *filepath_dummy_py;

      filepath_dummy_py = PyC_UnicodeFromBytes(filepath_dummy);

      size_t buf_len_dummy;
      buf = txt_to_buf(text, &buf_len_dummy);
      text->compiled = Py_CompileStringObject(buf, filepath_dummy_py, Py_file_input, NULL, -1);
      MEM_freeN(buf);

      Py_DECREF(filepath_dummy_py);

      if (PyErr_Occurred()) {
        BPY_text_free_code(text);
      }
    }

    if (text->compiled) {
      py_dict = PyC_DefaultNameSpace(filepath_dummy);
      py_result = PyEval_EvalCode(text->compiled, py_dict, py_dict);
    }
  }
  else {
    FILE *fp = BLI_fopen(filepath, "r");
    filepath_namespace = filepath;

    if (fp) {
      py_dict = PyC_DefaultNameSpace(filepath);

#ifdef _WIN32
      /* Previously we used PyRun_File to run directly the code on a FILE
       * object, but as written in the Python/C API Ref Manual, chapter 2,
       * 'FILE structs for different C libraries can be different and
       * incompatible'.
       * So now we load the script file data to a buffer.
       *
       * Note on use of 'globals()', it's important not copy the dictionary because
       * tools may inspect 'sys.modules["__main__"]' for variables defined in the code
       * where using a copy of 'globals()' causes code execution
       * to leave the main namespace untouched. see: #51444
       *
       * This leaves us with the problem of variables being included,
       * currently this is worked around using 'dict.__del__' it's ugly but works.
       */
      {
        const char *pystring =
            "with open(__file__, 'rb') as f:"
            "exec(compile(f.read(), __file__, 'exec'), globals().__delitem__('f') or globals())";

        fclose(fp);

        py_result = PyRun_String(pystring, Py_file_input, py_dict, py_dict);
      }
#else
      py_result = PyRun_File(fp, filepath, Py_file_input, py_dict, py_dict);
      fclose(fp);
#endif
    }
    else {
      PyErr_Format(
          PyExc_IOError, "Python file \"%s\" could not be opened: %s", filepath, strerror(errno));
      py_result = NULL;
    }
  }

  if (!py_result) {
    BPy_errors_to_report(reports);
    if (text) {
      if (do_jump) {
        /* ensure text is valid before use, the script may have freed itself */
        Main *bmain_new = CTX_data_main(C);
        if ((bmain_old == bmain_new) && (BLI_findindex(&bmain_new->texts, text) != -1)) {
          python_script_error_jump_text(text, filepath_namespace);
        }
      }
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
    mmod->md_dict = NULL;
    Py_DECREF(dict_back);
#endif

#undef PYMODULE_CLEAR_WORKAROUND
  }

  PyC_MainModule_Restore(main_mod);

  bpy_context_clear(C, &gilstate);

  return (py_result != NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Run Text / Filename / String
 * \{ */

bool BPY_run_filepath(bContext *C, const char *filepath, struct ReportList *reports)
{
  return python_script_exec(C, filepath, NULL, reports, false);
}

bool BPY_run_text(bContext *C, struct Text *text, struct ReportList *reports, const bool do_jump)
{
  return python_script_exec(C, NULL, text, reports, do_jump);
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
  PyObject *main_mod = NULL;
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
    retval = NULL;
  }
  else {
    retval = PyRun_String(expr, mode, py_dict, py_dict);
  }

  if (retval == NULL) {
    ok = false;

    ReportList reports;
    BKE_reports_init(&reports, RPT_STORE);
    BPy_errors_to_report(&reports);
    PyErr_Clear();

    /* Ensure the reports are printed. */
    if (!BKE_reports_print_test(&reports, RPT_ERROR)) {
      BKE_reports_print(&reports, RPT_ERROR);
    }

    ReportList *wm_reports = CTX_wm_reports(C);
    if (wm_reports) {
      BLI_movelisttolist(&wm_reports->list, &reports.list);
    }
    else {
      BKE_reports_clear(&reports);
    }
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

static void run_string_handle_error(struct BPy_RunErrInfo *err_info)
{
  if (err_info == NULL) {
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
  const char *err_str = py_err_str ? PyUnicode_AsUTF8(py_err_str) : "Unable to extract exception";
  PyErr_Clear();

  if (err_info->reports != NULL) {
    if (err_info->report_prefix) {
      BKE_reportf(err_info->reports, RPT_ERROR, "%s: %s", err_info->report_prefix, err_str);
    }
    else {
      BKE_report(err_info->reports, RPT_ERROR, err_str);
    }
  }

  /* Print the reports if they were not printed already. */
  if ((err_info->reports == NULL) || !BKE_reports_print_test(err_info->reports, RPT_ERROR)) {
    if (err_info->report_prefix) {
      fprintf(stderr, "%s: ", err_info->report_prefix);
    }
    fprintf(stderr, "%s\n", err_str);
  }

  if (err_info->r_string != NULL) {
    *err_info->r_string = BLI_strdup(err_str);
  }

  Py_XDECREF(py_err_str);
}

bool BPY_run_string_as_number(bContext *C,
                              const char *imports[],
                              const char *expr,
                              struct BPy_RunErrInfo *err_info,
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

bool BPY_run_string_as_string_and_size(bContext *C,
                                       const char *imports[],
                                       const char *expr,
                                       struct BPy_RunErrInfo *err_info,
                                       char **r_value,
                                       size_t *r_value_size)
{
  PyGILState_STATE gilstate;
  bool ok = true;

  if (expr[0] == '\0') {
    *r_value = NULL;
    return ok;
  }

  bpy_context_set(C, &gilstate);

  ok = PyC_RunString_AsStringAndSize(imports, expr, "<expr as str>", r_value, r_value_size);

  if (ok == false) {
    run_string_handle_error(err_info);
  }

  bpy_context_clear(C, &gilstate);

  return ok;
}

bool BPY_run_string_as_string(bContext *C,
                              const char *imports[],
                              const char *expr,
                              struct BPy_RunErrInfo *err_info,
                              char **r_value)
{
  size_t value_dummy_size;
  return BPY_run_string_as_string_and_size(C, imports, expr, err_info, r_value, &value_dummy_size);
}

bool BPY_run_string_as_intptr(bContext *C,
                              const char *imports[],
                              const char *expr,
                              struct BPy_RunErrInfo *err_info,
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
