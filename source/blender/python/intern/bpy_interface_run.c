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

static void python_script_error_jump_text(Text *text)
{
  int lineno;
  int offset;
  python_script_error_jump(text->id.name + 2, &lineno, &offset);
  if (lineno != -1) {
    /* select the line with the error */
    txt_move_to(text, lineno - 1, INT_MAX, false);
    txt_move_to(text, lineno - 1, offset, true);
  }
}

/* returns a dummy filename for a textblock so we can tell what file a text block comes from */
static void bpy_text_filename_get(char *fn, const Main *bmain, size_t fn_len, const Text *text)
{
  BLI_snprintf(fn, fn_len, "%s%c%s", ID_BLEND_PATH(bmain, &text->id), SEP, text->id.name + 2);
}

/* Very annoying! Undo #_PyModule_Clear(), see T23871. */
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
static bool python_script_exec(
    bContext *C, const char *fn, struct Text *text, struct ReportList *reports, const bool do_jump)
{
  Main *bmain_old = CTX_data_main(C);
  PyObject *main_mod = NULL;
  PyObject *py_dict = NULL, *py_result = NULL;
  PyGILState_STATE gilstate;

  BLI_assert(fn || text);

  if (fn == NULL && text == NULL) {
    return 0;
  }

  bpy_context_set(C, &gilstate);

  PyC_MainModule_Backup(&main_mod);

  if (text) {
    char fn_dummy[FILE_MAXDIR];
    bpy_text_filename_get(fn_dummy, bmain_old, sizeof(fn_dummy), text);

    if (text->compiled == NULL) { /* if it wasn't already compiled, do it now */
      char *buf;
      PyObject *fn_dummy_py;

      fn_dummy_py = PyC_UnicodeFromByte(fn_dummy);

      buf = txt_to_buf(text, NULL);
      text->compiled = Py_CompileStringObject(buf, fn_dummy_py, Py_file_input, NULL, -1);
      MEM_freeN(buf);

      Py_DECREF(fn_dummy_py);

      if (PyErr_Occurred()) {
        if (do_jump) {
          python_script_error_jump_text(text);
        }
        BPY_text_free_code(text);
      }
    }

    if (text->compiled) {
      py_dict = PyC_DefaultNameSpace(fn_dummy);
      py_result = PyEval_EvalCode(text->compiled, py_dict, py_dict);
    }
  }
  else {
    FILE *fp = BLI_fopen(fn, "r");

    if (fp) {
      py_dict = PyC_DefaultNameSpace(fn);

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
       * to leave the main namespace untouched. see: T51444
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
      py_result = PyRun_File(fp, fn, Py_file_input, py_dict, py_dict);
      fclose(fp);
#endif
    }
    else {
      PyErr_Format(
          PyExc_IOError, "Python file \"%s\" could not be opened: %s", fn, strerror(errno));
      py_result = NULL;
    }
  }

  if (!py_result) {
    if (text) {
      if (do_jump) {
        /* ensure text is valid before use, the script may have freed itself */
        Main *bmain_new = CTX_data_main(C);
        if ((bmain_old == bmain_new) && (BLI_findindex(&bmain_new->texts, text) != -1)) {
          python_script_error_jump_text(text);
        }
      }
    }
    BPy_errors_to_report(reports);
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

  if (imports && (!PyC_NameSpace_ImportArray(py_dict, imports))) {
    Py_DECREF(py_dict);
    retval = NULL;
  }
  else {
    retval = PyRun_String(expr, mode, py_dict, py_dict);
  }

  if (retval == NULL) {
    ok = false;
    BPy_errors_to_report(CTX_wm_reports(C));
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

  if (err_info->reports != NULL) {
    if (err_info->report_prefix) {
      BKE_reportf(err_info->reports, RPT_ERROR, "%s: %s", err_info->report_prefix, err_str);
    }
    else {
      BKE_report(err_info->reports, RPT_ERROR, err_str);
    }
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
