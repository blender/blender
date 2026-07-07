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

#include "../stroke/Operators.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Operators_Type;

#define BPy_Operators_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&Operators_Type))

/*---------------------------Python BPy_Operators structure definition----------*/
struct BPy_Operators {
  PyObject_HEAD
};

/*---------------------------Python BPy_Operators visible prototypes-----------*/

int Operators_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////
