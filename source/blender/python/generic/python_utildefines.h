/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 * \brief header-only utilities
 * \note light addition to Python.h, use py_capi_utils.h for larger features.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PyTuple_SET_ITEMS(op_arg, ...) \
  { \
    PyTupleObject *op = (PyTupleObject *)op_arg; \
    PyObject **ob_items = op->ob_item; \
    CHECK_TYPE_ANY(op_arg, PyObject *, PyTupleObject *); \
    BLI_assert(VA_NARGS_COUNT(__VA_ARGS__) == PyTuple_GET_SIZE(op)); \
    ARRAY_SET_ITEMS(ob_items, __VA_ARGS__); \
  } \
  (void)0

/**
 * Wrap #Py_INCREF & return the result,
 * use sparingly to avoid comma operator or temp var assignment.
 */
Py_LOCAL_INLINE(PyObject *) Py_INCREF_RET(PyObject *op)
{
  Py_INCREF(op);
  return op;
}

/**
 * Append & transfer ownership to the list,
 * avoids inline #Py_DECREF all over (which is quite a large macro).
 */
Py_LOCAL_INLINE(int) PyList_APPEND(PyObject *op, PyObject *v)
{
  int ret = PyList_Append(op, v);
  Py_DecRef(v);
  return ret;
}

#ifdef __cplusplus
}
#endif
