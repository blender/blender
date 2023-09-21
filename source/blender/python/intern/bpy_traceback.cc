/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file contains utility functions for getting data from a python stack
 * trace.
 */

#include <Python.h>
#include <frameobject.h>

#include "BLI_path_util.h"
#include "BLI_utildefines.h"
#ifdef WIN32
#  include "BLI_string.h" /* BLI_strcasecmp */
#endif

#include "bpy_traceback.h"

static const char *traceback_filepath(PyTracebackObject *tb, PyObject **r_coerce)
{
  PyCodeObject *code = PyFrame_GetCode(tb->tb_frame);
  *r_coerce = PyUnicode_EncodeFSDefault(code->co_filename);
  return PyBytes_AS_STRING(*r_coerce);
}

#define MAKE_PY_IDENTIFIER_EX(varname, value) static _Py_Identifier varname{value, -1};
#define MAKE_PY_IDENTIFIER(varname) MAKE_PY_IDENTIFIER_EX(PyId_##varname, #varname)

MAKE_PY_IDENTIFIER_EX(PyId_string, "<string>")
MAKE_PY_IDENTIFIER(msg);
MAKE_PY_IDENTIFIER(filename);
MAKE_PY_IDENTIFIER(lineno);
MAKE_PY_IDENTIFIER(offset);
MAKE_PY_IDENTIFIER(end_lineno);
MAKE_PY_IDENTIFIER(end_offset);
MAKE_PY_IDENTIFIER(text);

static int parse_syntax_error(PyObject *err,
                              PyObject **message,
                              PyObject **filename,
                              int *lineno,
                              int *offset,
                              int *end_lineno,
                              int *end_offset,
                              PyObject **text)
{
  Py_ssize_t hold;
  PyObject *v;

  *message = nullptr;
  *filename = nullptr;

  /* new style errors.  `err' is an instance */
  *message = _PyObject_GetAttrId(err, &PyId_msg);
  if (!*message) {
    goto finally;
  }

  v = _PyObject_GetAttrId(err, &PyId_filename);
  if (!v) {
    goto finally;
  }
  if (v == Py_None) {
    Py_DECREF(v);
    *filename = _PyUnicode_FromId(&PyId_string);
    if (*filename == nullptr) {
      goto finally;
    }
    Py_INCREF(*filename);
  }
  else {
    *filename = v;
  }

  v = _PyObject_GetAttrId(err, &PyId_lineno);
  if (!v) {
    goto finally;
  }
  hold = PyLong_AsSsize_t(v);
  Py_DECREF(v);
  if (hold < 0 && PyErr_Occurred()) {
    goto finally;
  }
  *lineno = int(hold);

  v = _PyObject_GetAttrId(err, &PyId_offset);
  if (!v) {
    goto finally;
  }
  if (v == Py_None) {
    *offset = -1;
    Py_DECREF(v);
  }
  else {
    hold = PyLong_AsSsize_t(v);
    Py_DECREF(v);
    if (hold < 0 && PyErr_Occurred()) {
      goto finally;
    }
    *offset = int(hold);
  }

  if (Py_TYPE(err) == (PyTypeObject *)PyExc_SyntaxError) {
    v = _PyObject_GetAttrId(err, &PyId_end_lineno);
    if (!v) {
      PyErr_Clear();
      *end_lineno = *lineno;
    }
    else if (v == Py_None) {
      *end_lineno = *lineno;
      Py_DECREF(v);
    }
    else {
      hold = PyLong_AsSsize_t(v);
      Py_DECREF(v);
      if (hold < 0 && PyErr_Occurred()) {
        goto finally;
      }
      *end_lineno = hold;
    }

    v = _PyObject_GetAttrId(err, &PyId_end_offset);
    if (!v) {
      PyErr_Clear();
      *end_offset = -1;
    }
    else if (v == Py_None) {
      *end_offset = -1;
      Py_DECREF(v);
    }
    else {
      hold = PyLong_AsSsize_t(v);
      Py_DECREF(v);
      if (hold < 0 && PyErr_Occurred()) {
        goto finally;
      }
      *end_offset = hold;
    }
  }
  else {
    /* `SyntaxError` subclasses. */
    *end_lineno = *lineno;
    *end_offset = -1;
  }

  v = _PyObject_GetAttrId(err, &PyId_text);
  if (!v) {
    goto finally;
  }
  if (v == Py_None) {
    Py_DECREF(v);
    *text = nullptr;
  }
  else {
    *text = v;
  }
  return 1;

finally:
  Py_XDECREF(*message);
  Py_XDECREF(*filename);
  return 0;
}
/* end copied function! */

bool python_script_error_jump(
    const char *filepath, int *r_lineno, int *r_offset, int *r_lineno_end, int *r_offset_end)
{
  bool success = false;
  PyObject *exception, *value, *tb;

  *r_lineno = -1;
  *r_offset = 0;

  *r_lineno_end = -1;
  *r_offset_end = 0;

  PyErr_Fetch(&exception, &value, (PyObject **)&tb);
  if (exception == nullptr) { /* Equivalent of `!PyErr_Occurred()`. */
    return false;
  }
  PyObject *base_exception_type = nullptr;
  if (PyErr_GivenExceptionMatches(exception, PyExc_SyntaxError)) {
    base_exception_type = PyExc_SyntaxError;
  }

  PyErr_NormalizeException(&exception, &value, &tb);

  if (base_exception_type == PyExc_SyntaxError) {
    /* No trace-back available when `SyntaxError`.
     * Python has no API for this. reference #parse_syntax_error() from `pythonrun.c`. */

    if (value) { /* Should always be true. */
      PyObject *message;
      PyObject *filepath_exc_py, *text_py;

      if (parse_syntax_error(value,
                             &message,
                             &filepath_exc_py,
                             r_lineno,
                             r_offset,
                             r_lineno_end,
                             r_offset_end,
                             &text_py))
      {
        const char *filepath_exc = PyUnicode_AsUTF8(filepath_exc_py);
        /* Python adds a '/', prefix, so check for both. */
        if ((BLI_path_cmp(filepath_exc, filepath) == 0) ||
            (ELEM(filepath_exc[0], '\\', '/') && BLI_path_cmp(filepath_exc + 1, filepath) == 0))
        {
          success = true;
        }
      }
    }
  }
  else {
    for (PyTracebackObject *tb_iter = (PyTracebackObject *)tb;
         tb_iter && (PyObject *)tb_iter != Py_None;
         tb_iter = tb_iter->tb_next)
    {
      PyObject *coerce;
      const char *tb_filepath = traceback_filepath(tb_iter, &coerce);
      const int match = ((BLI_path_cmp(tb_filepath, filepath) == 0) ||
                         (ELEM(tb_filepath[0], '\\', '/') &&
                          BLI_path_cmp(tb_filepath + 1, filepath) == 0));
      Py_DECREF(coerce);

      if (match) {
        /* Even though a match has been found, keep searching to find the inner most line. */
        success = true;
        *r_lineno = *r_lineno_end = tb_iter->tb_lineno;
      }
    }
  }

  PyErr_Restore(exception, value, tb);

  return success;
}
