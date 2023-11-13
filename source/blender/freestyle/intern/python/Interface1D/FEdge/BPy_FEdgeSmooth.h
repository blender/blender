/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_FEdge.h"

#include "../../../view_map/Silhouette.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject FEdgeSmooth_Type;

#define BPy_FEdgeSmooth_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&FEdgeSmooth_Type))

/*---------------------------Python BPy_FEdgeSmooth structure definition----------*/
typedef struct {
  BPy_FEdge py_fe;
  Freestyle::FEdgeSmooth *fes;
} BPy_FEdgeSmooth;

/*---------------------------Python BPy_FEdgeSmooth visible prototypes-----------*/

void FEdgeSmooth_mathutils_register_callback();

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
