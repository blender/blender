/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DVec3f.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject VertexOrientation3DF0D_Type;

#define BPy_VertexOrientation3DF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&VertexOrientation3DF0D_Type))

/*---------------------------Python BPy_VertexOrientation3DF0D structure definition----------*/
typedef struct {
  BPy_UnaryFunction0DVec3f py_uf0D_vec3f;
} BPy_VertexOrientation3DF0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
