/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DDouble.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Curvature2DAngleF0D_Type;

#define BPy_Curvature2DAngleF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&Curvature2DAngleF0D_Type))

/*---------------------------Python BPy_Curvature2DAngleF0D structure definition----------*/
struct BPy_Curvature2DAngleF0D {
  BPy_UnaryFunction0DDouble py_uf0D_double;
};

///////////////////////////////////////////////////////////////////////////////////////////
