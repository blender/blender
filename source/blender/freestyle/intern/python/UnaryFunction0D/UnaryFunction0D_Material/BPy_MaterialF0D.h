/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DMaterial.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject MaterialF0D_Type;

#define BPy_MaterialF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&MaterialF0D_Type))

/*---------------------------Python BPy_MaterialF0D structure definition----------*/
struct BPy_MaterialF0D {
  BPy_UnaryFunction0DMaterial py_uf0D_material;
};

///////////////////////////////////////////////////////////////////////////////////////////
