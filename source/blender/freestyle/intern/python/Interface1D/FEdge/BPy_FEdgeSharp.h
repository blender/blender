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

extern PyTypeObject FEdgeSharp_Type;

#define BPy_FEdgeSharp_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&FEdgeSharp_Type))

/*---------------------------Python BPy_FEdgeSharp structure definition----------*/
typedef struct {
  BPy_FEdge py_fe;
  Freestyle::FEdgeSharp *fes;
} BPy_FEdgeSharp;

/*---------------------------Python BPy_FEdgeSharp visible prototypes-----------*/

void FEdgeSharp_mathutils_register_callback();

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
