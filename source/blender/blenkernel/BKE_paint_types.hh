/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"

/** \file
 * \ingroup bke
 */

namespace blender {
namespace ocio {
class ColorSpace;
}
}  // namespace blender
struct AssetWeakReference;
enum class PaintMode : int8_t {
  Sculpt = 0,
  /** Vertex color. */
  Vertex = 1,
  Weight = 2,
  /** 3D view (projection painting). */
  Texture3D = 3,
  /** Image space (2D painting). */
  Texture2D = 4,
  GPencil = 6,
  /* Grease Pencil Vertex Paint */
  VertexGPencil = 7,
  SculptGPencil = 8,
  WeightGPencil = 9,
  /** Curves. */
  SculptCurves = 10,

  /** Keep last. */
  /* TODO: Shift the ordering so that invalid is first so that zero-initialization makes sense. */
  Invalid = 11,
};

namespace blender::bke {
struct PaintRuntime : NonCopyable, NonMovable {
  bool initialized = false;
  uint16_t ob_mode = 0;
  PaintMode paint_mode = PaintMode::Invalid;
  AssetWeakReference *previous_active_brush_reference = nullptr;

  blender::float2 last_rake = float2(0.0f, 0.0f);
  float last_rake_angle = 0.0f;

  int last_stroke_valid = false;
  blender::float3 average_stroke_accum = float3(0.0f, 0.0f, 0.0f);
  int average_stroke_counter = 0;

  /**
   * How much brush should be rotated in the view plane, 0 means x points right, y points up.
   * The convention is that the brush's _negative_ Y axis points in the tangent direction (of the
   * mouse curve, Bezier curve, etc.)
   */
  float brush_rotation = 0.0f;
  float brush_rotation_sec = 0.0f;

  /*******************************************************************************
   * all data below are used to communicate with cursor drawing and tex sampling *
   *******************************************************************************/
  bool draw_anchored = false;
  int anchored_size = 0;

  /**
   * Normalization factor due to accumulated value of curve along spacing.
   * Calculated when brush spacing changes to dampen strength of stroke
   * if space attenuation is used.
   */
  float overlap_factor = 0.0f;
  bool draw_inverted = false;
  /** Check is there an ongoing stroke right now. */
  bool stroke_active = false;

  /**
   * Store last location of stroke or whether the mesh was hit.
   * Valid only while stroke is active.
   */
  blender::float3 last_location = float3(0.0f, 0.0f, 0.0f);
  bool last_hit = false;

  blender::float2 anchored_initial_mouse = float2(0.0f, 0.0f);

  /**
   * Radius of brush, pre-multiplied with pressure.
   * In case of anchored brushes contains the anchored radius.
   */
  float pixel_radius = 0.0f;
  float initial_pixel_radius = 0.0f;
  float start_pixel_radius = 0.0f;

  /** Evaluated size pressure value */
  float size_pressure_value = 0.0f;

  /** Position of mouse, used to sample the texture. */
  blender::float2 tex_mouse = float2(0.0f, 0.0f);

  /** Position of mouse, used to sample the mask texture. */
  blender::float2 mask_tex_mouse = float2(0.0f, 0.0f);

  /** ColorSpace cache to avoid locking up during sampling. */
  bool do_linear_conversion = false;
  const blender::ocio::ColorSpace *colorspace = nullptr;

  /** WM Paint cursor. */
  void *paint_cursor = nullptr;

  PaintRuntime();
  ~PaintRuntime();
};
};  // namespace blender::bke
