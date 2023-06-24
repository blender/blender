/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Interface1D.h"

#include "../../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ViewEdge_Type;

#define BPy_ViewEdge_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&ViewEdge_Type))

/*---------------------------Python BPy_ViewEdge structure definition----------*/
typedef struct {
  BPy_Interface1D py_if1D;
  Freestyle::ViewEdge *ve;
} BPy_ViewEdge;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
