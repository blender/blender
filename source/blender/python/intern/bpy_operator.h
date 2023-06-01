/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern PyTypeObject pyop_base_Type;

#define BPy_OperatorBase_Check(v) (PyObject_TypeCheck(v, &pyop_base_Type))

typedef struct {
  PyObject_HEAD /* Required Python macro. */
} BPy_OperatorBase;

PyObject *BPY_operator_module(void);

#ifdef __cplusplus
}
#endif
