/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject BackboneStretcherShader_Type;

#define BPy_BackboneStretcherShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&BackboneStretcherShader_Type))

/*---------------------------Python BPy_BackboneStretcherShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_BackboneStretcherShader;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
