/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 */

#pragma once

#include <Python.h>

struct BPy_BMeshOpFunc {
  PyObject_HEAD /* Required Python macro. */
  const char *opname;
};

/**
 * This is the `__call__` for `bmesh.ops.xxx()`.
 */
PyObject *BPy_BMO_call(BPy_BMeshOpFunc *self, PyObject *args, PyObject *kw);
