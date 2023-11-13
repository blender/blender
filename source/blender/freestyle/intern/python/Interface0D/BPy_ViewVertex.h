/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Interface0D.h"

#include "../../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ViewVertex_Type;

#define BPy_ViewVertex_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&ViewVertex_Type))

/*---------------------------Python BPy_ViewVertex structure definition----------*/
typedef struct {
  BPy_Interface0D py_if0D;
  Freestyle::ViewVertex *vv;
} BPy_ViewVertex;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
