/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 *
 * Functions relating to compatibility across Python versions.
 */

#include <Python.h> /* IWYU pragma: keep. */

#include "BLI_utildefines.h" /* IWYU pragma: keep. */
#include "python_compat.hh"  /* IWYU pragma: keep. */

#if PY_VERSION_HEX >= 0x030d0000 /* >=3.14 */

/* Removed in Python 3.13. */
int _PyArg_CheckPositional(const char *name, Py_ssize_t nargs, Py_ssize_t min, Py_ssize_t max)
{
  BLI_assert(min >= 0);
  BLI_assert(min <= max);

  if (nargs < min) {
    if (name != nullptr) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s expected %s%zd argument%s, got %zd",
                   name,
                   (min == max ? "" : "at least "),
                   min,
                   min == 1 ? "" : "s",
                   nargs);
    }
    else {
      PyErr_Format(PyExc_TypeError,
                   "unpacked tuple should have %s%zd element%s,"
                   " but has %zd",
                   (min == max ? "" : "at least "),
                   min,
                   min == 1 ? "" : "s",
                   nargs);
    }
    return 0;
  }

  if (nargs == 0) {
    return 1;
  }

  if (nargs > max) {
    if (name != nullptr) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s expected %s%zd argument%s, got %zd",
                   name,
                   (min == max ? "" : "at most "),
                   max,
                   max == 1 ? "" : "s",
                   nargs);
    }
    else {
      PyErr_Format(PyExc_TypeError,
                   "unpacked tuple should have %s%zd element%s,"
                   " but has %zd",
                   (min == max ? "" : "at most "),
                   max,
                   max == 1 ? "" : "s",
                   nargs);
    }
    return 0;
  }

  return 1;
}

#endif
