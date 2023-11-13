/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1DVoid.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject IncrementChainingTimeStampF1D_Type;

#define BPy_IncrementChainingTimeStampF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&IncrementChainingTimeStampF1D_Type))

/*---------------------------Python BPy_IncrementChainingTimeStampF1D structure
 * definition----------*/
typedef struct {
  BPy_UnaryFunction1DVoid py_uf1D_void;
} BPy_IncrementChainingTimeStampF1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
