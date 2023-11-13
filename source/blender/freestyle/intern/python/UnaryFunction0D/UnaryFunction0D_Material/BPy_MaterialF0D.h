/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DMaterial.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject MaterialF0D_Type;

#define BPy_MaterialF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&MaterialF0D_Type))

/*---------------------------Python BPy_MaterialF0D structure definition----------*/
typedef struct {
  BPy_UnaryFunction0DMaterial py_uf0D_material;
} BPy_MaterialF0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
