/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;

/** Creates the `bpy` module and adds it to `sys.modules` for importing. */
void BPy_init_modules(struct bContext *C);

extern PyObject *bpy_package_py;

/* `bpy_interface_atexit.cc` */

void BPY_atexit_register(void);
void BPY_atexit_unregister(void);

extern struct CLG_LogRef *BPY_LOG_CONTEXT;
extern struct CLG_LogRef *BPY_LOG_RNA;
extern struct CLG_LogRef *BPY_LOG_INTERFACE;

#ifdef __cplusplus
}
#endif
