/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 * \brief header-only compatibility defines.
 *
 * \note this header should not be removed/cleaned where Python is used.
 * Because its required for Blender to build against different versions of Python.
 */

#pragma once

#include <Python.h>

/* Removes `intialized` member from Python 3.13+. */
#if PY_VERSION_HEX >= 0x030d0000
#  define PY_ARG_PARSER_HEAD_COMPAT()
#elif PY_VERSION_HEX >= 0x030c0000
/* Add `intialized` member for Python 3.12+. */
#  define PY_ARG_PARSER_HEAD_COMPAT() 0,
#else
#  define PY_ARG_PARSER_HEAD_COMPAT()
#endif

/* Python 3.13 made some changes, use the "new" names. */
#if PY_VERSION_HEX < 0x030d0000
#  define PyObject_GetOptionalAttr _PyObject_LookupAttr

[[nodiscard]] Py_LOCAL_INLINE(int)
    PyObject_GetOptionalAttrString(PyObject *obj, const char *name, PyObject **result)
{
  PyObject *oname = PyUnicode_FromString(name);
  if (oname == nullptr) {
    *result = nullptr;
    return -1;
  }
  const int status = PyObject_GetOptionalAttr(obj, oname, result);
  Py_DECREF(oname);
  return status;
}

#  define Py_IsFinalizing _Py_IsFinalizing
#endif

/* Python 3.14 made some changes, use the "new" names. */
#if PY_VERSION_HEX < 0x030e0000
#  define Py_HashPointer _Py_HashPointer
#  define PyThreadState_GetUnchecked _PyThreadState_UncheckedGet
/* TODO: Support: `PyDict_Pop`, it has different arguments. */
#endif

#if PY_VERSION_HEX >= 0x030d0000 /* >= 3.13 */
int _PyArg_CheckPositional(const char *name, Py_ssize_t nargs, Py_ssize_t min, Py_ssize_t max);
#endif
