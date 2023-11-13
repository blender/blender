/* SPDX-FileCopyrightText: 2023 Blender Authors
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

extern PyTypeObject NonTVertex_Type;

#define BPy_NonTVertex_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&NonTVertex_Type))

/*---------------------------Python BPy_NonTVertex structure definition----------*/
typedef struct {
  BPy_ViewVertex py_vv;
  Freestyle::NonTVertex *ntv;
} BPy_NonTVertex;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
