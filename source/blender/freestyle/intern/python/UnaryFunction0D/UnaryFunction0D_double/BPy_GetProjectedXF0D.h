/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject GetProjectedXF0D_Type;

#define BPy_GetProjectedXF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GetProjectedXF0D_Type))

/*---------------------------Python BPy_GetProjectedXF0D structure definition----------*/
typedef struct {
  BPy_UnaryFunction0DDouble py_uf0D_double;
} BPy_GetProjectedXF0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
