/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct IDProperty;

extern PyTypeObject BPy_IDPropertyUIManager_Type;

typedef struct BPy_IDPropertyUIManager {
  PyObject_VAR_HEAD
  struct IDProperty *property;
} BPy_IDPropertyUIManager;

void IDPropertyUIData_Init_Types(void);

#ifdef __cplusplus
}
#endif
