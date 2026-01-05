/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DViewShape.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject GetOccludeeF0D_Type;

#define BPy_GetOccludeeF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GetOccludeeF0D_Type))

/*---------------------------Python BPy_GetOccludeeF0D structure definition----------*/
struct BPy_GetOccludeeF0D {
  BPy_UnaryFunction0DViewShape py_uf0D_viewshape;
};

///////////////////////////////////////////////////////////////////////////////////////////
