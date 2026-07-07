/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

extern "C" {
#include <Python.h>
}

#include "../view_map/Functions0D.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0D_Type;

#define BPy_UnaryFunction0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0D_Type))

/*---------------------------Python BPy_UnaryFunction0D structure definition----------*/
struct BPy_UnaryFunction0D {
  PyObject_HEAD
  PyObject *py_uf0D;
};

/*---------------------------Python BPy_UnaryFunction0D visible prototypes-----------*/

int UnaryFunction0D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////
