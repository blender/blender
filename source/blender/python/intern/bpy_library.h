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

int BPY_library_load_type_ready(void);
extern PyMethodDef BPY_library_load_method_def;

extern PyMethodDef BPY_library_write_method_def;

#ifdef __cplusplus
}
#endif
