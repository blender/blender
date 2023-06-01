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

#include <stdbool.h>

/**
 * For faster execution we keep a special dictionary for py-drivers, with
 * the needed modules and aliases.
 */
int bpy_pydriver_create_dict(void);
/**
 * For PyDrivers
 * (drivers using one-line Python expressions to express relationships between targets).
 */
extern PyObject *bpy_pydriver_Dict;

extern bool BPY_driver_secure_bytecode_test_ex(PyObject *expr_code,
                                               PyObject *namespace_array[],
                                               const bool verbose,
                                               const char *error_prefix);
extern bool BPY_driver_secure_bytecode_test(PyObject *expr_code,
                                            PyObject *namespace,
                                            const bool verbose);

#ifdef __cplusplus
}
#endif
