/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.hh"
#include "BLI_utility_mixins.hh"

namespace blender {
namespace gpu {
class Texture;
}

/** \file
 * \ingroup bke
 */

namespace ocio {
class ColorSpace;
}
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

namespace bke {
namespace paint {
/**
 * Represents the essential library file needed for a given runtime.
 */
enum class AssetCategory : int8_t {
  Invalid = 0,
  CurvesSculpt = 1,
  GPencilDraw = 2,
  GPencilSculpt = 3,
  GPencilVertex = 4,
  GPencilWeight = 5,
  MeshSculpt = 6,
  MeshTexture = 7,
  MeshVertex = 8,
  MeshWeight = 9,
};

enum class eOverlayControlFlags : uint8_t {
  InvalidTexturePrimary = 1,
  InvalidTextureSecondary = (1 << 2),
  InvalidCurve = (1 << 3),
  InvalidMask = InvalidTexturePrimary | InvalidTextureSecondary | InvalidCurve,
  OverrideCursor = (1 << 4),
  OverridePrimary = (1 << 5),
  OverrideSecondary = (1 << 6),
  OverrideMask = OverrideCursor | OverridePrimary | OverrideSecondary,
};
ENUM_OPERATORS(eOverlayControlFlags);

struct TexSnapshot {
  gpu::Texture *overlay_texture = nullptr;
  int winx = 0;
  int winy = 0;
  int old_size = 0;
  float old_zoom = 0.0f;
  bool old_col = false;

  TexSnapshot() = default;
  ~TexSnapshot();
};

struct CursorSnapshot {
  gpu::Texture *overlay_texture = nullptr;
  int size = 0;
  int zoom = 0;
  int curve_preset = 0;

  CursorSnapshot() = default;
  ~CursorSnapshot();
};
};  // namespace paint

struct PaintRuntime : NonCopyable, NonMovable {
  bool initialized = false;
  uint16_t ob_mode = 0;
  paint::AssetCategory asset_category = paint::AssetCategory::Invalid;
  AssetWeakReference *previous_active_brush_reference = nullptr;

  float2 last_rake = float2(0.0f, 0.0f);
  float last_rake_angle = 0.0f;

  int last_stroke_valid = false;
  float3 average_stroke_accum = float3(0.0f, 0.0f, 0.0f);
  int average_stroke_counter = 0;

  /**
   * How much brush should be rotated in the view plane, 0 means x points right, y points up.
   * The convention is that the brush's _negative_ Y axis points in the tangent direction (of the
   * mouse curve, Bezier curve, etc.)
   */
  float brush_rotation = 0.0f;
  float brush_rotation_sec = 0.0f;

  /*******************************************************************************
   * All data below are used to communicate with cursor drawing and tex sampling *
   *******************************************************************************/

  bool draw_anchored = false;
  int anchored_size = 0;

  /**
   * Normalization factor due to accumulated value of curve along spacing.
   * Calculated when brush spacing changes to dampen strength of stroke
   * if space attenuation is used.
   */
  float overlap_factor = 0.0f;
  /** Check is there an ongoing stroke right now. */
  bool stroke_active = false;

  /**
   * Store last location of stroke or whether the mesh was hit.
   * Valid only while stroke is active.
   */
  float3 last_location = float3(0.0f, 0.0f, 0.0f);
  bool last_hit = false;

  float2 anchored_initial_mouse = float2(0.0f, 0.0f);

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
  float2 tex_mouse = float2(0.0f, 0.0f);

  /** Position of mouse, used to sample the mask texture. */
  float2 mask_tex_mouse = float2(0.0f, 0.0f);

  /** ColorSpace cache to avoid locking up during sampling. */
  bool do_linear_conversion = false;
  const ocio::ColorSpace *colorspace = nullptr;

  /** WM Paint cursor. */
  void *paint_cursor = nullptr;

  paint::eOverlayControlFlags overlay_flags = paint::eOverlayControlFlags{};

  std::unique_ptr<paint::TexSnapshot> primary_snap;
  std::unique_ptr<paint::TexSnapshot> secondary_snap;
  std::unique_ptr<paint::CursorSnapshot> cursor_snap;

  PaintRuntime();
  ~PaintRuntime();
};
};  // namespace bke

}  // namespace blender
