/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Helpers for GPU / drawing debugging.
 *
 *
 * GPU debug capture usage example:
 *
 * ### Instant frame capture. ###
 *
 * Will trigger a capture and load it inside RenderDoc or Xcode.
 *
 * \code
 * #include "GPU_debug.hh"
 *
 * void render_function()
 * {
 *   GPU_debug_capture_begin(__func__);
 *   // Draw-call submission goes here.
 *   GPU_debug_capture_end();
 * }
 * \endcode
 *
 * ### Capture scopes. ###
 *
 * Capture scope can be sprinkled around the codebase for easier selective capture.
 *
 * They are listed from inside Xcode (on Mac) when doing a Metal capture.
 *
 * OpenGL and Vulkan backend need to use the `--debug-gpu-scope-capture` launch argument to specify
 * which scope to capture. Building with RenderDoc API support is required for this launch option
 * to be available.
 *
 * They can be nested but only one can be captured at a time.
 *
 * \code
 * #include "GPU_debug.hh"
 *
 * void render_function()
 * {
 *   static gpu::DebugScope capture_scope = {"UniqueName"};
 *
 *   // Manually triggered version, better for conditional capture.
 *   capture_scope.begin();
 *   // Draw-call submission goes here.
 *   capture_scope.end();
 *
 *   {
 *     // Scoped version, better for complex control flow.
 *     static gpu::DebugScope capture_scope = {"AnotherUniqueName"};
 *     capture_scope.scoped_capture();
 *     // Draw-call submission goes here.
 *   }
 * }
 * \endcode
 */

#pragma once

#include "BLI_index_range.hh"

#include <string>

#define GPU_DEBUG_SHADER_COMPILATION_GROUP "Shader Compilation"
#define GPU_DEBUG_SHADER_SPECIALIZATION_GROUP "Shader Specialization"

void GPU_debug_group_begin(const char *name);
void GPU_debug_group_end();
/**
 * Return a formatted string showing the current group hierarchy in this format:
 * "Group1 > Group 2 > Group3 > ... > GroupN : "
 */
void GPU_debug_get_groups_names(int name_buf_len, char *r_name_buf);
std::string GPU_debug_get_groups_names(blender::IndexRange levels = blender::IndexRange(0, 9999));
/**
 * Return true if inside a debug group with the same name.
 */
bool GPU_debug_group_match(const char *ref);

/**
 * GPU Frame capture support.
 *
 * Allows instantaneous frame capture of GPU calls between begin/end.
 *
 * \param title: Optional title to set for the frame capture.
 */
void GPU_debug_capture_begin(const char *title);
void GPU_debug_capture_end();

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

namespace blender::gpu {

/**
 * Need to be declared as static with a unique identifier string.
 */
struct DebugScope {
  void *scope;

  DebugScope(const char *identifier)
  {
    scope = GPU_debug_capture_scope_create(identifier);
  }

  void begin_capture()
  {
    GPU_debug_capture_scope_begin(scope);
  }

  void end_capture()
  {
    GPU_debug_capture_scope_end(scope);
  }

  struct ScopedCapture {
    void *scope;

    ScopedCapture(void *scope) : scope(scope)
    {
      GPU_debug_capture_scope_begin(scope);
    }
    ~ScopedCapture()
    {
      GPU_debug_capture_scope_end(scope);
    }
  };

  /* Capture everything until the end of the scope. */
  ScopedCapture scoped_capture()
  {
    return ScopedCapture(scope);
  }
};

}  // namespace blender::gpu
