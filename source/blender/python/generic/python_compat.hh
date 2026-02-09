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

/* This code is not placed in the blender namespace, as it is meant to replace Python functions
 * in the global namespace. */

/* Python 3.14 made some changes, use the "new" names. */
#if PY_VERSION_HEX < 0x030e0000
#  define Py_HashPointer _Py_HashPointer
#  define PyThreadState_GetUnchecked _PyThreadState_UncheckedGet
/* TODO: Support: `PyDict_Pop`, it has different arguments. */
#endif

int _PyArg_CheckPositional(const char *name, Py_ssize_t nargs, Py_ssize_t min, Py_ssize_t max);
