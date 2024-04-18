/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 *
 * Extend upon CPython's API, filling in some gaps, these functions use PyC_
 * prefix to distinguish them apart from CPython.
 *
 * \note
 * This module should only depend on CPython, however it currently uses
 * BLI_string_utf8() for unicode conversion.
 */

/* Future-proof, See https://docs.python.org/3/c-api/arg.html#strings-and-buffers */
#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <frameobject.h>

#include "BLI_utildefines.h" /* for bool */

#include "py_capi_utils.h"

#include "python_utildefines.h"

#ifndef MATH_STANDALONE
#  include "MEM_guardedalloc.h"

#  include "BLI_string.h"

/* Only for #BLI_strncpy_wchar_from_utf8,
 * should replace with Python functions but too late in release now. */
#  include "BLI_string_utf8.h"
#endif

#ifdef _WIN32
#  include "BLI_math_base.h" /* isfinite() */
#endif

/* -------------------------------------------------------------------- */
/** \name Fast Python to C Array Conversion for Primitive Types
 * \{ */

/* array utility function */
int PyC_AsArray_FAST(void *array,
                     const size_t array_item_size,
                     PyObject *value_fast,
                     const Py_ssize_t length,
                     const PyTypeObject *type,
                     const char *error_prefix)
{
  const Py_ssize_t value_len = PySequence_Fast_GET_SIZE(value_fast);
  PyObject **value_fast_items = PySequence_Fast_ITEMS(value_fast);
  Py_ssize_t i;

  BLI_assert(PyList_Check(value_fast) || PyTuple_Check(value_fast));

  if (value_len != length) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s: invalid sequence length. expected %d, got %d",
                 error_prefix,
                 length,
                 value_len);
    return -1;
  }

  /* for each type */
  if (type == &PyFloat_Type) {
    switch (array_item_size) {
      case sizeof(double): {
        double *array_double = static_cast<double *>(array);
        for (i = 0; i < length; i++) {
          array_double[i] = PyFloat_AsDouble(value_fast_items[i]);
        }
        break;
      }
      case sizeof(float): {
        float *array_float = static_cast<float *>(array);
        for (i = 0; i < length; i++) {
          array_float[i] = PyFloat_AsDouble(value_fast_items[i]);
        }
        break;
      }
      default: {
        /* Internal error. */
        BLI_assert_unreachable();
      }
    }
  }
  else if (type == &PyLong_Type) {
    switch (array_item_size) {
      case sizeof(int64_t): {
        int64_t *array_int = static_cast<int64_t *>(array);
        for (i = 0; i < length; i++) {
          array_int[i] = PyC_Long_AsI64(value_fast_items[i]);
        }
        break;
      }
      case sizeof(int32_t): {
        int32_t *array_int = static_cast<int32_t *>(array);
        for (i = 0; i < length; i++) {
          array_int[i] = PyC_Long_AsI32(value_fast_items[i]);
        }
        break;
      }
      case sizeof(int16_t): {
        int16_t *array_int = static_cast<int16_t *>(array);
        for (i = 0; i < length; i++) {
          array_int[i] = PyC_Long_AsI16(value_fast_items[i]);
        }
        break;
      }
      case sizeof(int8_t): {
        int8_t *array_int = static_cast<int8_t *>(array);
        for (i = 0; i < length; i++) {
          array_int[i] = PyC_Long_AsI8(value_fast_items[i]);
        }
        break;
      }
      default: {
        /* Internal error. */
        BLI_assert_unreachable();
      }
    }
  }
  else if (type == &PyBool_Type) {
    switch (array_item_size) {
      case sizeof(int64_t): {
        int64_t *array_bool = static_cast<int64_t *>(array);
        for (i = 0; i < length; i++) {
          array_bool[i] = (PyLong_AsLong(value_fast_items[i]) != 0);
        }
        break;
      }
      case sizeof(int32_t): {
        int32_t *array_bool = static_cast<int32_t *>(array);
        for (i = 0; i < length; i++) {
          array_bool[i] = (PyLong_AsLong(value_fast_items[i]) != 0);
        }
        break;
      }
      case sizeof(int16_t): {
        int16_t *array_bool = static_cast<int16_t *>(array);
        for (i = 0; i < length; i++) {
          array_bool[i] = (PyLong_AsLong(value_fast_items[i]) != 0);
        }
        break;
      }
      case sizeof(int8_t): {
        int8_t *array_bool = static_cast<int8_t *>(array);
        for (i = 0; i < length; i++) {
          array_bool[i] = (PyLong_AsLong(value_fast_items[i]) != 0);
        }
        break;
      }
      default: {
        /* Internal error. */
        BLI_assert_unreachable();
      }
    }
  }
  else {
    PyErr_Format(PyExc_TypeError, "%s: internal error %s is invalid", error_prefix, type->tp_name);
    return -1;
  }

  if (PyErr_Occurred()) {
    PyErr_Format(PyExc_TypeError,
                 "%s: one or more items could not be used as a %s",
                 error_prefix,
                 type->tp_name);
    return -1;
  }

  return 0;
}

int PyC_AsArray(void *array,
                const size_t array_item_size,
                PyObject *value,
                const Py_ssize_t length,
                const PyTypeObject *type,
                const char *error_prefix)
{
  PyObject *value_fast;
  int ret;

  if (!(value_fast = PySequence_Fast(value, error_prefix))) {
    return -1;
  }

  ret = PyC_AsArray_FAST(array, array_item_size, value_fast, length, type, error_prefix);
  Py_DECREF(value_fast);
  return ret;
}

static int PyC_AsArray_Multi_impl(void **array_p,
                                  const size_t array_item_size,
                                  PyObject *value,
                                  const int *dims,
                                  const int dims_len,
                                  const PyTypeObject *type,
                                  const char *error_prefix);

static int PyC_AsArray_Multi_FAST_impl(void **array_p,
                                       const size_t array_item_size,
                                       PyObject *value_fast,
                                       const int *dims,
                                       const int dims_len,
                                       const PyTypeObject *type,
                                       const char *error_prefix)
{
  const Py_ssize_t value_len = PySequence_Fast_GET_SIZE(value_fast);
  const int length = dims[0];

  if (dims_len == 1) {
    if (PyC_AsArray_FAST(*array_p, array_item_size, value_fast, length, type, error_prefix) == -1)
    {
      return -1;
    }
    *array_p = POINTER_OFFSET(*array_p, array_item_size * length);
  }
  else {
    if (value_len != length) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s: invalid sequence length. expected %d, got %d",
                   error_prefix,
                   length,
                   value_len);
      return -1;
    }

    PyObject **value_fast_items = PySequence_Fast_ITEMS(value_fast);
    const int *dims_next = dims + 1;
    const int dims_next_len = dims_len - 1;

    for (int i = 0; i < length; i++) {
      if (PyC_AsArray_Multi_impl(array_p,
                                 array_item_size,
                                 value_fast_items[i],
                                 dims_next,
                                 dims_next_len,
                                 type,
                                 error_prefix) == -1)
      {
        return -1;
      }
    }
  }
  return 0;
}

static int PyC_AsArray_Multi_impl(void **array_p,
                                  const size_t array_item_size,
                                  PyObject *value,
                                  const int *dims,
                                  const int dims_len,
                                  const PyTypeObject *type,
                                  const char *error_prefix)
{
  PyObject *value_fast;
  int ret;

  if (!(value_fast = PySequence_Fast(value, error_prefix))) {
    return -1;
  }

  ret = PyC_AsArray_Multi_FAST_impl(
      array_p, array_item_size, value_fast, dims, dims_len, type, error_prefix);
  Py_DECREF(value_fast);
  return ret;
}

int PyC_AsArray_Multi_FAST(void *array,
                           const size_t array_item_size,
                           PyObject *value_fast,
                           const int *dims,
                           const int dims_len,
                           const PyTypeObject *type,
                           const char *error_prefix)
{
  return PyC_AsArray_Multi_FAST_impl(
      &array, array_item_size, value_fast, dims, dims_len, type, error_prefix);
}

int PyC_AsArray_Multi(void *array,
                      const size_t array_item_size,
                      PyObject *value,
                      const int *dims,
                      const int dims_len,
                      const PyTypeObject *type,
                      const char *error_prefix)
{
  return PyC_AsArray_Multi_impl(
      &array, array_item_size, value, dims, dims_len, type, error_prefix);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Typed Tuple Packing
 *
 * \note See #PyC_Tuple_Pack_* macros that take multiple arguments.
 * \{ */

/* array utility function */
PyObject *PyC_Tuple_PackArray_F32(const float *array, uint len)
{
  PyObject *tuple = PyTuple_New(len);
  for (uint i = 0; i < len; i++) {
    PyTuple_SET_ITEM(tuple, i, PyFloat_FromDouble(array[i]));
  }
  return tuple;
}

PyObject *PyC_Tuple_PackArray_F64(const double *array, uint len)
{
  PyObject *tuple = PyTuple_New(len);
  for (uint i = 0; i < len; i++) {
    PyTuple_SET_ITEM(tuple, i, PyFloat_FromDouble(array[i]));
  }
  return tuple;
}

PyObject *PyC_Tuple_PackArray_I32(const int *array, uint len)
{
  PyObject *tuple = PyTuple_New(len);
  for (uint i = 0; i < len; i++) {
    PyTuple_SET_ITEM(tuple, i, PyLong_FromLong(array[i]));
  }
  return tuple;
}

PyObject *PyC_Tuple_PackArray_I32FromBool(const int *array, uint len)
{
  PyObject *tuple = PyTuple_New(len);
  for (uint i = 0; i < len; i++) {
    PyTuple_SET_ITEM(tuple, i, PyBool_FromLong(array[i]));
  }
  return tuple;
}

PyObject *PyC_Tuple_PackArray_Bool(const bool *array, uint len)
{
  PyObject *tuple = PyTuple_New(len);
  for (uint i = 0; i < len; i++) {
    PyTuple_SET_ITEM(tuple, i, PyBool_FromLong(array[i]));
  }
  return tuple;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Typed Tuple Packing (Multi-Dimensional)
 * \{ */

static PyObject *PyC_Tuple_PackArray_Multi_F32_impl(const float **array_p,
                                                    const int dims[],
                                                    const int dims_len)
{
  const int len = dims[0];
  if (dims_len == 1) {
    PyObject *tuple = PyC_Tuple_PackArray_F32(*array_p, len);
    *array_p = (*array_p) + len;
    return tuple;
  }
  PyObject *tuple = PyTuple_New(dims[0]);
  const int *dims_next = dims + 1;
  const int dims_next_len = dims_len - 1;
  for (uint i = 0; i < len; i++) {
    PyTuple_SET_ITEM(
        tuple, i, PyC_Tuple_PackArray_Multi_F32_impl(array_p, dims_next, dims_next_len));
  }
  return tuple;
}
PyObject *PyC_Tuple_PackArray_Multi_F32(const float *array, const int dims[], const int dims_len)
{
  return PyC_Tuple_PackArray_Multi_F32_impl(&array, dims, dims_len);
}

static PyObject *PyC_Tuple_PackArray_Multi_F64_impl(const double **array_p,
                                                    const int dims[],
                                                    const int dims_len)
{
  const int len = dims[0];
  if (dims_len == 1) {
    PyObject *tuple = PyC_Tuple_PackArray_F64(*array_p, len);
    *array_p = (*array_p) + len;
    return tuple;
  }
  PyObject *tuple = PyTuple_New(dims[0]);
  const int *dims_next = dims + 1;
  const int dims_next_len = dims_len - 1;
  for (uint i = 0; i < len; i++) {
    PyTuple_SET_ITEM(
        tuple, i, PyC_Tuple_PackArray_Multi_F64_impl(array_p, dims_next, dims_next_len));
  }
  return tuple;
}
PyObject *PyC_Tuple_PackArray_Multi_F64(const double *array, const int dims[], const int dims_len)
{
  return PyC_Tuple_PackArray_Multi_F64_impl(&array, dims, dims_len);
}

static PyObject *PyC_Tuple_PackArray_Multi_I32_impl(const int **array_p,
                                                    const int dims[],
                                                    const int dims_len)
{
  const int len = dims[0];
  if (dims_len == 1) {
    PyObject *tuple = PyC_Tuple_PackArray_I32(*array_p, len);
    *array_p = (*array_p) + len;
    return tuple;
  }
  PyObject *tuple = PyTuple_New(dims[0]);
  const int *dims_next = dims + 1;
  const int dims_next_len = dims_len - 1;
  for (uint i = 0; i < len; i++) {
    PyTuple_SET_ITEM(
        tuple, i, PyC_Tuple_PackArray_Multi_I32_impl(array_p, dims_next, dims_next_len));
  }
  return tuple;
}
PyObject *PyC_Tuple_PackArray_Multi_I32(const int *array, const int dims[], const int dims_len)
{
  return PyC_Tuple_PackArray_Multi_I32_impl(&array, dims, dims_len);
}

static PyObject *PyC_Tuple_PackArray_Multi_Bool_impl(const bool **array_p,
                                                     const int dims[],
                                                     const int dims_len)
{
  const int len = dims[0];
  if (dims_len == 1) {
    PyObject *tuple = PyC_Tuple_PackArray_Bool(*array_p, len);
    *array_p = (*array_p) + len;
    return tuple;
  }
  PyObject *tuple = PyTuple_New(dims[0]);
  const int *dims_next = dims + 1;
  const int dims_next_len = dims_len - 1;
  for (uint i = 0; i < len; i++) {
    PyTuple_SET_ITEM(
        tuple, i, PyC_Tuple_PackArray_Multi_Bool_impl(array_p, dims_next, dims_next_len));
  }
  return tuple;
}
PyObject *PyC_Tuple_PackArray_Multi_Bool(const bool *array, const int dims[], const int dims_len)
{
  return PyC_Tuple_PackArray_Multi_Bool_impl(&array, dims, dims_len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tuple/List Filling
 * \{ */

void PyC_Tuple_Fill(PyObject *tuple, PyObject *value)
{
  const uint tot = PyTuple_GET_SIZE(tuple);
  uint i;

  for (i = 0; i < tot; i++) {
    PyTuple_SET_ITEM(tuple, i, value);
    Py_INCREF(value);
  }
}

void PyC_List_Fill(PyObject *list, PyObject *value)
{
  const uint tot = PyList_GET_SIZE(list);
  uint i;

  for (i = 0; i < tot; i++) {
    PyList_SET_ITEM(list, i, value);
    Py_INCREF(value);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bool/Enum Argument Parsing
 * \{ */

int PyC_ParseBool(PyObject *o, void *p)
{
  bool *bool_p = static_cast<bool *>(p);
  long value;
  if (((value = PyLong_AsLong(o)) == -1) || !ELEM(value, 0, 1)) {
    PyErr_Format(PyExc_ValueError, "expected a bool or int (0/1), got %s", Py_TYPE(o)->tp_name);
    return 0;
  }

  *bool_p = value ? true : false;
  return 1;
}

int PyC_ParseStringEnum(PyObject *o, void *p)
{
  PyC_StringEnum *e = static_cast<PyC_StringEnum *>(p);
  const char *value = PyUnicode_AsUTF8(o);
  if (value == nullptr) {
    PyErr_Format(PyExc_ValueError, "expected a string, got %s", Py_TYPE(o)->tp_name);
    return 0;
  }
  int i;
  for (i = 0; e->items[i].id; i++) {
    if (STREQ(e->items[i].id, value)) {
      e->value_found = e->items[i].value;
      return 1;
    }
  }

  /* Set as a precaution. */
  e->value_found = -1;

  PyObject *enum_items = PyTuple_New(i);
  for (i = 0; e->items[i].id; i++) {
    PyTuple_SET_ITEM(enum_items, i, PyUnicode_FromString(e->items[i].id));
  }
  PyErr_Format(PyExc_ValueError, "expected a string in %S, got '%s'", enum_items, value);
  Py_DECREF(enum_items);
  return 0;
}

const char *PyC_StringEnum_FindIDFromValue(const PyC_StringEnumItems *items, const int value)
{
  for (int i = 0; items[i].id; i++) {
    if (items[i].value == value) {
      return items[i].id;
    }
  }
  return nullptr;
}

int PyC_CheckArgs_DeepCopy(PyObject *args)
{
  PyObject *dummy_pydict;
  return PyArg_ParseTuple(args, "|O!:__deepcopy__", &PyDict_Type, &dummy_pydict) != 0;
}

/** \} */

#ifndef MATH_STANDALONE

/* -------------------------------------------------------------------- */
/** \name Simple Printing (for debugging)
 *
 * These are useful to run directly from a debugger to be able to inspect the state.
 * \{ */

void PyC_ObSpit(const char *name, PyObject *var)
{
  const char *null_str = "<null>";
  fprintf(stderr, "<%s> : ", name);
  if (var == nullptr) {
    fprintf(stderr, "%s\n", null_str);
  }
  else {
    PyObject_Print(var, stderr, 0);
    const PyTypeObject *type = Py_TYPE(var);
    fprintf(stderr,
            " ref:%d, ptr:%p, type: %s\n",
            int(var->ob_refcnt),
            (void *)var,
            type ? type->tp_name : null_str);
  }
}

void PyC_ObSpitStr(char *result, size_t result_maxncpy, PyObject *var)
{
  /* No name, creator of string can manage that. */
  const char *null_str = "<null>";
  if (var == nullptr) {
    BLI_snprintf(result, result_maxncpy, "%s", null_str);
  }
  else {
    const PyTypeObject *type = Py_TYPE(var);
    PyObject *var_str = PyObject_Repr(var);
    if (var_str == nullptr) {
      /* We could print error here,
       * but this may be used for generating errors - so don't for now. */
      PyErr_Clear();
    }
    BLI_snprintf(result,
                 result_maxncpy,
                 " ref=%d, ptr=%p, type=%s, value=%.200s",
                 int(var->ob_refcnt),
                 (void *)var,
                 type ? type->tp_name : null_str,
                 var_str ? PyUnicode_AsUTF8(var_str) : "<error>");
    if (var_str != nullptr) {
      Py_DECREF(var_str);
    }
  }
}

void PyC_LineSpit()
{

  const char *filename;
  int lineno;

  /* NOTE: allow calling from outside python (RNA). */
  if (!PyC_IsInterpreterActive()) {
    fprintf(stderr, "Python line lookup failed, interpreter inactive\n");
    return;
  }

  PyErr_Clear();
  PyC_FileAndNum(&filename, &lineno);

  fprintf(stderr, "%s:%d\n", filename, lineno);
}

void PyC_StackSpit()
{
  /* NOTE: allow calling from outside python (RNA). */
  if (!PyC_IsInterpreterActive()) {
    fprintf(stderr, "Python line lookup failed, interpreter inactive\n");
    return;
  }

  /* lame but handy */
  const PyGILState_STATE gilstate = PyGILState_Ensure();
  PyRun_SimpleString("__import__('traceback').print_stack()");
  PyGILState_Release(gilstate);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Access Current Frame File Name & Line Number
 * \{ */

void PyC_FileAndNum(const char **r_filename, int *r_lineno)
{
  PyFrameObject *frame;
  PyCodeObject *code;

  if (r_filename) {
    *r_filename = nullptr;
  }
  if (r_lineno) {
    *r_lineno = -1;
  }

  if (!(frame = PyEval_GetFrame())) {
    return;
  }
  if (!(code = PyFrame_GetCode(frame))) {
    return;
  }

  /* when executing a script */
  if (r_filename) {
    *r_filename = PyUnicode_AsUTF8(code->co_filename);
  }

  /* when executing a module */
  if (r_filename && *r_filename == nullptr) {
    /* try an alternative method to get the r_filename - module based
     * references below are all borrowed (double checked) */
    PyObject *mod_name = PyDict_GetItemString(PyEval_GetGlobals(), "__name__");
    if (mod_name) {
      PyObject *mod = PyDict_GetItem(PyImport_GetModuleDict(), mod_name);
      if (mod) {
        PyObject *mod_file = PyModule_GetFilenameObject(mod);
        if (mod_file) {
          *r_filename = PyUnicode_AsUTF8(mod_name);
          Py_DECREF(mod_file);
        }
        else {
          PyErr_Clear();
        }
      }

      /* unlikely, fallback */
      if (*r_filename == nullptr) {
        *r_filename = PyUnicode_AsUTF8(mod_name);
      }
    }
  }

  if (r_lineno) {
    *r_lineno = PyFrame_GetLineNumber(frame);
  }
}

void PyC_FileAndNum_Safe(const char **r_filename, int *r_lineno)
{
  if (!PyC_IsInterpreterActive()) {
    return;
  }

  PyC_FileAndNum(r_filename, r_lineno);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Access Utilities
 * \{ */

PyObject *PyC_Object_GetAttrStringArgs(PyObject *o, Py_ssize_t n, ...)
{
  /* NOTE: Would be nice if python had this built in. */

  Py_ssize_t i;
  PyObject *item = o;
  const char *attr;

  va_list vargs;

  va_start(vargs, n);
  for (i = 0; i < n; i++) {
    attr = va_arg(vargs, char *);
    item = PyObject_GetAttrString(item, attr);

    if (item) {
      Py_DECREF(item);
    }
    else {
      /* python will set the error value here */
      break;
    }
  }
  va_end(vargs);

  Py_XINCREF(item); /* final value has is increfed, to match PyObject_GetAttrString */
  return item;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frozen Set Creation
 * \{ */

PyObject *PyC_FrozenSetFromStrings(const char **strings)
{
  const char **str;
  PyObject *ret;

  ret = PyFrozenSet_New(nullptr);

  for (str = strings; *str; str++) {
    PyObject *py_str = PyUnicode_FromString(*str);
    PySet_Add(ret, py_str);
    Py_DECREF(py_str);
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exception Utilities
 * \{ */

PyObject *PyC_Err_Format_Prefix(PyObject *exception_type_prefix, const char *format, ...)
{
  PyObject *error_value_as_unicode = nullptr;

  if (PyErr_Occurred()) {
    PyObject *error_type, *error_value, *error_traceback;
    PyErr_Fetch(&error_type, &error_value, &error_traceback);

    if (PyUnicode_Check(error_value)) {
      error_value_as_unicode = error_value;
      Py_INCREF(error_value_as_unicode);
    }
    else {
      error_value_as_unicode = PyUnicode_FromFormat(
          "%.200s(%S)", Py_TYPE(error_value)->tp_name, error_value);
    }
    PyErr_Restore(error_type, error_value, error_traceback);
  }

  va_list args;

  va_start(args, format);
  PyObject *error_value_format = PyUnicode_FromFormatV(format, args); /* Can fail and be null. */
  va_end(args);

  if (error_value_as_unicode) {
    if (error_value_format) {
      PyObject *error_value_format_prev = error_value_format;
      error_value_format = PyUnicode_FromFormat(
          "%S, %S", error_value_format, error_value_as_unicode);
      Py_DECREF(error_value_format_prev);
    }
    else {
      /* Should never happen, hints at incorrect API use or memory corruption. */
      error_value_format = PyUnicode_FromFormat("(internal error), %S", error_value_as_unicode);
    }
    Py_DECREF(error_value_as_unicode);
  }

  PyErr_SetObject(exception_type_prefix, error_value_format);
  Py_XDECREF(error_value_format);

  /* Strange to always return null, matches #PyErr_Format. */
  return nullptr;
}

PyObject *PyC_Err_SetString_Prefix(PyObject *exception_type_prefix, const char *str)
{
  return PyC_Err_Format_Prefix(exception_type_prefix, "%s", str);
}

void PyC_Err_PrintWithFunc(PyObject *py_func)
{
  /* since we return to C code we can't leave the error */
  PyCodeObject *f_code = (PyCodeObject *)PyFunction_GET_CODE(py_func);
  PyErr_Print();
  PyErr_Clear();

  /* use py style error */
  fprintf(stderr,
          "File \"%s\", line %d, in %s\n",
          PyUnicode_AsUTF8(f_code->co_filename),
          f_code->co_firstlineno,
          PyUnicode_AsUTF8(((PyFunctionObject *)py_func)->func_name));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exception Buffer Access
 * \{ */

/**
 * When a script calls `sys.exit(..)` it is expected that Blender quits,
 * internally this raises as `SystemExit` exception which this function detects.
 *
 * Historically Blender would call `PyErr_Print` when encountering an error.
 * In some cases `PyErr_Print` is still called, but not all.
 * When only #PyC_ExceptionBuffer is used to access the error, it's not desirable
 * that `sys.exit()` fails to exit, causing `sys.exit(..)` to arbitrarily work depending
 * on the internals of Blender's error handling.
 * To avoid this discrepancy, detect when `PyErr_Print` *would* exit and call it in that case.
 * It's important to call `PyErr_Print` (instead of simply exiting), because the exception
 * may contain a message which the user should see.
 *
 * \note No need to handle freeing resources here, Python's `atexit` is used to cleanup
 * Blender's state when Python requests an exit (via `bpy_atexit` callback).
 */
static void pyc_exception_buffer_handle_system_exit()
{
  if (!PyErr_ExceptionMatches(PyExc_SystemExit)) {
    return;
  }
  /* Inspecting, follow Python's logic in #_Py_HandleSystemExit & treat as a regular exception. */
  if (_Py_GetConfig()->inspect) {
    return;
  }

  /* NOTE(@ideasman42): A `SystemExit` exception will exit immediately (unless inspecting).
   * So print the error and exit now. Without this #PyErr_Display shows the error stack-trace
   * as a regular exception (as if something went wrong) and fail to exit.
   * see: #99966 for additional context.
   *
   * Arguably accessing a `SystemExit` exception as a buffer should be supported without exiting.
   * (by temporarily enabling inspection for example) however - it's not obvious exactly when this
   * should be enabled and complicates the Python API by introducing different kinds of execution.
   * Since the rule of thumb is for Blender's embedded Python to match stand-alone Python,
   * favor exiting when a `SystemExit` is raised.
   * Especially since this exception more likely to be used for background/batch-processing
   * utilities where exiting immediately makes sense, the possibility of this being called
   * indirectly from python-drivers or modal-operators is less of a concern. */
  PyErr_Print();
}

PyObject *PyC_ExceptionBuffer()
{
  BLI_assert(PyErr_Occurred());

  pyc_exception_buffer_handle_system_exit();

  /* The resulting exception as a string (return value). */
  PyObject *result = nullptr;

  PyObject *error_type, *error_value, *error_traceback;
  PyErr_Fetch(&error_type, &error_value, &error_traceback);

  /* Normalizing is needed because it's possible the error value is a string which
   * #PyErr_Display will fail to print. */
  PyErr_NormalizeException(&error_type, &error_value, &error_traceback);

  /* `io.StringIO()`. */
  PyObject *string_io = nullptr;
  PyObject *string_io_mod = nullptr;
  PyObject *string_io_getvalue = nullptr;
  if ((string_io_mod = PyImport_ImportModule("io")) &&
      (string_io = PyObject_CallMethod(string_io_mod, "StringIO", nullptr)) &&
      (string_io_getvalue = PyObject_GetAttrString(string_io, "getvalue")))
  {
    PyObject *stderr_backup = PySys_GetObject("stderr"); /* Borrowed. */
    /* Since these were borrowed we don't want them freed when replaced. */
    Py_INCREF(stderr_backup);
    PySys_SetObject("stderr", string_io); /* Freed when overwriting. */

    PyErr_Display(error_type, error_value, error_traceback);

    result = PyObject_CallObject(string_io_getvalue, nullptr);
    PySys_SetObject("stderr", stderr_backup);
    Py_DECREF(stderr_backup); /* Now `sys` owns the reference again. */
  }
  else {
    PySys_WriteStderr("Internal error creating: io.StringIO()!\n");
    if (UNLIKELY(PyErr_Occurred())) {
      PyErr_Print(); /* Show the error accessing `io.StringIO`. */
    }
    PyErr_Display(error_type, error_value, error_traceback);
  }

  Py_XDECREF(string_io_mod);
  Py_XDECREF(string_io_getvalue);
  Py_XDECREF(string_io); /* Free the original reference. */

  if (result == nullptr) {
    result = PyObject_Str(error_value);
    /* Python does this too. */
    if (UNLIKELY(result == nullptr)) {
      result = PyUnicode_FromFormat("<unprintable %s object>", Py_TYPE(error_value)->tp_name);
    }
  }

  PyErr_Restore(error_type, error_value, error_traceback);

  return result;
}

PyObject *PyC_ExceptionBuffer_Simple()
{
  BLI_assert(PyErr_Occurred());

  pyc_exception_buffer_handle_system_exit();

  /* The resulting exception as a string (return value). */
  PyObject *result = nullptr;

  PyObject *error_type, *error_value, *error_traceback;

  PyErr_Fetch(&error_type, &error_value, &error_traceback);

  if (PyErr_GivenExceptionMatches(error_type, PyExc_SyntaxError)) {
    /* Special exception for syntax errors,
     * in these cases the full error is verbose and not very useful,
     * just use the initial text so we know what the error is. */
    if (PyTuple_CheckExact(error_value) && PyTuple_GET_SIZE(error_value) >= 1) {
      result = PyObject_Str(PyTuple_GET_ITEM(error_value, 0));
    }
  }

  if (result == nullptr) {
    result = PyObject_Str(error_value);
    /* Python does this too. */
    if (UNLIKELY(result == nullptr)) {
      result = PyUnicode_FromFormat("<unprintable %s object>", Py_TYPE(error_value)->tp_name);
    }
  }

  PyErr_Restore(error_type, error_value, error_traceback);

  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unicode Conversion
 *
 * In some cases we need to coerce strings, avoid doing this inline.
 * \{ */

const char *PyC_UnicodeAsBytesAndSize(PyObject *py_str, Py_ssize_t *r_size, PyObject **r_coerce)
{
  const char *result;

  result = PyUnicode_AsUTF8AndSize(py_str, r_size);

  if (result) {
    /* 99% of the time this is enough but we better support non unicode
     * chars since blender doesn't limit this */
    return result;
  }

  PyErr_Clear();

  if (PyBytes_Check(py_str)) {
    *r_size = PyBytes_GET_SIZE(py_str);
    return PyBytes_AS_STRING(py_str);
  }
  if ((*r_coerce = PyUnicode_EncodeFSDefault(py_str))) {
    *r_size = PyBytes_GET_SIZE(*r_coerce);
    return PyBytes_AS_STRING(*r_coerce);
  }

  /* leave error raised from EncodeFS */
  return nullptr;
}

const char *PyC_UnicodeAsBytes(PyObject *py_str, PyObject **r_coerce)
{
  const char *result;

  result = PyUnicode_AsUTF8(py_str);

  if (result) {
    /* 99% of the time this is enough but we better support non unicode
     * chars since blender doesn't limit this. */
    return result;
  }

  PyErr_Clear();

  if (PyBytes_Check(py_str)) {
    return PyBytes_AS_STRING(py_str);
  }
  if ((*r_coerce = PyUnicode_EncodeFSDefault(py_str))) {
    return PyBytes_AS_STRING(*r_coerce);
  }

  /* leave error raised from EncodeFS */
  return nullptr;
}

PyObject *PyC_UnicodeFromBytesAndSize(const char *str, Py_ssize_t size)
{
  PyObject *result = PyUnicode_FromStringAndSize(str, size);
  if (result) {
    /* 99% of the time this is enough but we better support non unicode
     * chars since blender doesn't limit this */
    return result;
  }

  PyErr_Clear();
  /* this means paths will always be accessible once converted, on all OS's */
  result = PyUnicode_DecodeFSDefaultAndSize(str, size);
  return result;
}

PyObject *PyC_UnicodeFromBytes(const char *str)
{
  return PyC_UnicodeFromBytesAndSize(str, strlen(str));
}

PyObject *PyC_UnicodeFromStdStr(const std::string &str)
{
  return PyC_UnicodeFromBytesAndSize(str.c_str(), str.length());
}

int PyC_ParseUnicodeAsBytesAndSize(PyObject *o, void *p)
{
  PyC_UnicodeAsBytesAndSize_Data *data = static_cast<PyC_UnicodeAsBytesAndSize_Data *>(p);
  if (UNLIKELY(o == nullptr)) {
    /* Signal to cleanup. */
    Py_CLEAR(data->value_coerce);
    return 1;
  }
  /* The value must be cleared. */
  BLI_assert(!(data->value_coerce || data->value || data->value_len));
  data->value = PyC_UnicodeAsBytesAndSize(o, &data->value_len, &data->value_coerce);
  if (data->value == nullptr) {
    /* Leave the error as-is. */
    return 0;
  }
  /* Needed to #Py_DECREF `data->value_coerce` on future failure. */
  return data->value_coerce ? Py_CLEANUP_SUPPORTED : 1;
}

int PyC_ParseUnicodeAsBytesAndSize_OrNone(PyObject *o, void *p)
{
  if (o == Py_None) {
    PyC_UnicodeAsBytesAndSize_Data *data = static_cast<PyC_UnicodeAsBytesAndSize_Data *>(p);
    BLI_assert(!(data->value_coerce || data->value || data->value_len));
    UNUSED_VARS_NDEBUG(data);
    return 1;
  }
  return PyC_ParseUnicodeAsBytesAndSize(o, p);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Name Space Creation/Manipulation
 * \{ */

PyObject *PyC_DefaultNameSpace(const char *filename)
{
  PyObject *modules = PyImport_GetModuleDict();
  PyObject *builtins = PyEval_GetBuiltins();
  PyObject *mod_main = PyModule_New("__main__");
  PyDict_SetItemString(modules, "__main__", mod_main);
  Py_DECREF(mod_main); /* `sys.modules` owns now. */
  PyModule_AddStringConstant(mod_main, "__name__", "__main__");
  if (filename) {
    /* __file__ mainly for nice UI'ness
     * NOTE: this won't map to a real file when executing text-blocks and buttons. */
    PyModule_AddObject(mod_main, "__file__", PyC_UnicodeFromBytes(filename));
  }
  PyModule_AddObject(mod_main, "__builtins__", builtins);
  Py_INCREF(builtins); /* AddObject steals a reference */
  return PyModule_GetDict(mod_main);
}

bool PyC_NameSpace_ImportArray(PyObject *py_dict, const char *imports[])
{
  for (int i = 0; imports[i]; i++) {
    PyObject *name = PyUnicode_FromString(imports[i]);
    PyObject *mod = PyImport_ImportModuleLevelObject(name, nullptr, nullptr, nullptr, 0);
    bool ok = false;
    if (mod) {
      PyDict_SetItem(py_dict, name, mod);
      ok = true;
      Py_DECREF(mod);
    }
    Py_DECREF(name);

    if (!ok) {
      return false;
    }
  }
  return true;
}

void PyC_MainModule_Backup(PyObject **r_main_mod)
{
  PyObject *modules = PyImport_GetModuleDict();
  PyObject *main_mod = PyDict_GetItemString(modules, "__main__");
  if (main_mod) {
    /* Ensure the backed up module is kept until it's ownership */
    /* is transferred back to `sys.modules`. */
    Py_INCREF(main_mod);
  }
  *r_main_mod = main_mod;
}

void PyC_MainModule_Restore(PyObject *main_mod)
{
  PyObject *modules = PyImport_GetModuleDict();
  if (main_mod) {
    PyDict_SetItemString(modules, "__main__", main_mod);
    Py_DECREF(main_mod);
  }
  else {
    PyDict_DelItemString(modules, "__main__");
  }
}

bool PyC_IsInterpreterActive()
{
  /* instead of PyThreadState_Get, which calls Py_FatalError */
  return (PyThreadState_GetDict() != nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Py_SetPythonHome Wrapper
 * \{ */

void PyC_RunQuicky(const char *filepath, int n, ...)
{
  /* NOTE: Would be nice if python had this built in
   * See: https://developer.blender.org/docs/handbook/tooling/pyfromc/ */

  FILE *fp = fopen(filepath, "r");

  if (fp) {
    const PyGILState_STATE gilstate = PyGILState_Ensure();

    va_list vargs;

    Py_ssize_t *sizes = static_cast<Py_ssize_t *>(PyMem_MALLOC(sizeof(*sizes) * (n / 2)));
    int i;

    PyObject *py_dict = PyC_DefaultNameSpace(filepath);
    PyObject *values = PyList_New(n / 2); /* namespace owns this, don't free */

    PyObject *py_result, *ret;

    PyObject *struct_mod = PyImport_ImportModule("struct");
    PyObject *calcsize = PyObject_GetAttrString(struct_mod, "calcsize"); /* struct.calcsize */
    PyObject *pack = PyObject_GetAttrString(struct_mod, "pack");         /* struct.pack */
    PyObject *unpack = PyObject_GetAttrString(struct_mod, "unpack");     /* struct.unpack */

    Py_DECREF(struct_mod);

    va_start(vargs, n);
    for (i = 0; i * 2 < n; i++) {
      const char *format = va_arg(vargs, char *);
      void *ptr = va_arg(vargs, void *);

      ret = PyObject_CallFunction(calcsize, "s", format);

      if (ret) {
        sizes[i] = PyLong_AsLong(ret);
        Py_DECREF(ret);
        ret = PyObject_CallFunction(unpack, "sy#", format, (char *)ptr, sizes[i]);
      }

      if (ret == nullptr) {
        printf("%s error, line:%d\n", __func__, __LINE__);
        PyErr_Print();
        PyErr_Clear();

        PyList_SET_ITEM(values, i, Py_INCREF_RET(Py_None)); /* hold user */

        sizes[i] = 0;
      }
      else {
        if (PyTuple_GET_SIZE(ret) == 1) {
          /* convenience, convert single tuples into single values */
          PyObject *tmp = PyTuple_GET_ITEM(ret, 0);
          Py_INCREF(tmp);
          Py_DECREF(ret);
          ret = tmp;
        }

        PyList_SET_ITEM(values, i, ret); /* hold user */
      }
    }
    va_end(vargs);

    /* set the value so we can access it */
    PyDict_SetItemString(py_dict, "values", values);
    Py_DECREF(values);

    py_result = PyRun_File(fp, filepath, Py_file_input, py_dict, py_dict);

    fclose(fp);

    if (py_result) {

      /* we could skip this but then only slice assignment would work
       * better not be so strict */
      values = PyDict_GetItemString(py_dict, "values");

      if (values && PyList_Check(values)) {

        /* don't use the result */
        Py_DECREF(py_result);
        py_result = nullptr;

        /* now get the values back */
        va_start(vargs, n);
        for (i = 0; i * 2 < n; i++) {
          const char *format = va_arg(vargs, char *);
          void *ptr = va_arg(vargs, void *);

          PyObject *item;
          PyObject *item_new;
          /* prepend the string formatting and remake the tuple */
          item = PyList_GET_ITEM(values, i);
          if (PyTuple_CheckExact(item)) {
            int ofs = PyTuple_GET_SIZE(item);
            item_new = PyTuple_New(ofs + 1);
            while (ofs--) {
              PyObject *member = PyTuple_GET_ITEM(item, ofs);
              PyTuple_SET_ITEM(item_new, ofs + 1, member);
              Py_INCREF(member);
            }

            PyTuple_SET_ITEM(item_new, 0, PyUnicode_FromString(format));
          }
          else {
            item_new = Py_BuildValue("sO", format, item);
          }

          ret = PyObject_Call(pack, item_new, nullptr);

          if (ret) {
            /* copy the bytes back into memory */
            memcpy(ptr, PyBytes_AS_STRING(ret), sizes[i]);
            Py_DECREF(ret);
          }
          else {
            printf("%s error on arg '%d', line:%d\n", __func__, i, __LINE__);
            PyC_ObSpit("failed converting:", item_new);
            PyErr_Print();
            PyErr_Clear();
          }

          Py_DECREF(item_new);
        }
        va_end(vargs);
      }
      else {
        printf("%s error, 'values' not a list, line:%d\n", __func__, __LINE__);
      }
    }
    else {
      printf("%s error line:%d\n", __func__, __LINE__);
      PyErr_Print();
      PyErr_Clear();
    }

    Py_DECREF(calcsize);
    Py_DECREF(pack);
    Py_DECREF(unpack);

    PyMem_FREE(sizes);

    PyGILState_Release(gilstate);
  }
  else {
    fprintf(stderr, "%s: '%s' missing\n", __func__, filepath);
  }
}

void *PyC_RNA_AsPointer(PyObject *value, const char *type_name)
{
  PyObject *as_pointer;
  PyObject *pointer;

  if (STREQ(Py_TYPE(value)->tp_name, type_name) &&
      (as_pointer = PyObject_GetAttrString(value, "as_pointer")) != nullptr &&
      PyCallable_Check(as_pointer))
  {
    void *result = nullptr;

    /* must be a 'type_name' object */
    pointer = PyObject_CallObject(as_pointer, nullptr);
    Py_DECREF(as_pointer);

    if (!pointer) {
      PyErr_SetString(PyExc_SystemError, "value.as_pointer() failed");
      return nullptr;
    }
    result = PyLong_AsVoidPtr(pointer);
    Py_DECREF(pointer);
    if (!result) {
      PyErr_SetString(PyExc_SystemError, "value.as_pointer() failed");
    }

    return result;
  }

  PyErr_Format(PyExc_TypeError,
               "expected '%.200s' type found '%.200s' instead",
               type_name,
               Py_TYPE(value)->tp_name);
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flag Set Utilities (#PyC_FlagSet)
 *
 * Convert to/from Python set of strings to an int flag.
 * \{ */

PyObject *PyC_FlagSet_AsString(const PyC_FlagSet *item)
{
  PyObject *py_items = PyList_New(0);
  for (; item->identifier; item++) {
    PyList_APPEND(py_items, PyUnicode_FromString(item->identifier));
  }
  PyObject *py_string = PyObject_Repr(py_items);
  Py_DECREF(py_items);
  return py_string;
}

int PyC_FlagSet_ValueFromID_int(const PyC_FlagSet *item, const char *identifier, int *r_value)
{
  for (; item->identifier; item++) {
    if (STREQ(item->identifier, identifier)) {
      *r_value = item->value;
      return 1;
    }
  }

  return 0;
}

int PyC_FlagSet_ValueFromID(const PyC_FlagSet *item,
                            const char *identifier,
                            int *r_value,
                            const char *error_prefix)
{
  if (PyC_FlagSet_ValueFromID_int(item, identifier, r_value) == 0) {
    PyObject *enum_str = PyC_FlagSet_AsString(item);
    PyErr_Format(
        PyExc_ValueError, "%s: '%.200s' not found in (%U)", error_prefix, identifier, enum_str);
    Py_DECREF(enum_str);
    return -1;
  }

  return 0;
}

int PyC_FlagSet_ToBitfield(const PyC_FlagSet *items,
                           PyObject *value,
                           int *r_value,
                           const char *error_prefix)
{
  /* set of enum items, concatenate all values with OR */
  int ret, flag = 0;

  /* set looping */
  Py_ssize_t pos = 0;
  Py_ssize_t hash = 0;
  PyObject *key;

  if (!PySet_Check(value)) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s expected a set, not %.200s",
                 error_prefix,
                 Py_TYPE(value)->tp_name);
    return -1;
  }

  *r_value = 0;

  while (_PySet_NextEntry(value, &pos, &key, &hash)) {
    const char *param = PyUnicode_AsUTF8(key);

    if (param == nullptr) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s set must contain strings, not %.200s",
                   error_prefix,
                   Py_TYPE(key)->tp_name);
      return -1;
    }

    if (PyC_FlagSet_ValueFromID(items, param, &ret, error_prefix) < 0) {
      return -1;
    }

    flag |= ret;
  }

  *r_value = flag;
  return 0;
}

PyObject *PyC_FlagSet_FromBitfield(PyC_FlagSet *items, int flag)
{
  PyObject *ret = PySet_New(nullptr);
  PyObject *pystr;

  for (; items->identifier; items++) {
    if (items->value & flag) {
      pystr = PyUnicode_FromString(items->identifier);
      PySet_Add(ret, pystr);
      Py_DECREF(pystr);
    }
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Run String (Evaluate to Primitive Types)
 * \{ */

bool PyC_RunString_AsNumber(const char *imports[],
                            const char *expr,
                            const char *filename,
                            double *r_value)
{
  PyObject *py_dict, *mod, *retval;
  bool ok = true;
  PyObject *main_mod = nullptr;

  PyC_MainModule_Backup(&main_mod);

  py_dict = PyC_DefaultNameSpace(filename);

  mod = PyImport_ImportModule("math");
  if (mod) {
    PyDict_Merge(py_dict, PyModule_GetDict(mod), 0); /* 0 - don't overwrite existing values */
    Py_DECREF(mod);
  }
  else { /* highly unlikely but possibly */
    PyErr_Print();
    PyErr_Clear();
  }

  if (imports && !PyC_NameSpace_ImportArray(py_dict, imports)) {
    ok = false;
  }
  else if ((retval = PyRun_String(expr, Py_eval_input, py_dict, py_dict)) == nullptr) {
    ok = false;
  }
  else {
    double val;

    if (PyTuple_Check(retval)) {
      /* Users my have typed in 10km, 2m
       * add up all values */
      int i;
      val = 0.0;

      for (i = 0; i < PyTuple_GET_SIZE(retval); i++) {
        const double val_item = PyFloat_AsDouble(PyTuple_GET_ITEM(retval, i));
        if (val_item == -1 && PyErr_Occurred()) {
          val = -1;
          break;
        }
        val += val_item;
      }
    }
    else {
      val = PyFloat_AsDouble(retval);
    }
    Py_DECREF(retval);

    if (val == -1 && PyErr_Occurred()) {
      ok = false;
    }
    else if (!isfinite(val)) {
      *r_value = 0.0;
    }
    else {
      *r_value = val;
    }
  }

  PyC_MainModule_Restore(main_mod);

  return ok;
}

bool PyC_RunString_AsIntPtr(const char *imports[],
                            const char *expr,
                            const char *filename,
                            intptr_t *r_value)
{
  PyObject *py_dict, *retval;
  bool ok = true;
  PyObject *main_mod = nullptr;

  PyC_MainModule_Backup(&main_mod);

  py_dict = PyC_DefaultNameSpace(filename);

  if (imports && !PyC_NameSpace_ImportArray(py_dict, imports)) {
    ok = false;
  }
  else if ((retval = PyRun_String(expr, Py_eval_input, py_dict, py_dict)) == nullptr) {
    ok = false;
  }
  else {
    intptr_t val;

    val = intptr_t(PyLong_AsVoidPtr(retval));
    if (val == 0 && PyErr_Occurred()) {
      ok = false;
    }
    else {
      *r_value = val;
    }

    Py_DECREF(retval);
  }

  PyC_MainModule_Restore(main_mod);

  return ok;
}

bool PyC_RunString_AsStringAndSize(const char *imports[],
                                   const char *expr,
                                   const char *filename,
                                   char **r_value,
                                   size_t *r_value_size)
{
  PyObject *py_dict, *retval;
  bool ok = true;
  PyObject *main_mod = nullptr;

  PyC_MainModule_Backup(&main_mod);

  py_dict = PyC_DefaultNameSpace(filename);

  if (imports && !PyC_NameSpace_ImportArray(py_dict, imports)) {
    ok = false;
  }
  else if ((retval = PyRun_String(expr, Py_eval_input, py_dict, py_dict)) == nullptr) {
    ok = false;
  }
  else {
    const char *val;
    Py_ssize_t val_len;

    val = PyUnicode_AsUTF8AndSize(retval, &val_len);
    if (val == nullptr && PyErr_Occurred()) {
      ok = false;
    }
    else {
      char *val_alloc = static_cast<char *>(MEM_mallocN(val_len + 1, __func__));
      memcpy(val_alloc, val, val_len + 1);
      *r_value = val_alloc;
      *r_value_size = val_len;
    }

    Py_DECREF(retval);
  }

  PyC_MainModule_Restore(main_mod);

  return ok;
}

bool PyC_RunString_AsString(const char *imports[],
                            const char *expr,
                            const char *filename,
                            char **r_value)
{
  size_t value_size;
  return PyC_RunString_AsStringAndSize(imports, expr, filename, r_value, &value_size);
}

bool PyC_RunString_AsStringAndSizeOrNone(const char *imports[],
                                         const char *expr,
                                         const char *filename,
                                         char **r_value,
                                         size_t *r_value_size)
{
  PyObject *py_dict, *retval;
  bool ok = true;
  PyObject *main_mod = nullptr;

  PyC_MainModule_Backup(&main_mod);

  py_dict = PyC_DefaultNameSpace(filename);

  if (imports && !PyC_NameSpace_ImportArray(py_dict, imports)) {
    ok = false;
  }
  else if ((retval = PyRun_String(expr, Py_eval_input, py_dict, py_dict)) == nullptr) {
    ok = false;
  }
  else {
    if (retval == Py_None) {
      *r_value = nullptr;
      *r_value_size = 0;
    }
    else {
      const char *val;
      Py_ssize_t val_len;

      val = PyUnicode_AsUTF8AndSize(retval, &val_len);
      if (val == nullptr && PyErr_Occurred()) {
        ok = false;
      }
      else {
        char *val_alloc = static_cast<char *>(MEM_mallocN(val_len + 1, __func__));
        memcpy(val_alloc, val, val_len + 1);
        *r_value = val_alloc;
        *r_value_size = val_len;
      }
    }

    Py_DECREF(retval);
  }

  PyC_MainModule_Restore(main_mod);

  return ok;
}

bool PyC_RunString_AsStringOrNone(const char *imports[],
                                  const char *expr,
                                  const char *filename,
                                  char **r_value)
{
  size_t value_size;
  return PyC_RunString_AsStringAndSizeOrNone(imports, expr, filename, r_value, &value_size);
}

/** \} */

#endif /* #ifndef MATH_STANDALONE */

/* -------------------------------------------------------------------- */
/** \name Int Conversion
 *
 * \note Python doesn't provide overflow checks for specific bit-widths.
 *
 * \{ */

/* Compiler optimizes out redundant checks. */
#ifdef __GNUC__
#  pragma warning(push)
#  pragma GCC diagnostic ignored "-Wtype-limits"
#endif

/* #PyLong_AsUnsignedLong, unlike #PyLong_AsLong, does not fall back to calling #PyNumber_Index
 * when its argument is not a `PyLongObject` instance. To match parsing signed integer types with
 * #PyLong_AsLong, this function performs the #PyNumber_Index fallback, if necessary, before
 * calling #PyLong_AsUnsignedLong. */
static ulong pyc_Long_AsUnsignedLong(PyObject *value)
{
  if (value == nullptr) {
    /* Let PyLong_AsUnsignedLong handle error raising. */
    return PyLong_AsUnsignedLong(value);
  }

  if (PyLong_Check(value)) {
    return PyLong_AsUnsignedLong(value);
  }

  /* Call `__index__` like PyLong_AsLong. */
  PyObject *value_converted = PyNumber_Index(value);
  if (value_converted == nullptr) {
    /* A `TypeError` will have been raised. */
    return ulong(-1);
  }
  ulong to_return = PyLong_AsUnsignedLong(value_converted);
  Py_DECREF(value_converted);
  return to_return;
}

int PyC_Long_AsBool(PyObject *value)
{
  const int test = _PyLong_AsInt(value);
  if (UNLIKELY(test == -1 && PyErr_Occurred())) {
    return -1;
  }
  if (UNLIKELY(uint(test) > 1)) {
    PyErr_SetString(PyExc_TypeError, "Python number not a bool (0/1)");
    return -1;
  }
  return test;
}

int8_t PyC_Long_AsI8(PyObject *value)
{
  const int test = _PyLong_AsInt(value);
  if (UNLIKELY(test == -1 && PyErr_Occurred())) {
    return -1;
  }
  if (UNLIKELY(test < INT8_MIN || test > INT8_MAX)) {
    PyErr_SetString(PyExc_OverflowError, "Python int too large to convert to C int8");
    return -1;
  }
  return int8_t(test);
}

int16_t PyC_Long_AsI16(PyObject *value)
{
  const int test = _PyLong_AsInt(value);
  if (UNLIKELY(test == -1 && PyErr_Occurred())) {
    return -1;
  }
  if (UNLIKELY(test < INT16_MIN || test > INT16_MAX)) {
    PyErr_SetString(PyExc_OverflowError, "Python int too large to convert to C int16");
    return -1;
  }
  return int16_t(test);
}

/* Inlined in header:
 * PyC_Long_AsI32
 * PyC_Long_AsI64
 */

uint8_t PyC_Long_AsU8(PyObject *value)
{
  const ulong test = pyc_Long_AsUnsignedLong(value);
  if (UNLIKELY(test == ulong(-1) && PyErr_Occurred())) {
    return uint8_t(-1);
  }
  if (UNLIKELY(test > UINT8_MAX)) {
    PyErr_SetString(PyExc_OverflowError, "Python int too large to convert to C uint8");
    return uint8_t(-1);
  }
  return uint8_t(test);
}

uint16_t PyC_Long_AsU16(PyObject *value)
{
  const ulong test = pyc_Long_AsUnsignedLong(value);
  if (UNLIKELY(test == ulong(-1) && PyErr_Occurred())) {
    return uint16_t(-1);
  }
  if (UNLIKELY(test > UINT16_MAX)) {
    PyErr_SetString(PyExc_OverflowError, "Python int too large to convert to C uint16");
    return uint16_t(-1);
  }
  return uint16_t(test);
}

uint32_t PyC_Long_AsU32(PyObject *value)
{
  const ulong test = pyc_Long_AsUnsignedLong(value);
  if (UNLIKELY(test == ulong(-1) && PyErr_Occurred())) {
    return uint32_t(-1);
  }
  if (UNLIKELY(test > UINT32_MAX)) {
    PyErr_SetString(PyExc_OverflowError, "Python int too large to convert to C uint32");
    return uint32_t(-1);
  }
  return uint32_t(test);
}

/* #PyLong_AsUnsignedLongLong, unlike #PyLong_AsLongLong, does not fall back to calling
 * #PyNumber_Index when its argument is not a `PyLongObject` instance. To match parsing signed
 * integer types with #PyLong_AsLongLong, this function performs the #PyNumber_Index fallback, if
 * necessary, before calling #PyLong_AsUnsignedLongLong. */
uint64_t PyC_Long_AsU64(PyObject *value)
{
  if (value == nullptr) {
    /* Let PyLong_AsUnsignedLongLong handle error raising. */
    return uint64_t(PyLong_AsUnsignedLongLong(value));
  }

  if (PyLong_Check(value)) {
    return uint64_t(PyLong_AsUnsignedLongLong(value));
  }

  /* Call `__index__` like PyLong_AsLongLong. */
  PyObject *value_converted = PyNumber_Index(value);
  if (value_converted == nullptr) {
    /* A `TypeError` will have been raised. */
    return uint64_t(-1);
  }

  uint64_t to_return = uint64_t(PyLong_AsUnsignedLongLong(value_converted));
  Py_DECREF(value_converted);
  return to_return;
}

#ifdef __GNUC__
#  pragma warning(pop)
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Py_buffer Utils
 * \{ */

char PyC_StructFmt_type_from_str(const char *typestr)
{
  switch (typestr[0]) {
    case '!':
    case '<':
    case '=':
    case '>':
    case '@':
      return typestr[1];
    default:
      return typestr[0];
  }
}

bool PyC_StructFmt_type_is_float_any(char format)
{
  switch (format) {
    case 'f':
    case 'd':
    case 'e':
      return true;
    default:
      return false;
  }
}

bool PyC_StructFmt_type_is_int_any(char format)
{
  switch (format) {
    case 'i':
    case 'I':
    case 'l':
    case 'L':
    case 'h':
    case 'H':
    case 'b':
    case 'B':
    case 'q':
    case 'Q':
    case 'n':
    case 'N':
    case 'P':
      return true;
    default:
      return false;
  }
}

bool PyC_StructFmt_type_is_byte(char format)
{
  switch (format) {
    case 'c':
    case 's':
    case 'p':
      return true;
    default:
      return false;
  }
}

bool PyC_StructFmt_type_is_bool(char format)
{
  switch (format) {
    case '?':
      return true;
    default:
      return false;
  }
}

/** \} */
