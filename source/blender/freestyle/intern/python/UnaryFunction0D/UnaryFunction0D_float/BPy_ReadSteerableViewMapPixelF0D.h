/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DFloat.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ReadSteerableViewMapPixelF0D_Type;

#define BPy_ReadSteerableViewMapPixelF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ReadSteerableViewMapPixelF0D_Type))

/*---------------------------Python BPy_ReadSteerableViewMapPixelF0D structure
 * definition----------*/
typedef struct {
  BPy_UnaryFunction0DFloat py_uf0D_float;
} BPy_ReadSteerableViewMapPixelF0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
