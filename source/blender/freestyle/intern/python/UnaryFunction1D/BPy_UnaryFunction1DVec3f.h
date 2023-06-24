/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1D.h"

#include "../../geometry/Geom.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction1DVec3f_Type;

#define BPy_UnaryFunction1DVec3f_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction1DVec3f_Type))

/*---------------------------Python BPy_UnaryFunction1DVec3f structure definition----------*/
typedef struct {
  BPy_UnaryFunction1D py_uf1D;
  Freestyle::UnaryFunction1D<Freestyle::Geometry::Vec3f> *uf1D_vec3f;
} BPy_UnaryFunction1DVec3f;

/*---------------------------Python BPy_UnaryFunction1DVec3f visible prototypes-----------*/
int UnaryFunction1DVec3f_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
