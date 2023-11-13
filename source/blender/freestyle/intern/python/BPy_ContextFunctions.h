/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

extern "C" {
#include <Python.h>
}

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------Python BPy_ContextFunctions visible prototypes-----------*/

int ContextFunctions_Init(PyObject *module);

#ifdef __cplusplus
}
#endif
