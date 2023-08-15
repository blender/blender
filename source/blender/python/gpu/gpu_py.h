/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "../generic/py_capi_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct PyC_StringEnumItems bpygpu_primtype_items[];
extern struct PyC_StringEnumItems bpygpu_dataformat_items[];

PyObject *bpygpu_create_module(PyModuleDef *module_type);
int bpygpu_finalize_type(PyTypeObject *py_type);

#ifdef __cplusplus
}
#endif
