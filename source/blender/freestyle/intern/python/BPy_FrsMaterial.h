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

#include "../scene_graph/FrsMaterial.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject FrsMaterial_Type;

#define BPy_FrsMaterial_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&FrsMaterial_Type))

/*---------------------------Python BPy_FrsMaterial structure definition----------*/
struct BPy_FrsMaterial {
  PyObject_HEAD
  Freestyle::FrsMaterial *m;
};

/*---------------------------Python BPy_FrsMaterial visible prototypes-----------*/

int FrsMaterial_Init(PyObject *module);
void FrsMaterial_mathutils_register_callback();

///////////////////////////////////////////////////////////////////////////////////////////
