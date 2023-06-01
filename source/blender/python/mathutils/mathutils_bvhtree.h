/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup mathutils
 */

#pragma once

PyMODINIT_FUNC PyInit_mathutils_bvhtree(void);

extern PyTypeObject PyBVHTree_Type;

#define PyBVHTree_Check(v) PyObject_TypeCheck((v), &PyBVHTree_Type)
#define PyBVHTree_CheckExact(v) (Py_TYPE(v) == &PyBVHTree_Type)
