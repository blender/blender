/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#include <Python.h>

namespace blender {

/**
 * Extend RNA types with C-API defined methods, properties and types.
 *
 * \param bpy_types: The `bpy.types` module, used to populate C-API defined types.
 */
void BPY_rna_types_extend_capi(PyObject *bpy_types);

}  // namespace blender
