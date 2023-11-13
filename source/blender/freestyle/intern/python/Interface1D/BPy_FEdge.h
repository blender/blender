/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Interface1D.h"

#include "../../view_map/Silhouette.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject FEdge_Type;

#define BPy_FEdge_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&FEdge_Type))

/*---------------------------Python BPy_FEdge structure definition----------*/
typedef struct {
  BPy_Interface1D py_if1D;
  Freestyle::FEdge *fe;
} BPy_FEdge;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
