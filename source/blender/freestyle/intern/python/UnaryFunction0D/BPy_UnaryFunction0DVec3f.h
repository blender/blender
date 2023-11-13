/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0D.h"

#include "../../geometry/Geom.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DVec3f_Type;

#define BPy_UnaryFunction0DVec3f_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DVec3f_Type))

/*---------------------------Python BPy_UnaryFunction0DVec3f structure definition----------*/
typedef struct {
  BPy_UnaryFunction0D py_uf0D;
  Freestyle::UnaryFunction0D<Freestyle::Geometry::Vec3f> *uf0D_vec3f;
} BPy_UnaryFunction0DVec3f;

/*---------------------------Python BPy_UnaryFunction0DVec3f visible prototypes-----------*/
int UnaryFunction0DVec3f_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
