/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

extern "C" {
#include <Python.h>
}

#include "../geometry/Noise.h"
#include "../system/PseudoNoise.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject FrsNoise_Type;

#define BPy_FrsNoise_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&FrsNoise_Type))

/*---------------------------Python BPy_FrsNoise structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::Noise *n;
  Freestyle::PseudoNoise *pn;
} BPy_FrsNoise;

/*---------------------------Python BPy_FrsNoise visible prototypes-----------*/

int FrsNoise_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
