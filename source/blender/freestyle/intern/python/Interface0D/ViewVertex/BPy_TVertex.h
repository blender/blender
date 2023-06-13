/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_ViewVertex.h"

#include "../../../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject TVertex_Type;

#define BPy_TVertex_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&TVertex_Type))

/*---------------------------Python BPy_TVertex structure definition----------*/
typedef struct {
  BPy_ViewVertex py_vv;
  Freestyle::TVertex *tv;
} BPy_TVertex;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
