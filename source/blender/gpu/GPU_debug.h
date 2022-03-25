/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * Helpers for GPU / drawing debugging.
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GPU_DEBUG_SHADER_COMPILATION_GROUP "Shader Compilation"

void GPU_debug_group_begin(const char *name);
void GPU_debug_group_end(void);
/**
 * Return a formatted string showing the current group hierarchy in this format:
 * "Group1 > Group 2 > Group3 > ... > GroupN : "
 */
void GPU_debug_get_groups_names(int name_buf_len, char *r_name_buf);
/**
 * Return true if inside a debug group with the same name.
 */
bool GPU_debug_group_match(const char *ref);

#ifdef __cplusplus
}
#endif
