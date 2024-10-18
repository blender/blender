/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Simple API to draw debug shapes in the viewport.
 */

#include "BKE_object.hh"
#include "BLI_link_utils.h"
#include "BLI_math_matrix.hh"
#include "GPU_batch.hh"
#include "GPU_capabilities.hh"
#include "GPU_debug.hh"

#include "draw_debug.hh"
#include "draw_debug_c.hh"
#include "draw_manager_c.hh"
#include "draw_shader.hh"
#include "draw_shader_shared.hh"

#include <iomanip>
#include <sstream>

#if defined(_DEBUG) || defined(WITH_DRAW_DEBUG)
#  define DRAW_DEBUG
#else
/* Uncomment to forcibly enable debug draw in release mode. */
// #define DRAW_DEBUG
#endif

namespace blender::draw {

/* -------------------------------------------------------------------- */
/** \name Init and state
 * \{ */

DebugDraw::DebugDraw()
{
  constexpr int circle_resolution = 16;
  for (auto axis : IndexRange(3)) {
    for (auto edge : IndexRange(circle_resolution)) {
      for (auto vert : IndexRange(2)) {
        const float angle = (2 * M_PI) * (edge + vert) / float(circle_resolution);
        const float point[3] = {cosf(angle), sinf(angle), 0.0f};
        sphere_verts_.append(
            float3(point[(0 + axis) % 3], point[(1 + axis) % 3], point[(2 + axis) % 3]));
      }
    }
  }

  constexpr int point_resolution = 4;
  for (auto axis : IndexRange(3)) {
    for (auto edge : IndexRange(point_resolution)) {
      for (auto vert : IndexRange(2)) {
        const float angle = (2 * M_PI) * (edge + vert) / float(point_resolution);
        const float point[3] = {cosf(angle), sinf(angle), 0.0f};
        point_verts_.append(
            float3(point[(0 + axis) % 3], point[(1 + axis) % 3], point[(2 + axis) % 3]));
      }
    }
  }
};

void DebugDraw::init()
{
  cpu_draw_buf_.command.vertex_len = 0;
  cpu_draw_buf_.command.vertex_first = 0;
  cpu_draw_buf_.command.instance_len = 1;
  cpu_draw_buf_.command.instance_first_array = 0;

  gpu_draw_buf_.command.vertex_len = 0;
  gpu_draw_buf_.command.vertex_first = 0;
  gpu_draw_buf_.command.instance_len = 1;
  gpu_draw_buf_.command.instance_first_array = 0;
  gpu_draw_buf_used = false;

  modelmat_reset();
}

void DebugDraw::modelmat_reset()
{
  model_mat_ = float4x4::identity();
}

void DebugDraw::modelmat_set(const float modelmat[4][4])
{
  model_mat_ = float4x4_view(modelmat);
}

GPUStorageBuf *DebugDraw::gpu_draw_buf_get()
{
  if (!gpu_draw_buf_used) {
    gpu_draw_buf_used = true;
    gpu_draw_buf_.push_update();
  }
  return gpu_draw_buf_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw functions
 * \{ */

void DebugDraw::draw_line(float3 v1, float3 v2, float4 color)
{
  draw_line(v1, v2, color_pack(color));
}

void DebugDraw::draw_polygon(Span<float3> face_verts, float4 color)
{
  BLI_assert(!face_verts.is_empty());

  uint col = color_pack(color);
  float3 v0 = math::transform_point(model_mat_, face_verts.last());
  for (auto vert : face_verts) {
    float3 v1 = math::transform_point(model_mat_, vert);
    draw_line(v0, v1, col);
    v0 = v1;
  }
}

void DebugDraw::draw_matrix(const float4x4 &m4)
{
  float3 v0 = float3(0.0f, 0.0f, 0.0f);
  float3 v1 = float3(1.0f, 0.0f, 0.0f);
  float3 v2 = float3(0.0f, 1.0f, 0.0f);
  float3 v3 = float3(0.0f, 0.0f, 1.0f);

  mul_project_m4_v3(m4.ptr(), v0);
  mul_project_m4_v3(m4.ptr(), v1);
  mul_project_m4_v3(m4.ptr(), v2);
  mul_project_m4_v3(m4.ptr(), v3);

  draw_line(v0, v1, float4(1.0f, 0.0f, 0.0f, 1.0f));
  draw_line(v0, v2, float4(0.0f, 1.0f, 0.0f, 1.0f));
  draw_line(v0, v3, float4(0.0f, 0.0f, 1.0f, 1.0f));
}

void DebugDraw::draw_bbox(const BoundBox &bbox, const float4 color)
{
  uint col = color_pack(color);
  draw_line(bbox.vec[0], bbox.vec[1], col);
  draw_line(bbox.vec[1], bbox.vec[2], col);
  draw_line(bbox.vec[2], bbox.vec[3], col);
  draw_line(bbox.vec[3], bbox.vec[0], col);

  draw_line(bbox.vec[4], bbox.vec[5], col);
  draw_line(bbox.vec[5], bbox.vec[6], col);
  draw_line(bbox.vec[6], bbox.vec[7], col);
  draw_line(bbox.vec[7], bbox.vec[4], col);

  draw_line(bbox.vec[0], bbox.vec[4], col);
  draw_line(bbox.vec[1], bbox.vec[5], col);
  draw_line(bbox.vec[2], bbox.vec[6], col);
  draw_line(bbox.vec[3], bbox.vec[7], col);
}

void DebugDraw::draw_matrix_as_bbox(const float4x4 &mat, const float4 color)
{
  BoundBox bb;
  const float min[3] = {-1.0f, -1.0f, -1.0f}, max[3] = {1.0f, 1.0f, 1.0f};
  BKE_boundbox_init_from_minmax(&bb, min, max);
  for (auto i : IndexRange(8)) {
    mul_project_m4_v3(mat.ptr(), bb.vec[i]);
  }
  draw_bbox(bb, color);
}

void DebugDraw::draw_sphere(const float3 center, float radius, const float4 color)
{
  uint col = color_pack(color);
  for (auto i : IndexRange(sphere_verts_.size() / 2)) {
    float3 v0 = sphere_verts_[i * 2] * radius + center;
    float3 v1 = sphere_verts_[i * 2 + 1] * radius + center;
    draw_line(v0, v1, col);
  }
}

void DebugDraw::draw_point(const float3 center, float radius, const float4 color)
{
  uint col = color_pack(color);
  for (auto i : IndexRange(point_verts_.size() / 2)) {
    float3 v0 = point_verts_[i * 2] * radius + center;
    float3 v1 = point_verts_[i * 2 + 1] * radius + center;
    draw_line(v0, v1, col);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internals
 *
 * IMPORTANT: All of these are copied from the shader libraries (`common_debug_draw_lib.glsl`).
 * They need to be kept in sync to write the same data.
 * \{ */

void DebugDraw::draw_line(float3 v1, float3 v2, uint color)
{
  DebugDrawBuf &buf = cpu_draw_buf_;
  uint index = buf.command.vertex_len;
  if (index + 2 < DRW_DEBUG_DRAW_VERT_MAX) {
    buf.verts[index + 0] = vert_pack(math::transform_point(model_mat_, v1), color);
    buf.verts[index + 1] = vert_pack(math::transform_point(model_mat_, v2), color);
    buf.command.vertex_len += 2;
  }
}

/* Keep in sync with drw_debug_color_pack(). */
uint DebugDraw::color_pack(float4 color)
{
  color = math::clamp(color, 0.0f, 1.0f);
  uint result = 0;
  result |= uint(color.x * 255.0f) << 0u;
  result |= uint(color.y * 255.0f) << 8u;
  result |= uint(color.z * 255.0f) << 16u;
  result |= uint(color.w * 255.0f) << 24u;
  return result;
}

DRWDebugVert DebugDraw::vert_pack(float3 pos, uint color)
{
  DRWDebugVert vert;
  vert.pos0 = *reinterpret_cast<uint32_t *>(&pos.x);
  vert.pos1 = *reinterpret_cast<uint32_t *>(&pos.y);
  vert.pos2 = *reinterpret_cast<uint32_t *>(&pos.z);
  vert.vert_color = color;
  return vert;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display
 * \{ */

void DebugDraw::display_lines()
{
  if (cpu_draw_buf_.command.vertex_len == 0 && gpu_draw_buf_used == false) {
    return;
  }
  GPU_debug_group_begin("Lines");
  cpu_draw_buf_.push_update();

  float4x4 persmat;
  const DRWView *view = DRW_view_get_active();
  DRW_view_persmat_get(view, persmat.ptr(), false);

  drw_state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);

  gpu::Batch *batch = drw_cache_procedural_lines_get();
  GPUShader *shader = DRW_shader_debug_draw_display_get();
  GPU_batch_set_shader(batch, shader);
  GPU_shader_uniform_mat4(shader, "persmat", persmat.ptr());

  if (gpu_draw_buf_used) {
    GPU_debug_group_begin("GPU");
    GPU_storagebuf_bind(gpu_draw_buf_, DRW_DEBUG_DRAW_SLOT);
    GPU_batch_draw_indirect(batch, gpu_draw_buf_, 0);
    GPU_storagebuf_unbind(gpu_draw_buf_);
    GPU_debug_group_end();
  }

  GPU_debug_group_begin("CPU");
  GPU_storagebuf_bind(cpu_draw_buf_, DRW_DEBUG_DRAW_SLOT);
  GPU_batch_draw_indirect(batch, cpu_draw_buf_, 0);
  GPU_storagebuf_unbind(cpu_draw_buf_);
  GPU_debug_group_end();

  GPU_debug_group_end();
}

void DebugDraw::display_to_view()
{
  GPU_debug_group_begin("DebugDraw");

  display_lines();
  /* Init again so we don't draw the same thing twice. */
  init();

  GPU_debug_group_end();
}

/** \} */

}  // namespace blender::draw

/* -------------------------------------------------------------------- */
/** \name DebugDraw Access
 * \{ */

blender::draw::DebugDraw *DRW_debug_get()
{
  return reinterpret_cast<blender::draw::DebugDraw *>(DST.debug);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name C-API private
 * \{ */

void drw_debug_draw()
{
#ifdef DRAW_DEBUG
  if (DST.debug == nullptr) {
    return;
  }
  /* TODO(@fclem): Convenience for now. Will have to move to #DRWManager. */
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->display_to_view();
#endif
}

/**
 * NOTE: Init is once per draw manager cycle.
 */
void drw_debug_init()
{
  /* Module should not be used in release builds. */
  /* TODO(@fclem): Hide the functions declarations without using `ifdefs` everywhere. */
#ifdef DRAW_DEBUG
  /* TODO(@fclem): Convenience for now. Will have to move to #DRWManager. */
  if (DST.debug == nullptr) {
    DST.debug = reinterpret_cast<DRWDebugModule *>(new blender::draw::DebugDraw());
  }
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->init();
#endif
}

void drw_debug_module_free(DRWDebugModule *module)
{
  if (module != nullptr) {
    delete reinterpret_cast<blender::draw::DebugDraw *>(module);
  }
}

GPUStorageBuf *drw_debug_gpu_draw_buf_get()
{
  return reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->gpu_draw_buf_get();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name C-API public
 * \{ */

void DRW_debug_modelmat_reset()
{
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->modelmat_reset();
}

void DRW_debug_modelmat(const float modelmat[4][4])
{
#ifdef DRAW_DEBUG
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->modelmat_set(modelmat);
#else
  UNUSED_VARS(modelmat);
#endif
}

void DRW_debug_line_v3v3(const float v1[3], const float v2[3], const float color[4])
{
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->draw_line(v1, v2, color);
}

void DRW_debug_polygon_v3(const float (*v)[3], int vert_len, const float color[4])
{
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->draw_polygon(
      blender::Span<float3>((float3 *)v, vert_len), color);
}

void DRW_debug_m4(const float m[4][4])
{
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->draw_matrix(float4x4(m));
}

void DRW_debug_m4_as_bbox(const float m[4][4], bool invert, const float color[4])
{
  blender::float4x4 m4(m);
  if (invert) {
    m4 = blender::math::invert(m4);
  }
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->draw_matrix_as_bbox(m4, color);
}

void DRW_debug_bbox(const BoundBox *bbox, const float color[4])
{
#ifdef DRAW_DEBUG
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->draw_bbox(*bbox, color);
#else
  UNUSED_VARS(bbox, color);
#endif
}

void DRW_debug_sphere(const float center[3], float radius, const float color[4])
{
  reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->draw_sphere(center, radius, color);
}

/** \} */
