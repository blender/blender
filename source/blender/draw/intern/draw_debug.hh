/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Simple API to draw debug shapes and log in the viewport.
 *
 * Both CPU and GPU implementation are supported and symmetrical (meaning GPU shader can use it
 * too, see common_debug_print/draw_lib.glsl).
 *
 * NOTE: CPU logging will overlap GPU logging on screen as it is drawn after.
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "DNA_object_types.h"
#include "DRW_gpu_wrapper.hh"

namespace blender::draw {

/* Shortcuts to avoid boilerplate code and match shader API. */
#define drw_debug_line(...) DRW_debug_get()->draw_line(__VA_ARGS__)
#define drw_debug_polygon(...) DRW_debug_get()->draw_polygon(__VA_ARGS__)
#define drw_debug_bbox(...) DRW_debug_get()->draw_bbox(__VA_ARGS__)
#define drw_debug_sphere(...) DRW_debug_get()->draw_sphere(__VA_ARGS__)
#define drw_debug_point(...) DRW_debug_get()->draw_point(__VA_ARGS__)
#define drw_debug_matrix(...) DRW_debug_get()->draw_matrix(__VA_ARGS__)
#define drw_debug_matrix_as_bbox(...) DRW_debug_get()->draw_matrix_as_bbox(__VA_ARGS__)
#define drw_print(...) DRW_debug_get()->print(__VA_ARGS__)
#define drw_print_hex(...) DRW_debug_get()->print_hex(__VA_ARGS__)
#define drw_print_binary(...) DRW_debug_get()->print_binary(__VA_ARGS__)
#define drw_print_no_endl(...) DRW_debug_get()->print_no_endl(__VA_ARGS__)

/* Will log variable along with its name, like the shader version of print(). */
#define drw_print_id(v_) DRW_debug_get()->print(#v_, "= ", v_)
#define drw_print_id_no_endl(v_) DRW_debug_get()->print_no_endl(#v_, "= ", v_)

class DebugDraw {
 private:
  using DebugDrawBuf = StorageBuffer<DRWDebugDrawBuffer>;
  using DebugPrintBuf = StorageBuffer<DRWDebugPrintBuffer>;

  /** Data buffers containing all verts or chars to draw. */
  DebugDrawBuf cpu_draw_buf_ = {"DebugDrawBuf-CPU"};
  DebugDrawBuf gpu_draw_buf_ = {"DebugDrawBuf-GPU"};
  DebugPrintBuf cpu_print_buf_ = {"DebugPrintBuf-CPU"};
  DebugPrintBuf gpu_print_buf_ = {"DebugPrintBuf-GPU"};
  /** True if the gpu buffer have been requested and may contain data to draw. */
  bool gpu_print_buf_used = false;
  bool gpu_draw_buf_used = false;
  /** Matrix applied to all points before drawing. Could be a stack if needed. */
  float4x4 model_mat_;
  /** Precomputed shapes verts. */
  Vector<float3> sphere_verts_;
  Vector<float3> point_verts_;
  /** Cursor position for print functionality. */
  uint print_col_ = 0;
  uint print_row_ = 0;

 public:
  DebugDraw();
  ~DebugDraw(){};

  /**
   * Resets all buffers and reset model matrix state.
   * Not to be called by user.
   */
  void init();

  /**
   * Resets model matrix state to identity.
   */
  void modelmat_reset();
  /**
   * Sets model matrix transform to apply to any vertex passed to drawing functions.
   */
  void modelmat_set(const float modelmat[4][4]);

  /**
   * Drawing functions that will draw wire-frames with the given color.
   */
  void draw_line(float3 v1, float3 v2, float4 color = {1, 0, 0, 1});
  void draw_polygon(Span<float3> face_verts, float4 color = {1, 0, 0, 1});
  void draw_bbox(const BoundBox &bbox, const float4 color = {1, 0, 0, 1});
  void draw_sphere(const float3 center, float radius, const float4 color = {1, 0, 0, 1});
  void draw_point(const float3 center, float radius = 0.01f, const float4 color = {1, 0, 0, 1});
  /**
   * Draw a matrix transformation as 3 colored axes.
   */
  void draw_matrix(const float4x4 &m4);
  /**
   * Draw a matrix as a 2 units length bounding box, centered on origin.
   */
  void draw_matrix_as_bbox(const float4x4 &mat, const float4 color = {1, 0, 0, 1});

  /**
   * Will draw all debug shapes and text cached up until now to the current view / frame-buffer.
   * Draw buffers will be emptied and ready for new debug data.
   */
  void display_to_view();

  /**
   * Log variable or strings inside the viewport.
   * Using a unique non string argument will print the variable name with it.
   * Concatenate by using multiple arguments. i.e: `print("Looped ", n, "times.")`.
   */
  template<typename... Ts> void print(StringRefNull str, Ts... args)
  {
    print_no_endl(str, args...);
    print_newline();
  }
  template<typename T> void print(const T &value)
  {
    print_value(value);
    print_newline();
  }
  template<typename T> void print_hex(const T &value)
  {
    print_value_hex(value);
    print_newline();
  }
  template<typename T> void print_binary(const T &value)
  {
    print_value_binary(value);
    print_newline();
  }

  /**
   * Same as `print()` but does not finish the line.
   */
  void print_no_endl(std::string arg)
  {
    print_string(arg);
  }
  void print_no_endl(StringRef arg)
  {
    print_string(arg);
  }
  void print_no_endl(StringRefNull arg)
  {
    print_string(arg);
  }
  void print_no_endl(char const *arg)
  {
    print_string(StringRefNull(arg));
  }
  template<typename T> void print_no_endl(T arg)
  {
    print_value(arg);
  }
  template<typename T, typename... Ts> void print_no_endl(T arg, Ts... args)
  {
    print_no_endl(arg);
    print_no_endl(args...);
  }

  /**
   * Not to be called by user. Should become private.
   */
  GPUStorageBuf *gpu_draw_buf_get();
  GPUStorageBuf *gpu_print_buf_get();

 private:
  uint color_pack(float4 color);
  DRWDebugVert vert_pack(float3 pos, uint color);

  void draw_line(float3 v1, float3 v2, uint color);

  void print_newline();
  void print_string_start(uint len);
  void print_string(std::string str);
  void print_char4(uint data);
  void print_append_char(uint char1, uint &char4);
  void print_append_digit(uint digit, uint &char4);
  void print_append_space(uint &char4);
  void print_value_binary(uint value);
  void print_value_uint(uint value, const bool hex, bool is_negative, const bool is_unsigned);

  template<typename T> void print_value(const T &value);
  template<typename T> void print_value_hex(const T &value);
  template<typename T> void print_value_binary(const T &value);

  void display_lines();
  void display_prints();
};

}  // namespace blender::draw

/**
 * Ease of use function to get the debug module.
 * TODO(fclem): Should be removed once DRWManager is no longer global.
 * IMPORTANT: Can return nullptr if storage buffer is not supported.
 */
blender::draw::DebugDraw *DRW_debug_get();
