/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#pragma once

#include "BLI_sys_types.h"

namespace blender::gpu {
class Batch;
class IndexBuf;
class VertBuf;
}  // namespace blender::gpu

namespace blender::ed::vse {

struct ColorVertex;

/**
 * Flat-colored 2D geometry draw batching utility.
 *
 * Internally uses #GPU_SHADER_3D_FLAT_COLOR to draw single-colored rectangles, quads
 * or lines. After adding a number of primitives with #add_quad, #add_wire_quad, #add_line,
 * draw them using #draw. Note that #draw can be called behind the scenes if number of primitives
 * is larger than the internal batch buffer size.
 */
class SeqQuadsBatch {
 public:
  SeqQuadsBatch();
  ~SeqQuadsBatch();

  /** Draw all the previously added primitives. */
  void draw();
  /** Add an axis-aligned quad. */
  void add_quad(float x1, float y1, float x2, float y2, const uchar color[4])
  {
    add_quad(x1, y1, x1, y2, x2, y1, x2, y2, color, color, color, color);
  }
  /** Add a quad with four arbitrary coordinates and one color. */
  void add_quad(float x1,
                float y1,
                float x2,
                float y2,
                float x3,
                float y3,
                float x4,
                float y4,
                const uchar color[4])
  {
    add_quad(x1, y1, x2, y2, x3, y3, x4, y4, color, color, color, color);
  }
  /** Add a quad with four arbitrary coordinates and a color for each. */
  void add_quad(float x1,
                float y1,
                float x2,
                float y2,
                float x3,
                float y3,
                float x4,
                float y4,
                const uchar color1[4],
                const uchar color2[4],
                const uchar color3[4],
                const uchar color4[4]);
  /** Add four lines of an axis-aligned quad edges. */
  void add_wire_quad(float x1, float y1, float x2, float y2, const uchar color[4]);
  /** Add a line with single color. */
  void add_line(float x1, float y1, float x2, float y2, const uchar color[4])
  {
    add_line(x1, y1, x2, y2, color, color);
  }
  /** Add a line with two endpoint colors. */
  void add_line(
      float x1, float y1, float x2, float y2, const uchar color1[4], const uchar color2[4]);

 private:
  static constexpr int MAX_QUADS = 1024;
  static constexpr int MAX_LINES = 4096;

  gpu::VertBuf *vbo_quads = nullptr;
  gpu::IndexBuf *ibo_quads = nullptr;
  gpu::Batch *batch_quads = nullptr;
  ColorVertex *verts_quads = nullptr;
  int quads_num = 0;

  gpu::VertBuf *vbo_lines = nullptr;
  gpu::Batch *batch_lines = nullptr;
  ColorVertex *verts_lines = nullptr;
  int lines_num = 0;
};

}  // namespace blender::ed::vse
