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

#include "draw_debug.h"
#include "draw_debug.hh"
#include "draw_manager.h"
#include "draw_shader.hh"
#include "draw_shader_shared.h"

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
        float point[3] = {cosf(angle), sinf(angle), 0.0f};
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
        float point[3] = {cosf(angle), sinf(angle), 0.0f};
        point_verts_.append(
            float3(point[(0 + axis) % 3], point[(1 + axis) % 3], point[(2 + axis) % 3]));
      }
    }
  }
};

void DebugDraw::init()
{
  cpu_print_buf_.command.vertex_len = 0;
  cpu_print_buf_.command.vertex_first = 0;
  cpu_print_buf_.command.instance_len = 1;
  cpu_print_buf_.command.instance_first_array = 0;

  cpu_draw_buf_.command.vertex_len = 0;
  cpu_draw_buf_.command.vertex_first = 0;
  cpu_draw_buf_.command.instance_len = 1;
  cpu_draw_buf_.command.instance_first_array = 0;

  gpu_print_buf_.command.vertex_len = 0;
  gpu_print_buf_.command.vertex_first = 0;
  gpu_print_buf_.command.instance_len = 1;
  gpu_print_buf_.command.instance_first_array = 0;
  gpu_print_buf_used = false;

  gpu_draw_buf_.command.vertex_len = 0;
  gpu_draw_buf_.command.vertex_first = 0;
  gpu_draw_buf_.command.instance_len = 1;
  gpu_draw_buf_.command.instance_first_array = 0;
  gpu_draw_buf_used = false;

  print_col_ = 0;
  print_row_ = 0;

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

GPUStorageBuf *DebugDraw::gpu_print_buf_get()
{
  if (!gpu_print_buf_used) {
    gpu_print_buf_used = true;
    gpu_print_buf_.push_update();
  }
  return gpu_print_buf_;
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
/** \name Print functions
 * \{ */

template<> void DebugDraw::print_value<uint>(const uint &value)
{
  print_value_uint(value, false, false, true);
}
template<> void DebugDraw::print_value<int>(const int &value)
{
  print_value_uint(uint(abs(value)), false, (value < 0), false);
}
template<> void DebugDraw::print_value<bool>(const bool &value)
{
  print_string(value ? "true " : "false");
}
template<> void DebugDraw::print_value<float>(const float &val)
{
  std::stringstream ss;
  ss << std::setw(12) << std::to_string(val);
  print_string(ss.str());
}
template<> void DebugDraw::print_value<double>(const double &val)
{
  print_value(float(val));
}

template<> void DebugDraw::print_value_hex<uint>(const uint &value)
{
  print_value_uint(value, true, false, false);
}
template<> void DebugDraw::print_value_hex<int>(const int &value)
{
  print_value_uint(uint(value), true, false, false);
}
template<> void DebugDraw::print_value_hex<float>(const float &value)
{
  print_value_uint(*reinterpret_cast<const uint *>(&value), true, false, false);
}
template<> void DebugDraw::print_value_hex<double>(const double &val)
{
  print_value_hex(float(val));
}

template<> void DebugDraw::print_value_binary<uint>(const uint &value)
{
  print_value_binary(value);
}
template<> void DebugDraw::print_value_binary<int>(const int &value)
{
  print_value_binary(uint(value));
}
template<> void DebugDraw::print_value_binary<float>(const float &value)
{
  print_value_binary(*reinterpret_cast<const uint *>(&value));
}
template<> void DebugDraw::print_value_binary<double>(const double &val)
{
  print_value_binary(float(val));
}

template<> void DebugDraw::print_value<float2>(const float2 &value)
{
  print_no_endl("float2(", value[0], ", ", value[1], ")");
}
template<> void DebugDraw::print_value<float3>(const float3 &value)
{
  print_no_endl("float3(", value[0], ", ", value[1], ", ", value[1], ")");
}
template<> void DebugDraw::print_value<float4>(const float4 &value)
{
  print_no_endl("float4(", value[0], ", ", value[1], ", ", value[2], ", ", value[3], ")");
}

template<> void DebugDraw::print_value<int2>(const int2 &value)
{
  print_no_endl("int2(", value[0], ", ", value[1], ")");
}
template<> void DebugDraw::print_value<int3>(const int3 &value)
{
  print_no_endl("int3(", value[0], ", ", value[1], ", ", value[1], ")");
}
template<> void DebugDraw::print_value<int4>(const int4 &value)
{
  print_no_endl("int4(", value[0], ", ", value[1], ", ", value[2], ", ", value[3], ")");
}

template<> void DebugDraw::print_value<uint2>(const uint2 &value)
{
  print_no_endl("uint2(", value[0], ", ", value[1], ")");
}
template<> void DebugDraw::print_value<uint3>(const uint3 &value)
{
  print_no_endl("uint3(", value[0], ", ", value[1], ", ", value[1], ")");
}
template<> void DebugDraw::print_value<uint4>(const uint4 &value)
{
  print_no_endl("uint4(", value[0], ", ", value[1], ", ", value[2], ", ", value[3], ")");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internals
 *
 * IMPORTANT: All of these are copied from the shader libraries (`common_debug_draw_lib.glsl` &
 * `common_debug_print_lib.glsl`). They need to be kept in sync to write the same data.
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

void DebugDraw::print_newline()
{
  print_col_ = 0u;
  print_row_ = ++cpu_print_buf_.command.instance_first_array;
}

void DebugDraw::print_string_start(uint len)
{
  /* Break before word. */
  if (print_col_ + len > DRW_DEBUG_PRINT_WORD_WRAP_COLUMN) {
    print_newline();
  }
}

/* Copied from gpu_shader_dependency. */
void DebugDraw::print_string(std::string str)
{
  size_t len_before_pad = str.length();
  /* Pad string to uint size to avoid out of bound reads. */
  while (str.length() % 4 != 0) {
    str += " ";
  }

  print_string_start(len_before_pad);
  for (size_t i = 0; i < len_before_pad; i += 4) {
    union {
      uint8_t chars[4];
      uint32_t word;
    };

    chars[0] = *(reinterpret_cast<const uint8_t *>(str.c_str()) + i + 0);
    chars[1] = *(reinterpret_cast<const uint8_t *>(str.c_str()) + i + 1);
    chars[2] = *(reinterpret_cast<const uint8_t *>(str.c_str()) + i + 2);
    chars[3] = *(reinterpret_cast<const uint8_t *>(str.c_str()) + i + 3);

    if (i + 4 > len_before_pad) {
      chars[len_before_pad - i] = '\0';
    }
    print_char4(word);
  }
}

/* Keep in sync with shader. */
void DebugDraw::print_char4(uint data)
{
  /* Convert into char stream. */
  for (; data != 0u; data >>= 8u) {
    uint char1 = data & 0xFFu;
    /* Check for null terminator. */
    if (char1 == 0x00) {
      break;
    }
    /* NOTE: Do not skip the header manually like in GPU. */
    uint cursor = cpu_print_buf_.command.vertex_len++;
    if (cursor < DRW_DEBUG_PRINT_MAX) {
      /* For future usage. (i.e: Color) */
      uint flags = 0u;
      uint col = print_col_++;
      uint print_header = (flags << 24u) | (print_row_ << 16u) | (col << 8u);
      cpu_print_buf_.char_array[cursor] = print_header | char1;
      /* Break word. */
      if (print_col_ > DRW_DEBUG_PRINT_WORD_WRAP_COLUMN) {
        print_newline();
      }
    }
  }
}

void DebugDraw::print_append_char(uint char1, uint &char4)
{
  char4 = (char4 << 8u) | char1;
}

void DebugDraw::print_append_digit(uint digit, uint &char4)
{
  const uint char_A = 0x41u;
  const uint char_0 = 0x30u;
  bool is_hexadecimal = digit > 9u;
  char4 = (char4 << 8u) | (is_hexadecimal ? (char_A + digit - 10u) : (char_0 + digit));
}

void DebugDraw::print_append_space(uint &char4)
{
  char4 = (char4 << 8u) | 0x20u;
}

void DebugDraw::print_value_binary(uint value)
{
  print_string("0b");
  print_string_start(10u * 4u);
  uint digits[10] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  uint digit = 0u;
  for (uint i = 0u; i < 32u; i++) {
    print_append_digit(((value >> i) & 1u), digits[digit / 4u]);
    digit++;
    if ((i % 4u) == 3u) {
      print_append_space(digits[digit / 4u]);
      digit++;
    }
  }
  /* Numbers are written from right to left. So we need to reverse the order. */
  for (int j = 9; j >= 0; j--) {
    print_char4(digits[j]);
  }
}

void DebugDraw::print_value_uint(uint value,
                                 const bool hex,
                                 bool is_negative,
                                 const bool is_unsigned)
{
  print_string_start(3u * 4u);
  const uint blank_value = hex ? 0x30303030u : 0x20202020u;
  const uint prefix = hex ? 0x78302020u : 0x20202020u;
  uint digits[3] = {blank_value, blank_value, prefix};
  const uint base = hex ? 16u : 10u;
  uint digit = 0u;
  /* Add `u` suffix. */
  if (is_unsigned) {
    print_append_char('u', digits[digit / 4u]);
    digit++;
  }
  /* Number's digits. */
  for (; value != 0u || digit == uint(is_unsigned); value /= base) {
    print_append_digit(value % base, digits[digit / 4u]);
    digit++;
  }
  /* Add negative sign. */
  if (is_negative) {
    print_append_char('-', digits[digit / 4u]);
    digit++;
  }
  /* Need to pad to uint alignment because we are issuing chars in "reverse". */
  for (uint i = digit % 4u; i < 4u && i > 0u; i++) {
    print_append_space(digits[digit / 4u]);
    digit++;
  }
  /* Numbers are written from right to left. So we need to reverse the order. */
  for (int j = 2; j >= 0; j--) {
    print_char4(digits[j]);
  }
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

  GPUBatch *batch = drw_cache_procedural_lines_get();
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

void DebugDraw::display_prints()
{
  if (cpu_print_buf_.command.vertex_len == 0 && gpu_print_buf_used == false) {
    return;
  }
  GPU_debug_group_begin("Prints");
  cpu_print_buf_.push_update();

  drw_state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_PROGRAM_POINT_SIZE);

  GPUBatch *batch = drw_cache_procedural_points_get();
  GPUShader *shader = DRW_shader_debug_print_display_get();
  GPU_batch_set_shader(batch, shader);
  float f_viewport[4];
  GPU_viewport_size_get_f(f_viewport);
  GPU_shader_uniform_2fv(shader, "viewport_size", &f_viewport[2]);

  if (gpu_print_buf_used) {
    GPU_debug_group_begin("GPU");
    GPU_storagebuf_bind(gpu_print_buf_, DRW_DEBUG_PRINT_SLOT);
    GPU_batch_draw_indirect(batch, gpu_print_buf_, 0);
    GPU_storagebuf_unbind(gpu_print_buf_);
    GPU_debug_group_end();
  }

  GPU_debug_group_begin("CPU");
  GPU_storagebuf_bind(cpu_print_buf_, DRW_DEBUG_PRINT_SLOT);
  GPU_batch_draw_indirect(batch, cpu_print_buf_, 0);
  GPU_storagebuf_unbind(cpu_print_buf_);
  GPU_debug_group_end();

  GPU_debug_group_end();
}

void DebugDraw::display_to_view()
{
  GPU_debug_group_begin("DebugDraw");

  display_lines();
  /* Print 3D shapes before text to avoid overlaps. */
  display_prints();
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

GPUStorageBuf *drw_debug_gpu_print_buf_get()
{
  return reinterpret_cast<blender::draw::DebugDraw *>(DST.debug)->gpu_print_buf_get();
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
