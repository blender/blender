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

enum eUVPackIsland_ShapeMethod {
  /** Use Axis-Aligned Bounding-Boxes. */
  ED_UVPACK_SHAPE_AABB = 0,
  /** Use convex hull. */
  ED_UVPACK_SHAPE_CONVEX,
  /** Use concave hull. */
  ED_UVPACK_SHAPE_CONCAVE,
};

namespace blender::geometry {

/** See also #UnwrapOptions. */
class UVPackIsland_Params {
 public:
  /** Reasonable defaults. */
  UVPackIsland_Params();

  void setFromUnwrapOptions(const UnwrapOptions &options);
  void setUDIMOffsetFromSpaceImage(const SpaceImage *sima);

  /** Islands can be rotated to improve packing. */
  bool rotate;
  /** (In UV Editor) only pack islands which have one or more selected UVs. */
  bool only_selected_uvs;
  /** (In 3D Viewport or UV Editor) only pack islands which have selected faces. */
  bool only_selected_faces;
  /** When determining islands, use Seams as boundary edges. */
  bool use_seams;
  /** (In 3D Viewport or UV Editor) use aspect ratio from face. */
  bool correct_aspect;
  /** Ignore islands which have any pinned UVs. */
  bool ignore_pinned;
  /** Treat unselected UVs as if they were pinned. */
  bool pin_unselected;
  /** Additional space to add around each island. */
  float margin;
  /** Which formula to use when scaling island margin. */
  eUVPackIsland_MarginMethod margin_method;
  /** Additional translation for bottom left corner. */
  float udim_base_offset[2];
  /** Which shape to use when packing. */
  eUVPackIsland_ShapeMethod shape_method;
};

class PackIsland {
 public:
  rctf bounds_rect;
  /** Output. */
  float2 pre_translate;
  /** Unchanged by #pack_islands, used by caller. */
  int caller_index;

  void add_triangle(const float2 uv0, const float2 uv1, const float2 uv2);
  void add_polygon(const blender::Span<float2> uvs, MemArena *arena, Heap *heap);
  void finalize_geometry(const UVPackIsland_Params &params, MemArena *arena, Heap *heap);

 private:
  blender::Vector<float2> triangle_vertices_;
  friend class Occupancy;
};

void pack_islands(const Span<PackIsland *> &islands,
                  const UVPackIsland_Params &params,
                  float r_scale[2]);

/** Compute `r = mat * (a + b)` with high precision. */
void mul_v2_m2_add_v2v2(float r[2], const float mat[2][2], const float a[2], const float b[2]);

}  // namespace blender::geometry
