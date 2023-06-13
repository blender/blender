/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1DVec2f.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Normal2DF1D_Type;

#define BPy_Normal2DF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&Normal2DF1D_Type))

/*---------------------------Python BPy_Normal2DF1D structure definition----------*/
typedef struct {
  BPy_UnaryFunction1DVec2f py_uf1D_vec2f;
} BPy_Normal2DF1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
