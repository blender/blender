/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Simple API to draw debug shapes and log in the viewport.
 *
 * Both CPU and GPU implementation are supported and symmetrical (meaning GPU shader can use it
 * too, see common_draw_lib.glsl).
 *
 * NOTE: CPU logging will overlap GPU logging on screen as it is drawn after.
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_mutex.hh"

#include "DNA_object_types.h"

#include "draw_shader_shared.hh"

#include "DRW_gpu_wrapper.hh"

namespace blender::draw {

class View;

/**
 * Clear all debug visuals (regardless of visual's lifetime).
 *
 * Usually called before populating persistent data to override previous visuals.
 * Needs an active GPUContext.
 */
void drw_debug_clear();

/* Used for virtually infinite lifetime.
 * Useful for debugging render or baking jobs, or non-modal operators. */
constexpr uint drw_debug_persistent_lifetime = ~0u;

/**
 * Drawing functions that will draw wire-frames with the given color.
 *
 * IMPORTANT: `lifetime` is in unit of **display** and not in unit of time.
 * One display is defined as one call to `DebugDraw::display_to_view` which happens once
 * per 3D viewport if overlays are not turned off.
 *
 * - The default value of 1 is good for continuous event debugging in one viewport.
 * - Above 1 is a good value for infrequent events or to compare continuous event history.
 *   Alternatively also allows replicating the display to several viewport.
 * - drw_debug_persistent_lifetime is a good value for manually triggered event (e.g. an operator).
 *   It is best to clear the display cache (using `drw_debug_clear`) before adding new persistent
 *   visuals.
 *
 * All added debug drawing will be shared across viewports. If lifetime is greater than 1 or if a
 * viewport doesn't display the visuals it produced, the visuals will be displayed onto other
 * viewport(s).
 *
 * These functions are threadsafe and can be called concurrently at anytime, even outside the
 * UI redraw loop.
 */

void drw_debug_line(float3 v1, float3 v2, float4 color = {1, 0, 0, 1}, uint lifetime = 1);

void drw_debug_polygon(Span<float3> face_verts, float4 color = {1, 0, 0, 1}, uint lifetime = 1);

void drw_debug_bbox(const BoundBox &bbox, float4 color = {1, 0, 0, 1}, uint lifetime = 1);

void drw_debug_sphere(float3 center, float radius, float4 color = {1, 0, 0, 1}, uint lifetime = 1);
/** Same as drw_debug_sphere but with small default radius. */
void drw_debug_point(float3 pos, float rad = 0.01f, float4 col = {1, 0, 0, 1}, uint lifetime = 1);
/** Draw a matrix transform as 3 colored axes. */
void drw_debug_matrix(const float4x4 &m4, uint lifetime = 1);
/** Draw a matrix as a 2 units length bounding box, centered on origin. */
void drw_debug_matrix_as_bbox(const float4x4 &mat, float4 color = {1, 0, 0, 1}, uint lifetime = 1);

class DebugDraw {
 private:
  using DebugDrawBuf = StorageBuffer<DRWDebugDrawBuffer>;

  /**
   * Ensure thread-safety when adding geometry to the CPU debug buffer.
   * GPU debug buffer currently expects draw submission to be externally synchronized.
   */
  std::atomic<int> vertex_len_;
  /** Data buffers containing all verts or chars to draw. */
  SwapChain<DebugDrawBuf *, 2> cpu_draw_buf_ = {};
  SwapChain<DebugDrawBuf *, 2> gpu_draw_buf_ = {};
  /** True if the gpu buffer have been requested and may contain data to draw. */
  bool gpu_draw_buf_used = false;

  /* Reference counter used by GPUContext to allow freeing of DebugDrawBuf before the last
   * context is destroyed. */
  int ref_count_ = 0;
  Mutex ref_count_mutex_;

 public:
  void reset();

  /**
   * Draw all debug shapes to the given current view / frame-buffer.
   * Draw buffers will be emptied and ready for new debug data.
   */
  void display_to_view(View &view);

  /** Get GPU debug draw buffer. Can, return nullptr if WITH_DRAW_DEBUG is not enabled. */
  GPUStorageBuf *gpu_draw_buf_get();

  void acquire()
  {
    std::scoped_lock lock(ref_count_mutex_);
    ref_count_++;
    if (ref_count_ == 1) {
      reset();
    }
  }

  void release()
  {
    std::scoped_lock lock(ref_count_mutex_);
    ref_count_--;
    if (ref_count_ == 0) {
      clear_gpu_data();
    }
  }

  static DebugDraw &get()
  {
    static DebugDraw module;
    return module;
  }

  void draw_line(float3 v1, float3 v2, uint color, uint lifetime = 1);

  static uint color_pack(float4 color);

 private:
  void display_lines(View &view);

  void clear_gpu_data();
};

}  // namespace blender::draw
