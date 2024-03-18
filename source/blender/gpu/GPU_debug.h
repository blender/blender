/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Helpers for GPU / drawing debugging.
 *
 *
 ** GPU debug capture usage example:
 *
 ** Instant frame capture. **
 *
 * \code
 * #include "GPU_debug.h"
 * static void do_render_engine(Render *re)
 * {
 *   GPU_debug_capture_begin();
 *   RE_engine_render(re, false);
 *   GPU_debug_capture_end();
 * }
 * \endcode
 *
 ** Capture scopes. **
 *
 * \code
 * #include "GPU_debug.h"
 * void *capture_scope = nullptr;
 * static void do_render_engine(Render *re)
 * {
 *   if (!capture_scope) {
 *     // Create capture scope which will display in external tool.
 *     capture_scope = GPU_debug_capture_scope_create("Render Frame");
 *   }
 *
 *   // Commands within scope boundary captured when requested in tool.
 *   GPU_debug_capture_scope_begin(capture_scope);
 *   RE_engine_render(re, false);
 *   GPU_debug_capture_scope_end(capture_scope);
 * }
 * \endcode
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

/**
 * GPU Frame capture support.
 *
 * Allows instantaneous frame capture of GPU calls between begin/end.
 */
void GPU_debug_capture_begin(void);
void GPU_debug_capture_end(void);

/**
 * GPU debug frame capture scopes.
 *
 * Allows creation of a GPU frame capture scope that define a region within which an external GPU
 * Frame capture tool can perform a deferred capture of GPU API calls within the boundary upon user
 * request.
 *
 * \param name: Unique name of capture scope displayed within capture tool.
 * \return pointer wrapping an API-specific capture scope object.
 * \note a capture scope should be created a single time and only used within one begin/end pair.
 */
void *GPU_debug_capture_scope_create(const char *name);

/**
 * Used to declare the region within which GPU calls are captured when the scope is triggered.
 *
 * \param scope: Pointer to capture scope object created with GPU_debug_capture_scope_create.
 * \return True if the capture tool is actively capturing this scope when function is executed.
 * Otherwise, False.
 */
bool GPU_debug_capture_scope_begin(void *scope);
void GPU_debug_capture_scope_end(void *scope);

#ifdef __cplusplus
}
#endif
