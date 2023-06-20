/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_heap.h"
#include "BLI_math_matrix.hh"
#include "BLI_memarena.h"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_space_types.h"
#include "DNA_vec_types.h"

#pragma once

/** \file
 * \ingroup geo
 */

struct UnwrapOptions;

enum eUVPackIsland_MarginMethod {
  /** Use scale of existing UVs to multiply margin. */
  ED_UVPACK_MARGIN_SCALED = 0,
  /** Just add the margin, ignoring any UV scale. */
  ED_UVPACK_MARGIN_ADD,
  /** Specify a precise fraction of final UV output. */
  ED_UVPACK_MARGIN_FRACTION,
};

enum eUVPackIsland_RotationMethod {
  /** No rotation. */
  ED_UVPACK_ROTATION_NONE = 0,
  /** Rotated to a minimal rectangle, either vertical or horizontal. */
  ED_UVPACK_ROTATION_AXIS_ALIGNED,
  /** Only 90 degree rotations are allowed. */
  ED_UVPACK_ROTATION_CARDINAL,
  /** Any angle. */
  ED_UVPACK_ROTATION_ANY,
};

enum eUVPackIsland_ShapeMethod {
  /** Use Axis-Aligned Bounding-Boxes. */
  ED_UVPACK_SHAPE_AABB = 0,
  /** Use convex hull. */
  ED_UVPACK_SHAPE_CONVEX,
  /** Use concave hull. */
  ED_UVPACK_SHAPE_CONCAVE,
};

enum eUVPackIsland_PinMethod {
  /** Pin has no impact on packing. */
  ED_UVPACK_PIN_NONE = 0,
  /**
   * Ignore islands containing any pinned UV's.
   * \note Not exposed in the UI, used only for live-unwrap.
   */
  ED_UVPACK_PIN_IGNORE,
  ED_UVPACK_PIN_LOCK_ROTATION,
  ED_UVPACK_PIN_LOCK_ROTATION_SCALE,
  ED_UVPACK_PIN_LOCK_SCALE,
  /** Lock the island in-place (translation, rotation and scale). */
  ED_UVPACK_PIN_LOCK_ALL,
};

namespace blender::geometry {

/** See also #UnwrapOptions. */
class UVPackIsland_Params {
 public:
  /** Reasonable defaults. */
  UVPackIsland_Params();

  void setFromUnwrapOptions(const UnwrapOptions &options);
  void setUDIMOffsetFromSpaceImage(const SpaceImage *sima);
  bool isCancelled() const;

  /** Restrictions around island rotation. */
  eUVPackIsland_RotationMethod rotate_method;
  /** Resize islands to fill the unit square. */
  bool scale_to_fit;
  /** (In UV Editor) only pack islands which have one or more selected UVs. */
  bool only_selected_uvs;
  /** (In 3D Viewport or UV Editor) only pack islands which have selected faces. */
  bool only_selected_faces;
  /** When determining islands, use Seams as boundary edges. */
  bool use_seams;
  /** (In 3D Viewport or UV Editor) use aspect ratio from face. */
  bool correct_aspect;
  /** How will pinned islands be treated. */
  eUVPackIsland_PinMethod pin_method;
  /** Treat unselected UVs as if they were pinned. */
  bool pin_unselected;
  /** Overlapping islands stick together. */
  bool merge_overlap;
  /** Additional space to add around each island. */
  float margin;
  /** Which formula to use when scaling island margin. */
  eUVPackIsland_MarginMethod margin_method;
  /** Additional translation for bottom left corner. */
  float udim_base_offset[2];
  /** Target vertical extent. Should be 1.0f for the unit square. */
  float target_extent;
  /** Target aspect ratio. */
  float target_aspect_y;
  /** Which shape to use when packing. */
  eUVPackIsland_ShapeMethod shape_method;

  /** Abandon packing early when set by the job system. */
  bool *stop;
  bool *do_update;
  /** How much progress we have made. From wmJob. */
  float *progress;
};

class uv_phi;
class PackIsland {
 public:
  PackIsland();

  /** Aspect ratio, required for rotation. */
  float aspect_y;
  /** Are any of the UVs pinned? */
  bool pinned;
  /** Output pre-translation. */
  float2 pre_translate;
  /** Output angle in radians. */
  float angle;
  /** Unchanged by #pack_islands, used by caller. */
  int caller_index;

  void add_triangle(const float2 uv0, const float2 uv1, const float2 uv2);
  void add_polygon(const blender::Span<float2> uvs, MemArena *arena, Heap *heap);

  void build_transformation(const float scale, const double rotation, float r_matrix[2][2]) const;
  void build_inverse_transformation(const float scale,
                                    const double rotation,
                                    float r_matrix[2][2]) const;

  float2 get_diagonal_support(const float scale, const float rotation, const float margin) const;
  float2 get_diagonal_support_d4(const float scale,
                                 const float rotation,
                                 const float margin) const;

  /** Center of AABB and inside-or-touching the convex hull. */
  float2 pivot_;
  /** Half of the diagonal of the AABB. */
  float2 half_diagonal_;
  float pre_rotate_;

  void place_(const float scale, const uv_phi phi);
  void finalize_geometry_(const UVPackIsland_Params &params, MemArena *arena, Heap *heap);

  bool can_rotate_(const UVPackIsland_Params &params) const;
  bool can_scale_(const UVPackIsland_Params &params) const;
  bool can_translate_(const UVPackIsland_Params &params) const;

  blender::Vector<float2> triangle_vertices_;

 private:
  void calculate_pivot_(); /* Calculate `pivot_` and `half_diagonal_` based on added triangles. */
  void calculate_pre_rotation_(const UVPackIsland_Params &params);

  friend class Occupancy;
  friend class OverlapMerger;
};

float pack_islands(const Span<PackIsland *> &islands, const UVPackIsland_Params &params);

/** Compute `r = mat * (a + b)` with high precision. */
void mul_v2_m2_add_v2v2(float r[2], const float mat[2][2], const float a[2], const float b[2]);

}  // namespace blender::geometry
