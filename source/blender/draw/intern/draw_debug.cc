/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Simple API to draw debug shapes in the viewport.
 */

#include "BKE_object.hh"
#include "BLI_math_bits.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "GPU_batch.hh"
#include "GPU_debug.hh"

#include "draw_context_private.hh"
#include "draw_debug.hh"
#include "draw_shader.hh"
#include "draw_shader_shared.hh"

namespace blender::draw {

/* -------------------------------------------------------------------- */
/** \name Init and state
 * \{ */

void DebugDraw::reset()
{
  for (int i = 0; i < 2; i++) {
    vertex_len_.store(0);

    if (cpu_draw_buf_.current() == nullptr) {
      cpu_draw_buf_.current() = MEM_new<DebugDrawBuf>("DebugDrawBuf-CPU", "DebugDrawBuf-CPU");
      gpu_draw_buf_.current() = MEM_new<DebugDrawBuf>("DebugDrawBuf-GPU", "DebugDrawBuf-GPU");
    }

    cpu_draw_buf_.current()->command.vertex_len = 0;
    cpu_draw_buf_.current()->command.vertex_first = 0;
    cpu_draw_buf_.current()->command.instance_len = 1;
    cpu_draw_buf_.current()->command.instance_first_array = 0;

    gpu_draw_buf_.current()->command.vertex_len = 0;
    gpu_draw_buf_.current()->command.vertex_first = 0;
    gpu_draw_buf_.current()->command.instance_len = 1;
    gpu_draw_buf_.current()->command.instance_first_array = 0;
    gpu_draw_buf_.current()->push_update();

    cpu_draw_buf_.swap();
    gpu_draw_buf_.swap();
  }

  gpu_draw_buf_used = false;
}

GPUStorageBuf *DebugDraw::gpu_draw_buf_get()
{
#ifdef WITH_DRAW_DEBUG
  gpu_draw_buf_used = true;
  return *gpu_draw_buf_.current();
#else
  return nullptr;
#endif
}

void DebugDraw::clear_gpu_data()
{
  for (int i = 0; i < 2; i++) {
    MEM_SAFE_DELETE(cpu_draw_buf_.current());
    MEM_SAFE_DELETE(gpu_draw_buf_.current());

    cpu_draw_buf_.swap();
    gpu_draw_buf_.swap();
  }
}

void drw_debug_clear()
{
  DebugDraw::get().reset();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw functions
 * \{ */

void drw_debug_line(const float3 v1, const float3 v2, const float4 color, const uint lifetime)
{
  DebugDraw &dd = DebugDraw::get();
  dd.draw_line(v1, v2, debug_color_pack(color), lifetime);
}

void drw_debug_polygon(Span<float3> face_verts, const float4 color, const uint lifetime)
{
  BLI_assert(!face_verts.is_empty());
  DebugDraw &dd = DebugDraw::get();
  uint col = debug_color_pack(color);
  float3 v0 = face_verts.last();
  for (auto vert : face_verts) {
    float3 v1 = vert;
    dd.draw_line(v0, v1, col, lifetime);
    v0 = v1;
  }
}

void drw_debug_bbox(const BoundBox &bbox, const float4 color, const uint lifetime)
{
  DebugDraw &dd = DebugDraw::get();
  uint col = debug_color_pack(color);
  dd.draw_line(bbox.vec[0], bbox.vec[1], col, lifetime);
  dd.draw_line(bbox.vec[1], bbox.vec[2], col, lifetime);
  dd.draw_line(bbox.vec[2], bbox.vec[3], col, lifetime);
  dd.draw_line(bbox.vec[3], bbox.vec[0], col, lifetime);

  dd.draw_line(bbox.vec[4], bbox.vec[5], col, lifetime);
  dd.draw_line(bbox.vec[5], bbox.vec[6], col, lifetime);
  dd.draw_line(bbox.vec[6], bbox.vec[7], col, lifetime);
  dd.draw_line(bbox.vec[7], bbox.vec[4], col, lifetime);

  dd.draw_line(bbox.vec[0], bbox.vec[4], col, lifetime);
  dd.draw_line(bbox.vec[1], bbox.vec[5], col, lifetime);
  dd.draw_line(bbox.vec[2], bbox.vec[6], col, lifetime);
  dd.draw_line(bbox.vec[3], bbox.vec[7], col, lifetime);
}

static Vector<float3> precompute_sphere_points(int circle_resolution)
{
  Vector<float3> result;
  for (auto axis : IndexRange(3)) {
    for (auto edge : IndexRange(circle_resolution)) {
      for (auto vert : IndexRange(2)) {
        const float angle = (2 * M_PI) * (edge + vert) / float(circle_resolution);
        const float point[3] = {cosf(angle), sinf(angle), 0.0f};
        result.append(float3(point[(0 + axis) % 3], point[(1 + axis) % 3], point[(2 + axis) % 3]));
      }
    }
  }
  return result;
}

void drw_debug_sphere(const float3 center, float radius, const float4 color, const uint lifetime)
{
  /** Precomputed shapes verts. */
  static Vector<float3> sphere_verts = precompute_sphere_points(16);

  DebugDraw &dd = DebugDraw::get();
  uint col = debug_color_pack(color);
  for (auto i : IndexRange(sphere_verts.size() / 2)) {
    float3 v0 = sphere_verts[i * 2] * radius + center;
    float3 v1 = sphere_verts[i * 2 + 1] * radius + center;
    dd.draw_line(v0, v1, col, lifetime);
  }
}

void drw_debug_point(const float3 pos, float rad, const float4 col, const uint lifetime)
{
  static Vector<float3> point_verts = precompute_sphere_points(4);

  DebugDraw &dd = DebugDraw::get();
  uint color = debug_color_pack(col);
  for (auto i : IndexRange(point_verts.size() / 2)) {
    float3 v0 = point_verts[i * 2] * rad + pos;
    float3 v1 = point_verts[i * 2 + 1] * rad + pos;
    dd.draw_line(v0, v1, color, lifetime);
  }
}

void drw_debug_matrix(const float4x4 &m4, const uint lifetime)
{
  float3 v0 = math::transform_point(m4, float3(0.0f, 0.0f, 0.0f));
  float3 v1 = math::transform_point(m4, float3(1.0f, 0.0f, 0.0f));
  float3 v2 = math::transform_point(m4, float3(0.0f, 1.0f, 0.0f));
  float3 v3 = math::transform_point(m4, float3(0.0f, 0.0f, 1.0f));

  DebugDraw &dd = DebugDraw::get();
  dd.draw_line(v0, v1, debug_color_pack(float4(1.0f, 0.0f, 0.0f, 1.0f)), lifetime);
  dd.draw_line(v0, v2, debug_color_pack(float4(0.0f, 1.0f, 0.0f, 1.0f)), lifetime);
  dd.draw_line(v0, v3, debug_color_pack(float4(0.0f, 0.0f, 1.0f, 1.0f)), lifetime);
}

void drw_debug_matrix_as_bbox(const float4x4 &mat, const float4 color, const uint lifetime)
{
  BoundBox bb;
  std::array<float3, 8> corners = bounds::corners(Bounds<float3>(float3(-1), float3(1)));
  for (auto i : IndexRange(8)) {
    mul_project_m4_v3(mat.ptr(), corners[i]);
  }
  drw_debug_bbox(bb, color, lifetime);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internals
 *
 * \{ */

void DebugDraw::draw_line(float3 v1, float3 v2, uint color, const uint lifetime)
{
  DebugDrawBuf &buf = *cpu_draw_buf_.current();
  uint index = vertex_len_.fetch_add(2);
  if (index + 2 < DRW_DEBUG_DRAW_VERT_MAX) {
    buf.verts[index / 2] = debug_line_make(float_as_uint(v1.x),
                                           float_as_uint(v1.y),
                                           float_as_uint(v1.z),
                                           float_as_uint(v2.x),
                                           float_as_uint(v2.y),
                                           float_as_uint(v2.z),
                                           color,
                                           lifetime);
    buf.command.vertex_len += 2;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display
 * \{ */

void DebugDraw::display_lines(View &view)
{
  const bool cpu_draw_buf_used = vertex_len_.load() != 0;

  if (!cpu_draw_buf_used && !gpu_draw_buf_used) {
    return;
  }

  command::StateSet::set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);

  gpu::Batch *batch = GPU_batch_procedural_lines_get();
  GPUShader *shader = DRW_shader_debug_draw_display_get();
  GPU_batch_set_shader(batch, shader);
  GPU_shader_uniform_mat4(shader, "persmat", view.persmat().ptr());
  GPU_shader_uniform_2f(shader, "size_viewport", viewport_size[2], viewport_size[3]);

  if (gpu_draw_buf_used) {
    GPU_debug_group_begin("GPU");
    /* Reset buffer. */
    gpu_draw_buf_.next()->command.vertex_len = 0;
    gpu_draw_buf_.next()->push_update();

    GPU_storagebuf_bind(*gpu_draw_buf_.current(), DRW_DEBUG_DRAW_SLOT);
    GPU_storagebuf_bind(*gpu_draw_buf_.next(), DRW_DEBUG_DRAW_FEEDBACK_SLOT);
    GPU_batch_draw_indirect(batch, *gpu_draw_buf_.current(), 0);
    GPU_storagebuf_unbind(*gpu_draw_buf_.current());
    GPU_storagebuf_unbind(*gpu_draw_buf_.next());
    GPU_debug_group_end();
  }

  {
    GPU_debug_group_begin("CPU");
    /* We might have race condition here (a writer thread might still be outputting vertices).
     * But that is ok. At worse, we will be missing some vertex data and show 1 corrupted line. */
    cpu_draw_buf_.current()->command.vertex_len = vertex_len_.load();
    cpu_draw_buf_.current()->push_update();
    /* Reset buffer. */
    cpu_draw_buf_.next()->command.vertex_len = 0;
    cpu_draw_buf_.next()->push_update();

    GPU_storagebuf_bind(*cpu_draw_buf_.current(), DRW_DEBUG_DRAW_SLOT);
    GPU_storagebuf_bind(*cpu_draw_buf_.next(), DRW_DEBUG_DRAW_FEEDBACK_SLOT);
    GPU_batch_draw_indirect(batch, *cpu_draw_buf_.current(), 0);
    GPU_storagebuf_unbind(*cpu_draw_buf_.current());
    GPU_storagebuf_unbind(*cpu_draw_buf_.next());

    /* Read result of lifetime management. */
    cpu_draw_buf_.next()->read();
    vertex_len_.store(min_ii(DRW_DEBUG_DRAW_VERT_MAX, cpu_draw_buf_.next()->command.vertex_len));
    GPU_debug_group_end();
  }

  gpu_draw_buf_.swap();
  cpu_draw_buf_.swap();
}

void DebugDraw::display_to_view(View &view)
{
  /* Display only on the main thread. Avoid concurrent usage of the resource. */
  BLI_assert(BLI_thread_is_main());

  GPU_debug_group_begin("DebugDraw");

  display_lines(view);

  GPU_debug_group_end();
}

/** \} */

}  // namespace blender::draw
