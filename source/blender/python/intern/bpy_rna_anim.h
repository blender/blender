/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup pythonintern
 */

#ifdef __cplusplus
extern "C" {
#endif

extern char pyrna_struct_keyframe_insert_doc[];
extern char pyrna_struct_keyframe_delete_doc[];
extern char pyrna_struct_driver_add_doc[];
extern char pyrna_struct_driver_remove_doc[];

PyObject *pyrna_struct_keyframe_insert(BPy_StructRNA *self, PyObject *args, PyObject *kw);
PyObject *pyrna_struct_keyframe_delete(BPy_StructRNA *self, PyObject *args, PyObject *kw);
PyObject *pyrna_struct_driver_add(BPy_StructRNA *self, PyObject *args);
PyObject *pyrna_struct_driver_remove(BPy_StructRNA *self, PyObject *args);

#ifdef __cplusplus
}
#endif
