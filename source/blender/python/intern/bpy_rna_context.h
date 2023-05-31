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

extern PyMethodDef BPY_rna_context_temp_override_method_def;

void bpy_rna_context_types_init(void);

#ifdef __cplusplus
}
#endif
