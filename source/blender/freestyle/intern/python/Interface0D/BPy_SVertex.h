/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Interface0D.h"

#include "../../view_map/Silhouette.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject SVertex_Type;

#define BPy_SVertex_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&SVertex_Type))

/*---------------------------Python BPy_SVertex structure definition----------*/
typedef struct {
  BPy_Interface0D py_if0D;
  Freestyle::SVertex *sv;
} BPy_SVertex;

/*---------------------------Python BPy_SVertex visible prototypes-----------*/

void SVertex_mathutils_register_callback();

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
