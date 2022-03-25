/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
