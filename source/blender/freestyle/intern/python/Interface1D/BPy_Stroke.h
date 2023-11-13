/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Interface1D.h"

#include "../../stroke/Stroke.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Stroke_Type;

#define BPy_Stroke_Check(v) (((PyObject *)v)->ob_type == &Stroke_Type)

/*---------------------------Python BPy_Stroke structure definition----------*/
typedef struct {
  BPy_Interface1D py_if1D;
  Freestyle::Stroke *s;
} BPy_Stroke;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
