/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include "GEO_uv_pack.hh"

#include "BKE_global.hh"

#include "BLI_array.hh"
#include "BLI_bounds.hh"
#include "BLI_boxpack_2d.h"
#include "BLI_convexhull_2d.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_rect.h"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

namespace blender::geometry {

/** Store information about an island's placement such as translation, rotation and reflection. */
class UVPhi {
 public:
  UVPhi() = default;
  bool is_valid() const;

  float2 translation = float2(-1.0f, -1.0f);
  float rotation = 0.0f;
  // bool reflect = false;
};

bool UVPhi::is_valid() const
{
  return translation.x != -1.0f;
}

void mul_v2_m2_add_v2v2(float r[2], const float mat[2][2], const float a[2], const float b[2])
{
  /* Compute `r = mat * (a + b)` with high precision.
   *
   * Often, linear transforms are written as:
   *  `A.x + b`
   *
   * When transforming UVs, the familiar expression can damage UVs due to round-off error,
   * especially when using UDIM and if there are large numbers of islands.
   *
   * Instead, we provide a helper which evaluates:
   *  `A. (x + b)`
   *
   * To further reduce damage, all internal calculations are
   * performed using double precision. */

  const double x = double(a[0]) + double(b[0]);
  const double y = double(a[1]) + double(b[1]);

  r[0] = float(mat[0][0] * x + mat[1][0] * y);
  r[1] = float(mat[0][1] * x + mat[1][1] * y);
}

/**
 * Compute signed distance squared to a line passing through `uva` and `uvb`.
 */
static float dist_signed_squared_to_edge(const float2 probe, const float2 uva, const float2 uvb)
{
  const float2 edge = uvb - uva;
  const float2 side = probe - uva;

  const float edge_length_squared = math::length_squared(edge);
  /* Tolerance here is to avoid division by zero later. */
  if (edge_length_squared < 1e-40f) {
    return math::length_squared(side);
  }

  const float numerator = edge.x * side.y - edge.y * side.x; /* c.f. cross product. */
  const float numerator_ssq = numerator >= 0.0f ? numerator * numerator : -numerator * numerator;

  return numerator_ssq / edge_length_squared;
}

/**
 * \return the larger dimension of `extent`, factoring in the target aspect ratio.
 */
static float get_aspect_scaled_extent(const rctf &extent, const UVPackIsland_Params &params)
{
  const float width = BLI_rctf_size_x(&extent);
  const float height = BLI_rctf_size_y(&extent);
  return std::max(width / params.target_aspect_y, height);
}

/**
 * \return the area of `extent`, factoring in the target aspect ratio.
 */
static float get_aspect_scaled_area(const rctf &extent, const UVPackIsland_Params &params)
{
  const float width = BLI_rctf_size_x(&extent);
  const float height = BLI_rctf_size_y(&extent);
  return (width / params.target_aspect_y) * height;
}

/**
 * \return true if `b` is a preferred layout over `a`, given the packing parameters supplied.
 */
static bool is_larger(const rctf &a, const rctf &b, const UVPackIsland_Params &params)
{
  const float extent_a = get_aspect_scaled_extent(a, params);
  const float extent_b = get_aspect_scaled_extent(b, params);

  /* Equal extent, use smaller area. */
  if (compare_ff_relative(extent_a, extent_b, FLT_EPSILON, 64)) {
    const float area_a = get_aspect_scaled_area(a, params);
    const float area_b = get_aspect_scaled_area(b, params);
    return area_b < area_a;
  }

  return extent_b < extent_a;
}

PackIsland::PackIsland()
{
  /* Initialize to the identity transform. */
  aspect_y = 1.0f;
  pinned = false;
  pre_translate = float2(0.0f);
  angle = 0.0f;
  caller_index = -31415927; /* Accidentally -pi */
  pivot_ = float2(0.0f);
  half_diagonal_ = float2(0.0f);
  pre_rotate_ = 0.0f;
}

void PackIsland::add_triangle(const float2 uv0, const float2 uv1, const float2 uv2)
{
  /* Be careful with winding. */
  if (dist_signed_squared_to_edge(uv0, uv1, uv2) < 0.0f) {
    triangle_vertices_.append(uv0);
    triangle_vertices_.append(uv1);
    triangle_vertices_.append(uv2);
  }
  else {
    triangle_vertices_.append(uv0);
    triangle_vertices_.append(uv2);
    triangle_vertices_.append(uv1);
  }
}

void PackIsland::add_polygon(const Span<float2> uvs, MemArena *arena, Heap *heap)
{
  /* Internally, PackIsland uses triangles as the primitive, so we have to triangulate. */

  int vert_count = int(uvs.size());
  BLI_assert(vert_count >= 3);
  int nfilltri = vert_count - 2;
  if (nfilltri == 1) {
    /* Trivial case, just one triangle. */
    add_triangle(uvs[0], uvs[1], uvs[2]);
    return;
  }

  /* Storage. */
  uint(*tris)[3] = static_cast<uint(*)[3]>(
      BLI_memarena_alloc(arena, sizeof(*tris) * size_t(nfilltri)));
  const float (*source)[2] = reinterpret_cast<const float (*)[2]>(uvs.data());

  /* Triangulate. */
  BLI_polyfill_calc_arena(source, vert_count, 0, tris, arena);

  /* Beautify improves performance of packer. (Optional)
   * Long thin triangles, especially at 45 degree angles,
   * can trigger worst-case performance in #trace_triangle.
   * Using `Beautify` brings more inputs into average-case. */
  BLI_polyfill_beautify(source, vert_count, tris, arena, heap);

  /* Add as triangles. */
  for (int j = 0; j < nfilltri; j++) {
    uint *tri = tris[j];
    add_triangle(source[tri[0]], source[tri[1]], source[tri[2]]);
  }

  BLI_heap_clear(heap, nullptr);
}

static bool can_rotate(const Span<PackIsland *> islands, const UVPackIsland_Params &params)
{
  for (const PackIsland *island : islands) {
    if (!island->can_rotate_(params)) {
      return false;
    }
  }
  return true;
}

/** Angle rounding helper for "D4" transforms. */
static float angle_match(float angle_radians, float target_radians)
{
  if (fabsf(angle_radians - target_radians) < DEG2RADF(0.1f)) {
    return target_radians;
  }
  return angle_radians;
}

static float angle_wrap(float angle_radians)
{
  angle_radians = angle_radians - floorf((angle_radians + M_PI_2) / M_PI) * M_PI;
  BLI_assert(DEG2RADF(-90.0f) <= angle_radians);
  BLI_assert(angle_radians <= DEG2RADF(90.0f));
  return angle_radians;
}

/** Angle rounding helper for "D4" transforms. */
static float plusminus_90_angle(float angle_radians)
{
  angle_radians = angle_wrap(angle_radians);
  angle_radians = angle_match(angle_radians, DEG2RADF(-90.0f));
  angle_radians = angle_match(angle_radians, DEG2RADF(0.0f));
  angle_radians = angle_match(angle_radians, DEG2RADF(90.0f));
  BLI_assert(DEG2RADF(-90.0f) <= angle_radians);
  BLI_assert(angle_radians <= DEG2RADF(90.0f));
  return angle_radians;
}

void PackIsland::calculate_pre_rotation_(const UVPackIsland_Params &params)
{
  pre_rotate_ = 0.0f;
  if (params.rotate_method == ED_UVPACK_ROTATION_CARDINAL) {
    /* Arbitrary rotations are not allowed. */
    return;
  }
  if (!can_rotate_before_pack_(params)) {
    return; /* Nothing to do. */
  }

  BLI_assert(ELEM(params.rotate_method,
                  ED_UVPACK_ROTATION_ANY,
                  ED_UVPACK_ROTATION_AXIS_ALIGNED,
                  ED_UVPACK_ROTATION_AXIS_ALIGNED_X,
                  ED_UVPACK_ROTATION_AXIS_ALIGNED_Y));

  /* As a heuristic to improve layout efficiency, #PackIsland's are first rotated by an
   * angle which minimizes the area of the enclosing AABB. This angle is stored in the
   * `pre_rotate_` member. The different packing strategies will later rotate the island further,
   * stored in the `angle_` member.
   *
   * As AABBs have 180 degree rotational symmetry, we only consider `-90 <= pre_rotate_ <= 90`.
   *
   * As a further heuristic, we "stand up" the AABBs so they are "tall" rather than "wide". */

  /* TODO: Use "Rotating Calipers" directly. */
  {
    Array<float2> coords(triangle_vertices_.size());
    for (const int64_t i : triangle_vertices_.index_range()) {
      coords[i].x = triangle_vertices_[i].x * aspect_y;
      coords[i].y = triangle_vertices_[i].y;
    }

    float angle = -BLI_convexhull_aabb_fit_points_2d(coords);

    if (true) {
      /* "Stand-up" islands. */

      float matrix[2][2];
      angle_to_mat2(matrix, -angle);
      for (const int64_t i : coords.index_range()) {
        mul_m2_v2(matrix, coords[i]);
      }

      Bounds<float2> island_bounds = *bounds::min_max(coords.as_span());
      float2 diagonal = island_bounds.max - island_bounds.min;
      switch (params.rotate_method) {
        case ED_UVPACK_ROTATION_AXIS_ALIGNED_X: {
          if (diagonal.x < diagonal.y) {
            angle += DEG2RADF(90.0f);
          }
          pre_rotate_ = angle_wrap(angle);
          break;
        }
        case ED_UVPACK_ROTATION_AXIS_ALIGNED_Y: {
          if (diagonal.x > diagonal.y) {
            angle += DEG2RADF(90.0f);
          }
          pre_rotate_ = angle_wrap(angle);
          break;
        }
        default: {
          if (diagonal.y < diagonal.x) {
            angle += DEG2RADF(90.0f);
          }
          pre_rotate_ = plusminus_90_angle(angle);
          break;
        }
      }
    }
  }
  if (!pre_rotate_) {
    return;
  }

  /* Pre-Rotate `triangle_vertices_`. */
  float matrix[2][2];
  build_transformation(1.0f, pre_rotate_, matrix);
  for (const int64_t i : triangle_vertices_.index_range()) {
    mul_m2_v2(matrix, triangle_vertices_[i]);
  }
}

void PackIsland::finalize_geometry_(const UVPackIsland_Params &params, MemArena *arena, Heap *heap)
{
  BLI_assert(BLI_heap_len(heap) == 0);

  /* After all the triangles and polygons have been added to a #PackIsland, but before we can start
   * running packing algorithms, there is a one-time finalization process where we can
   * pre-calculate a few quantities about the island, including pre-rotation, bounding box, or
   * computing convex hull.
   * In the future, we might also detect special-cases for speed or efficiency, such as
   * rectangle approximation, circle approximation, detecting if the shape has any holes,
   * analyzing the shape for rotational symmetry or removing overlaps. */
  BLI_assert(triangle_vertices_.size() >= 3);

  calculate_pre_rotation_(params);

  const eUVPackIsland_ShapeMethod shape_method = params.shape_method;
  if (shape_method == ED_UVPACK_SHAPE_CONVEX) {
    /* Compute convex hull of existing triangles. */
    if (triangle_vertices_.size() <= 3) {
      calculate_pivot_();
      return; /* Trivial case, calculate pivot only. */
    }

    int vert_count = int(triangle_vertices_.size());

    /* Allocate storage. */
    int *index_map = static_cast<int *>(
        BLI_memarena_alloc(arena, sizeof(*index_map) * vert_count));

    /* Compute convex hull. */
    int convex_len = BLI_convexhull_2d(triangle_vertices_, index_map);
    if (convex_len >= 3) {
      /* Write back. */
      triangle_vertices_.clear();
      float2 *convex_verts = static_cast<float2 *>(
          BLI_memarena_alloc(arena, sizeof(*convex_verts) * convex_len));
      for (int i = 0; i < convex_len; i++) {
        convex_verts[i] = triangle_vertices_[index_map[i]];
      }
      add_polygon(Span(convex_verts, convex_len), arena, heap);
    }
  }

  /* Pivot calculation might be performed multiple times during pre-processing.
   * To ensure the `pivot_` used during packing includes any changes, we also calculate
   * the pivot *last* to ensure it is correct.
   */
  calculate_pivot_();
}

void PackIsland::calculate_pivot_()
{
  /* The meaning of `pivot_` is somewhat ambiguous, as technically, the only restriction is that it
   * can't be *outside* the convex hull of the shape. Anywhere in the interior, or even on the
   * boundary of the convex hull is fine.
   * (The GJK support function for every direction away from `pivot_` is numerically >= 0.0f)
   *
   * Ideally, `pivot_` would be the center of the shape's minimum covering circle (MCC). That would
   * improve packing performance, and potentially even improve packing efficiency.
   *
   * However, computing the MCC *efficiently* is somewhat complicated.
   *
   * Instead, we compromise, and `pivot_` is currently calculated as the center of the AABB.
   *
   * If we later special-case circle packing, *AND* we can preserve the
   * numerically-not-outside-the-convex-hull property, we may want to revisit this choice.
   */
  Bounds<float2> triangle_bounds = *bounds::min_max(triangle_vertices_.as_span());
  pivot_ = (triangle_bounds.min + triangle_bounds.max) * 0.5f;
  half_diagonal_ = (triangle_bounds.max - triangle_bounds.min) * 0.5f;
  BLI_assert(half_diagonal_.x >= 0.0f);
  BLI_assert(half_diagonal_.y >= 0.0f);
}

void PackIsland::place_(const float scale, const UVPhi phi)
{
  angle = phi.rotation + pre_rotate_;

  float matrix_inverse[2][2];
  build_inverse_transformation(scale, phi.rotation, matrix_inverse);
  mul_v2_m2v2(pre_translate, matrix_inverse, phi.translation);
  pre_translate -= pivot_;

  if (pre_rotate_) {
    build_inverse_transformation(1.0f, pre_rotate_, matrix_inverse);
    mul_m2_v2(matrix_inverse, pre_translate);
  }
}

UVPackIsland_Params::UVPackIsland_Params()
{
  rotate_method = ED_UVPACK_ROTATION_NONE;
  scale_to_fit = true;
  only_selected_uvs = false;
  only_selected_faces = false;
  use_seams = false;
  correct_aspect = false;
  pin_method = ED_UVPACK_PIN_NONE;
  pin_unselected = false;
  merge_overlap = false;
  margin = 0.001f;
  margin_method = ED_UVPACK_MARGIN_SCALED;
  udim_base_offset[0] = 0.0f;
  udim_base_offset[1] = 0.0f;
  target_extent = 1.0f;   /* Assume unit square. */
  target_aspect_y = 1.0f; /* Assume unit square. */
  shape_method = ED_UVPACK_SHAPE_AABB;
  stop = nullptr;
  do_update = nullptr;
  progress = nullptr;
}

/* Compact representation for AABB packers. */
class UVAABBIsland {
 public:
  float2 uv_diagonal;
  int64_t index;
  float aspect_y;
};

/**
 * Pack AABB islands using the "Alpaca" strategy, with no rotation.
 *
 * Each box is packed into an "L" shaped region, gradually filling up space.
 * "Alpaca" is a pun, as it's pronounced the same as "L-Packer" in English.
 *
 * In theory, alpaca_turbo should be the fastest non-trivial packer, hence the "turbo" suffix.
 *
 * Technically, the algorithm here is only `O(n)`, In practice, to get reasonable results,
 * the input must be pre-sorted, which costs an additional `O(nlogn)` time complexity.
 */
static void pack_islands_alpaca_turbo(const int64_t exclude_index,
                                      const rctf &exclude,
                                      const Span<std::unique_ptr<UVAABBIsland>> islands,
                                      const float target_aspect_y,
                                      MutableSpan<UVPhi> r_phis,
                                      rctf *r_extent)
{
  /* Exclude an initial AABB near the origin. */
  float next_u1 = exclude.xmax;
  float next_v1 = exclude.ymax;
  bool zigzag = next_u1 < next_v1 * target_aspect_y; /* Horizontal or Vertical strip? */

  float u0 = zigzag ? next_u1 : 0.0f;
  float v0 = zigzag ? 0.0f : next_v1;

  /* Visit every island in order, except the excluded islands at the start. */
  for (int64_t index = exclude_index; index < islands.size(); index++) {
    UVAABBIsland &island = *islands[index];
    const float dsm_u = island.uv_diagonal.x;
    const float dsm_v = island.uv_diagonal.y;

    bool restart = false;
    if (zigzag) {
      restart = (next_v1 < v0 + dsm_v);
    }
    else {
      restart = (next_u1 < u0 + dsm_u);
    }
    if (restart) {
      /* We're at the end of a strip. Restart from U axis or V axis. */
      zigzag = next_u1 < next_v1 * target_aspect_y;
      u0 = zigzag ? next_u1 : 0.0f;
      v0 = zigzag ? 0.0f : next_v1;
    }

    /* Place the island. */
    UVPhi &phi = r_phis[island.index];
    phi.rotation = 0.0f;
    phi.translation.x = u0 + dsm_u * 0.5f;
    phi.translation.y = v0 + dsm_v * 0.5f;
    if (zigzag) {
      /* Move upwards. */
      v0 += dsm_v;
      next_u1 = max_ff(next_u1, u0 + dsm_u);
      next_v1 = max_ff(next_v1, v0);
    }
    else {
      /* Move sideways. */
      u0 += dsm_u;
      next_v1 = max_ff(next_v1, v0 + dsm_v);
      next_u1 = max_ff(next_u1, u0);
    }
  }

  /* Write back extent. */
  *r_extent = {0.0f, next_u1, 0.0f, next_v1};
}

/**
 * Helper function for #pack_islands_alpaca_rotate
 *
 * The "Hole" is an AABB region of the UV plane that is stored in an unusual way.
 * \param hole: is the XY position of lower left corner of the AABB.
 * \param hole_diagonal: is the extent of the AABB, possibly flipped.
 * \param hole_rotate: is a boolean value, tracking if `hole_diagonal` is flipped.
 *
 * Given an alternate AABB specified by `(u0, v0, u1, v1)`, the helper will
 * update the Hole to the candidate location if it is larger.
 */
static void update_hole_rotate(float2 &hole,
                               float2 &hole_diagonal,
                               bool &hole_rotate,
                               const float u0,
                               const float v0,
                               const float u1,
                               const float v1)
{
  BLI_assert(hole_diagonal.x <= hole_diagonal.y); /* Confirm invariants. */

  const float hole_area = hole_diagonal.x * hole_diagonal.y;
  const float quad_area = (u1 - u0) * (v1 - v0);
  if (quad_area <= hole_area) {
    return; /* No update, existing hole is larger than candidate. */
  }
  hole.x = u0;
  hole.y = v0;
  hole_diagonal.x = u1 - u0;
  hole_diagonal.y = v1 - v0;
  if (hole_diagonal.y < hole_diagonal.x) {
    std::swap(hole_diagonal.x, hole_diagonal.y);
    hole_rotate = true;
  }
  else {
    hole_rotate = false;
  }

  const float updated_area = hole_diagonal.x * hole_diagonal.y;
  BLI_assert(hole_area < updated_area); /* Confirm hole grew in size. */
  UNUSED_VARS(updated_area);

  BLI_assert(hole_diagonal.x <= hole_diagonal.y); /* Confirm invariants. */
}

/**
 * Pack AABB islands using the "Alpaca" strategy, with rotation.
 *
 * Same as #pack_islands_alpaca_turbo, with support for rotation in 90 degree increments.
 *
 * Also adds the concept of a "Hole", which is unused space that can be filled.
 * Tracking the "Hole" has a slight performance cost, while improving packing efficiency.
 */
static void pack_islands_alpaca_rotate(const int64_t exclude_index,
                                       const rctf &exclude,
                                       const Span<std::unique_ptr<UVAABBIsland>> islands,
                                       const float target_aspect_y,
                                       MutableSpan<UVPhi> r_phis,
                                       rctf *r_extent)
{
  /* Exclude an initial AABB near the origin. */
  float next_u1 = exclude.xmax;
  float next_v1 = exclude.ymax;
  bool zigzag = next_u1 / target_aspect_y < next_v1; /* Horizontal or Vertical strip? */

  /* Track an AABB "hole" which may be filled at any time. */
  float2 hole(0.0f);
  float2 hole_diagonal(0.0f);
  bool hole_rotate = false;

  float u0 = zigzag ? next_u1 : 0.0f;
  float v0 = zigzag ? 0.0f : next_v1;

  /* Visit every island in order, except the excluded islands at the start. */
  for (int64_t index = exclude_index; index < islands.size(); index++) {
    UVAABBIsland &island = *islands[index];
    UVPhi &phi = r_phis[island.index];
    const float uvdiag_x = island.uv_diagonal.x * island.aspect_y;
    float min_dsm = std::min(uvdiag_x, island.uv_diagonal.y);
    float max_dsm = std::max(uvdiag_x, island.uv_diagonal.y);

    if (min_dsm < hole_diagonal.x && max_dsm < hole_diagonal.y) {
      /* Place island in the hole. */
      if (hole_rotate == (min_dsm == island.uv_diagonal.x)) {
        phi.rotation = DEG2RADF(90.0f);
        phi.translation.x = hole[0] + island.uv_diagonal.y * 0.5f / island.aspect_y;
        phi.translation.y = hole[1] + island.uv_diagonal.x * 0.5f * island.aspect_y;
      }
      else {
        phi.rotation = 0.0f;
        phi.translation.x = hole[0] + island.uv_diagonal.x * 0.5f;
        phi.translation.y = hole[1] + island.uv_diagonal.y * 0.5f;
      }

      /* Update space left in the hole. */
      float p[6];
      p[0] = hole[0];
      p[1] = hole[1];
      p[2] = hole[0] + (hole_rotate ? max_dsm : min_dsm) / island.aspect_y;
      p[3] = hole[1] + (hole_rotate ? min_dsm : max_dsm);
      p[4] = hole[0] + (hole_rotate ? hole_diagonal.y : hole_diagonal.x);
      p[5] = hole[1] + (hole_rotate ? hole_diagonal.x : hole_diagonal.y);
      hole_diagonal.x = 0; /* Invalidate old hole. */
      update_hole_rotate(hole, hole_diagonal, hole_rotate, p[0], p[3], p[4], p[5]);
      update_hole_rotate(hole, hole_diagonal, hole_rotate, p[2], p[1], p[4], p[5]);

      /* Island is placed in the hole, no need to check for restart, or process movement. */
      continue;
    }

    bool restart = false;
    if (zigzag) {
      restart = (next_v1 < v0 + min_dsm);
    }
    else {
      restart = (next_u1 < u0 + min_dsm / island.aspect_y);
    }
    if (restart) {
      update_hole_rotate(hole, hole_diagonal, hole_rotate, u0, v0, next_u1, next_v1);
      /* We're at the end of a strip. Restart from U axis or V axis. */
      zigzag = next_u1 / target_aspect_y < next_v1;
      u0 = zigzag ? next_u1 : 0.0f;
      v0 = zigzag ? 0.0f : next_v1;
    }

    /* Place the island. */
    if (zigzag == (min_dsm == uvdiag_x)) {
      phi.rotation = DEG2RADF(90.0f);
      phi.translation.x = u0 + island.uv_diagonal.y * 0.5f / island.aspect_y;
      phi.translation.y = v0 + island.uv_diagonal.x * 0.5f * island.aspect_y;
    }
    else {
      phi.rotation = 0.0f;
      phi.translation.x = u0 + island.uv_diagonal.x * 0.5f;
      phi.translation.y = v0 + island.uv_diagonal.y * 0.5f;
    }

    /* Move according to the "Alpaca rules", with rotation. */
    if (zigzag) {
      /* Move upwards. */
      v0 += min_dsm;
      next_u1 = max_ff(next_u1, u0 + max_dsm / island.aspect_y);
      next_v1 = max_ff(next_v1, v0);
    }
    else {
      /* Move sideways. */
      u0 += min_dsm / island.aspect_y;
      next_v1 = max_ff(next_v1, v0 + max_dsm);
      next_u1 = max_ff(next_u1, u0);
    }
  }

  /* Write back total pack AABB. */
  *r_extent = {0.0f, next_u1, 0.0f, next_v1};
}

/**
 * Use a fast algorithm to pack the supplied `aabbs`.
 */
static void pack_islands_fast(const int64_t exclude_index,
                              const rctf &exclude,
                              const Span<std::unique_ptr<UVAABBIsland>> aabbs,
                              const bool rotate,
                              const float target_aspect_y,
                              MutableSpan<UVPhi> r_phis,
                              rctf *r_extent)
{
  if (rotate) {
    pack_islands_alpaca_rotate(exclude_index, exclude, aabbs, target_aspect_y, r_phis, r_extent);
  }
  else {
    pack_islands_alpaca_turbo(exclude_index, exclude, aabbs, target_aspect_y, r_phis, r_extent);
  }
}

/** Frits Göbel, 1979. */
static void pack_gobel(const Span<std::unique_ptr<UVAABBIsland>> aabbs,
                       const float scale,
                       const int m,
                       MutableSpan<UVPhi> r_phis)
{
  for (const int64_t i : aabbs.index_range()) {
    UVPhi &phi = *(UVPhi *)&r_phis[aabbs[i]->index];
    phi.rotation = 0.0f;
    if (i == 0) {
      phi.translation.x = 0.5f * scale;
      phi.translation.y = 0.5f * scale;
      continue;
    }
    int xx = (i - 1) % m;
    int yy = int(i - 1) / m;
    phi.translation.x = (xx + 0.5f) * scale;
    phi.translation.y = (yy + 0.5f) * scale;
    if (xx >= yy) {
      phi.translation.x += (1 + sqrtf(0.5f)) * scale;
    }
    else {
      phi.translation.y += sqrtf(0.5f) * scale;
    }

    if (i == m * (m + 1) + 1) {
      phi.translation.x += (m + sqrtf(0.5f)) * scale;
      phi.translation.y -= scale;
    }
    else if (i > m * (m + 1) + 1) {
      phi.rotation = DEG2RADF(45.0f);
      phi.translation.x = ((i - m * (m + 1) - 1.5f) * cosf(phi.rotation) + 1.0f) * scale;
      phi.translation.y = phi.translation.x;
    }
  }
}

static bool pack_islands_optimal_pack_table(const int table_count,
                                            const float max_extent,
                                            const float *optimal,
                                            const char * /*unused_comment*/,
                                            int64_t island_count,
                                            const float large_uv,
                                            const Span<std::unique_ptr<UVAABBIsland>> aabbs,
                                            const UVPackIsland_Params &params,
                                            MutableSpan<UVPhi> r_phis,
                                            rctf *r_extent)
{
  if (table_count < island_count) {
    return false;
  }
  rctf extent = {0.0f, large_uv * max_extent, 0.0f, large_uv * max_extent};
  if (is_larger(extent, *r_extent, params)) {
    return false;
  }
  *r_extent = extent;

  for (int i = 0; i < island_count; i++) {
    UVPhi &phi = r_phis[aabbs[i]->index];
    phi.translation.x = optimal[i * 3 + 0] * large_uv;
    phi.translation.y = optimal[i * 3 + 1] * large_uv;
    phi.rotation = optimal[i * 3 + 2];
  }
  return true;
}

/* Attempt to find an "Optimal" packing of the islands, e.g. assuming squares or circles. */
static void pack_islands_optimal_pack(const Span<std::unique_ptr<UVAABBIsland>> aabbs,
                                      const UVPackIsland_Params &params,
                                      MutableSpan<UVPhi> r_phis,
                                      rctf *r_extent)
{
  if (params.shape_method == ED_UVPACK_SHAPE_AABB) {
    return;
  }
  if (params.target_aspect_y != 1.0f) {
    return;
  }
  if (params.rotate_method != ED_UVPACK_ROTATION_ANY) {
    return;
  }

  float large_uv = 0.0f;
  for (const int64_t i : aabbs.index_range()) {
    large_uv = max_ff(large_uv, aabbs[i]->uv_diagonal.x);
    large_uv = max_ff(large_uv, aabbs[i]->uv_diagonal.y);
  }

  int64_t island_count_patch = aabbs.size();

  const float opt_11[] = {
      /* Walter Trump, 1979. */
      2.6238700165660708840676f,
      2.4365065643739085565755f,
      0.70130710554829878145f,
      1.9596047386700836678841f,
      1.6885655318806973568257f,
      0.70130710554829878145f,
      1.9364970731945949644626f,
      3.1724566890997589752033f,
      0.70130710554829878145f,
      1.2722458068219282267819f,
      2.4245322476118422727609f,
      0.70130710554829878145f,
      3.1724918301381124230431f,
      1.536261617698265524723f,
      0.70130710554829878145f,
      3.3770999999999999907629f,
      3.3770999999999999907629f,
      0.0f,
      0.5f,
      1.5f,
      0.0f,
      2.5325444557069398676674f,
      0.5f,
      0.0f,
      0.5f,
      3.3770999999999999907629f,
      0.0f,
      1.5f,
      0.5f,
      0.0f,
      0.5f,
      0.5f,
      0.0f,
  };
  pack_islands_optimal_pack_table(11,
                                  3.8770999999999999907629f,
                                  opt_11,
                                  "Walter Trump, 1979",
                                  island_count_patch,
                                  large_uv,
                                  aabbs,
                                  params,
                                  r_phis,
                                  r_extent);

  const float opt_18[] = {
      /* Pertti Hamalainen, 1979. */
      2.4700161985907582717914f,
      2.4335783708246112588824f,
      0.42403103949074028022892f,
      1.3528594569415370862941f,
      2.3892972847076845432923f,
      0.42403103949074028022892f,
      2.0585783708246108147932f,
      1.5221405430584633577951f,
      0.42403103949074028022892f,
      1.7642972847076845432923f,
      3.3007351124738324443797f,
      0.42403103949074028022892f,
      3.3228756555322949139963f,
      1.5f,
      0.0f,
      3.3228756555322949139963f,
      3.3228756555322949139963f,
      0.0f,
      0.5f,
      1.5f,
      0.0f,
      2.3228756555322949139963f,
      4.3228756555322949139963f,
      0.0f,
      0.5f,
      3.3228756555322949139963f,
      0.0f,
      1.5f,
      0.5f,
      0.0f,
      3.3228756555322949139963f,
      0.5f,
      0.0f,
      3.3228756555322949139963f,
      4.3228756555322949139963f,
      0.0f,
      4.3228756555322949139963f,
      1.5f,
      0.0f,
      4.3228756555322949139963f,
      3.3228756555322949139963f,
      0.0f,
      0.5f,
      0.5f,
      0.0f,
      0.5f,
      4.3228756555322949139963f,
      0.0f,
      4.3228756555322949139963f,
      0.5f,
      0.0f,
      4.3228756555322949139963f,
      4.3228756555322949139963f,
      0.0f,
  };
  pack_islands_optimal_pack_table(18,
                                  4.8228756555322949139963f,
                                  opt_18,
                                  "Pertti Hamalainen, 1979",
                                  island_count_patch,
                                  large_uv,
                                  aabbs,
                                  params,
                                  r_phis,
                                  r_extent);

  const float opt_19[] = {
      /* Robert Wainwright, 1979. */
      2.1785113019775792508881f,
      1.9428090415820631342569f,
      0.78539816339744827899949f,
      1.4714045207910317891731f,
      2.6499158227686105959719f,
      0.78539816339744827899949f,
      2.9428090415820640224354f,
      2.7071067811865479058042f,
      0.78539816339744827899949f,
      2.2357022603955165607204f,
      3.4142135623730953675192f,
      0.78539816339744827899949f,
      1.4428090415820635783462f,
      1.2642977396044836613243f,
      0.78539816339744827899949f,
      3.3856180831641271566923f,
      1.5f,
      0.0f,
      0.73570226039551600560884f,
      1.9714045207910311230393f,
      0.78539816339744827899949f,
      3.6213203435596432733234f,
      3.4428090415820635783462f,
      0.78539816339744827899949f,
      2.9142135623730958116084f,
      4.1499158227686105959719f,
      0.78539816339744827899949f,
      2.3856180831641271566923f,
      0.5f,
      0.0f,
      0.5f,
      3.3856180831641271566923f,
      0.0f,
      1.5f,
      4.3856180831641271566923f,
      0.0f,
      4.3856180831641271566923f,
      2.5f,
      0.0f,
      3.3856180831641271566923f,
      0.5f,
      0.0f,
      4.3856180831641271566923f,
      1.5f,
      0.0f,
      0.5f,
      0.5f,
      0.0f,
      0.5f,
      4.3856180831641271566923f,
      0.0f,
      4.3856180831641271566923f,
      0.5f,
      0.0f,
      4.3856180831641271566923f,
      4.3856180831641271566923f,
      0.0f,
  };
  pack_islands_optimal_pack_table(19,
                                  4.8856180831641271566923f,
                                  opt_19,
                                  "Robert Wainwright, 1979",
                                  island_count_patch,
                                  large_uv,
                                  aabbs,
                                  params,
                                  r_phis,
                                  r_extent);

  const float opt_26[] = {
      /* Erich Friedman, 1997. */
      2.3106601717798209705279f,
      2.8106601717798214146171f,
      0.78539816339744827899949f,
      1.6035533905932735088129f,
      2.1035533905932739529021f,
      0.78539816339744827899949f,
      3.0177669529663684322429f,
      2.1035533905932739529021f,
      0.78539816339744827899949f,
      2.3106601717798209705279f,
      1.3964466094067264911871f,
      0.78539816339744827899949f,
      1.6035533905932735088129f,
      3.5177669529663688763321f,
      0.78539816339744827899949f,
      0.89644660940672593607559f,
      2.8106601717798214146171f,
      0.78539816339744827899949f,
      3.0177669529663684322429f,
      3.5177669529663688763321f,
      0.78539816339744827899949f,
      3.7248737341529158939579f,
      2.8106601717798214146171f,
      0.78539816339744827899949f,
      2.3106601717798209705279f,
      4.2248737341529167821363f,
      0.78539816339744827899949f,
      0.5f,
      1.5f,
      0.0f,
      1.5f,
      0.5f,
      0.0f,
      3.1213203435596419410558f,
      0.5f,
      0.0f,
      4.1213203435596419410558f,
      1.5f,
      0.0f,
      0.5f,
      4.1213203435596419410558f,
      0.0f,
      0.5f,
      0.5f,
      0.0f,
      4.1213203435596419410558f,
      4.1213203435596419410558f,
      0.0f,
      4.1213203435596419410558f,
      0.5f,
      0.0f,
      1.5f,
      5.1213203435596419410558f,
      0.0f,
      3.1213203435596419410558f,
      5.1213203435596419410558f,
      0.0f,
      5.1213203435596419410558f,
      2.5f,
      0.0f,
      5.1213203435596419410558f,
      1.5f,
      0.0f,
      0.5f,
      5.1213203435596419410558f,
      0.0f,
      4.1213203435596419410558f,
      5.1213203435596419410558f,
      0.0f,
      5.1213203435596419410558f,
      4.1213203435596419410558f,
      0.0f,
      5.1213203435596419410558f,
      0.5f,
      0.0f,
      5.1213203435596419410558f,
      5.1213203435596419410558f,
      0.0f,
  };
  pack_islands_optimal_pack_table(26,
                                  5.6213203435596419410558f,
                                  opt_26,
                                  "Erich Friedman, 1997",
                                  island_count_patch,
                                  large_uv,
                                  aabbs,
                                  params,
                                  r_phis,
                                  r_extent);

  if (island_count_patch == 37) {
    island_count_patch = 38; /* TODO, Cantrell 2002. */
  }
  if (island_count_patch == 50) {
    island_count_patch = 52; /* TODO, Cantrell 2002. */
  }
  if (island_count_patch == 51) {
    island_count_patch = 52; /* TODO, Hajba 2009. */
  }
  if (island_count_patch == 65) {
    island_count_patch = 67; /* TODO, Gobel 1979. */
  }
  if (island_count_patch == 66) {
    island_count_patch = 67; /* TODO, Stenlund 1980. */
  }
  /* See https://www.combinatorics.org/files/Surveys/ds7/ds7v5-2009/ds7-2009.html
   * https://erich-friedman.github.io/packing/squinsqu */
  for (int a = 1; a < 20; a++) {
    int n = a * a + a + 3 + floorf((a - 1) * sqrtf(2.0f));
    if (island_count_patch == n) {
      float max_uv_gobel = large_uv * (a + 1 + sqrtf(0.5f));
      rctf extent = {0.0f, max_uv_gobel, 0.0f, max_uv_gobel};
      if (is_larger(*r_extent, extent, params)) {
        *r_extent = extent;
        pack_gobel(aabbs, large_uv, a, r_phis);
      }
      return;
    }
  }
}

/* Wrapper around #BLI_box_pack_2d. */
static void pack_island_box_pack_2d(const Span<std::unique_ptr<UVAABBIsland>> aabbs,
                                    const UVPackIsland_Params &params,
                                    MutableSpan<UVPhi> r_phis,
                                    rctf *r_extent)
{
  /* Allocate storage. */
  BoxPack *box_array = MEM_malloc_arrayN<BoxPack>(size_t(aabbs.size()), __func__);

  /* Prepare for box_pack_2d. */
  for (const int64_t i : aabbs.index_range()) {
    BoxPack *box = box_array + i;
    box->w = aabbs[i]->uv_diagonal.x / params.target_aspect_y;
    box->h = aabbs[i]->uv_diagonal.y;
  }

  const bool sort_boxes = false; /* Use existing ordering from `aabbs`. */

  float box_max_u = 0.0f;
  float box_max_v = 0.0f;
  BLI_box_pack_2d(box_array, int(aabbs.size()), sort_boxes, &box_max_u, &box_max_v);
  box_max_u *= params.target_aspect_y;
  rctf extent = {0.0f, box_max_u, 0.0f, box_max_v};

  if (is_larger(*r_extent, extent, params)) {
    *r_extent = extent;
    /* Write back box_pack UVs. */
    for (const int64_t i : aabbs.index_range()) {
      BoxPack *box = box_array + i;
      UVPhi &phi = *(UVPhi *)&r_phis[aabbs[i]->index];
      phi.rotation = 0.0f; /* #BLI_box_pack_2d never rotates. */
      phi.translation.x = (box->x + box->w * 0.5f) * params.target_aspect_y;
      phi.translation.y = (box->y + box->h * 0.5f);
    }
  }

  /* Housekeeping. */
  MEM_freeN(box_array);
}

/**
 * Helper class for the `xatlas` strategy.
 * Accelerates geometry queries by approximating exact queries with a bitmap.
 * Includes some book keeping variables to simplify the algorithm.
 *
 * \note The last entry, `(width-1, height-1)` is named the "top-right".
 */
class Occupancy {
 public:
  Occupancy(const float initial_scale);

  void increase_scale(); /* Resize the scale of the bitmap and clear it. */
  void clear();          /* Clear occupancy information. */

  /* Write or Query a triangle on the bitmap. */
  float trace_triangle(const float2 &uv0,
                       const float2 &uv1,
                       const float2 &uv2,
                       const float margin,
                       const bool write) const;

  /* Write or Query an island on the bitmap. */
  float trace_island(const PackIsland *island,
                     const UVPhi phi,
                     const float scale,
                     const float margin,
                     const bool write) const;

  int bitmap_radix = 800;               /* Width and Height of `bitmap`. */
  float bitmap_scale_reciprocal = 1.0f; /* == 1.0f / `bitmap_scale`. */
 private:
  mutable Array<float> bitmap_;

  mutable float2 witness_;         /* Witness to a previously known occupied pixel. */
  mutable float witness_distance_; /* Signed distance to nearest placed island. */
  mutable uint triangle_hint_;     /* Hint to a previously suspected overlapping triangle. */

  const float terminal = 1048576.0f; /* 4 * bitmap_radix < terminal < INT_MAX / 4. */
};

Occupancy::Occupancy(const float initial_scale) : bitmap_(bitmap_radix * bitmap_radix, false)
{
  increase_scale();
  bitmap_scale_reciprocal = bitmap_radix / initial_scale; /* Actually set the value. */
}

void Occupancy::increase_scale()
{
  BLI_assert(bitmap_scale_reciprocal > 0.0f); /* TODO: Packing has failed, report error. */

  bitmap_scale_reciprocal *= 0.5f;
  clear();
}

void Occupancy::clear()
{
  for (int i = 0; i < bitmap_radix * bitmap_radix; i++) {
    bitmap_[i] = terminal;
  }
  witness_.x = -1;
  witness_.y = -1;
  witness_distance_ = 0.0f;
  triangle_hint_ = 0;
}

static float signed_distance_fat_triangle(const float2 probe,
                                          const float2 uv0,
                                          const float2 uv1,
                                          const float2 uv2)
{
  /* Be careful with ordering, uv0 <- uv1 <- uv2 <- uv0 <- uv1 etc. */
  const float dist01_ssq = dist_signed_squared_to_edge(probe, uv0, uv1);
  const float dist12_ssq = dist_signed_squared_to_edge(probe, uv1, uv2);
  const float dist20_ssq = dist_signed_squared_to_edge(probe, uv2, uv0);
  float result_ssq = max_fff(dist01_ssq, dist12_ssq, dist20_ssq);
  if (result_ssq < 0.0f) {
    return -sqrtf(-result_ssq);
  }
  BLI_assert(result_ssq >= 0.0f);
  result_ssq = std::min(result_ssq, math::length_squared(probe - uv0));
  result_ssq = std::min(result_ssq, math::length_squared(probe - uv1));
  result_ssq = std::min(result_ssq, math::length_squared(probe - uv2));
  BLI_assert(result_ssq >= 0.0f);
  return sqrtf(result_ssq);
}

float Occupancy::trace_triangle(const float2 &uv0,
                                const float2 &uv1,
                                const float2 &uv2,
                                const float margin,
                                const bool write) const
{
  const float x0 = min_fff(uv0.x, uv1.x, uv2.x);
  const float y0 = min_fff(uv0.y, uv1.y, uv2.y);
  const float x1 = max_fff(uv0.x, uv1.x, uv2.x);
  const float y1 = max_fff(uv0.y, uv1.y, uv2.y);
  float spread = write ? margin * 2 : 0.0f;
  int ix0 = std::max(int(floorf((x0 - spread) * bitmap_scale_reciprocal)), 0);
  int iy0 = std::max(int(floorf((y0 - spread) * bitmap_scale_reciprocal)), 0);
  int ix1 = std::min(int(floorf((x1 + spread) * bitmap_scale_reciprocal + 2)), bitmap_radix);
  int iy1 = std::min(int(floorf((y1 + spread) * bitmap_scale_reciprocal + 2)), bitmap_radix);

  const float2 uv0s = uv0 * bitmap_scale_reciprocal;
  const float2 uv1s = uv1 * bitmap_scale_reciprocal;
  const float2 uv2s = uv2 * bitmap_scale_reciprocal;

  /* TODO: Better epsilon handling here could reduce search size. */
  float epsilon = 0.7071f; /* == sqrt(0.5f), rounded up by 0.00002f. */
  epsilon = std::max(epsilon, 2 * margin * bitmap_scale_reciprocal);

  if (!write) {
    if (ix0 <= witness_.x && witness_.x < ix1) {
      if (iy0 <= witness_.y && witness_.y < iy1) {
        const float distance = signed_distance_fat_triangle(witness_, uv0s, uv1s, uv2s);
        const float extent = epsilon - distance - witness_distance_;
        const float pixel_round_off = -0.1f; /* Go faster on nearly-axis aligned edges. */
        if (extent > pixel_round_off) {
          return std::max(0.0f, extent); /* Witness observes occupied. */
        }
      }
    }
  }

  /* Iterate in opposite direction to outer search to improve witness effectiveness. */
  for (int y = iy1 - 1; y >= iy0; y--) {
    for (int x = ix1 - 1; x >= ix0; x--) {
      float *hotspot = &bitmap_[y * bitmap_radix + x];
      if (!write && *hotspot > epsilon) {
        continue;
      }
      const float2 probe(x, y);
      const float distance = signed_distance_fat_triangle(probe, uv0s, uv1s, uv2s);
      if (write) {
        *hotspot = min_ff(distance, *hotspot);
        continue;
      }
      const float extent = epsilon - distance - *hotspot;
      if (extent > 0.0f) {
        witness_ = probe;
        witness_distance_ = *hotspot;
        return extent; /* Occupied. */
      }
    }
  }
  return -1.0f; /* Available. */
}

float2 PackIsland::get_diagonal_support(const float scale,
                                        const float rotation,
                                        /* const bool reflection, */
                                        const float margin) const
{
  /* Caution: Only "Dihedral Group D4" transforms are calculated exactly.
   * if the transform is Non-D4, an upper bound will be returned instead. */

  if (rotation == DEG2RADF(-180.0f) || rotation == 0.0f || rotation == DEG2RADF(180.0f)) {
    return half_diagonal_ * scale + margin;
  }

  if (rotation == DEG2RADF(-90.0f) || rotation == DEG2RADF(90.0f) || rotation == DEG2RADF(270.0f))
  {
    return float2(half_diagonal_.y / aspect_y, half_diagonal_.x * aspect_y) * scale + margin;
  }

  float matrix[2][2];
  build_transformation(scale, rotation, matrix);

  /* TODO: Use convex hull to calculate support. */
  float diagonal_rotated[2];
  mul_v2_m2v2(diagonal_rotated, matrix, half_diagonal_);
  float sx = fabsf(diagonal_rotated[0]);
  float sy = fabsf(diagonal_rotated[1]);

  return float2(sx + sy * 0.7071f + margin, sx * 0.7071f + sy + margin); /* Upper bound. */
}

float Occupancy::trace_island(const PackIsland *island,
                              const UVPhi phi,
                              const float scale,
                              const float margin,
                              const bool write) const
{
  const float2 diagonal_support = island->get_diagonal_support(scale, phi.rotation, margin);

  if (!write) {
    if (phi.translation.x < diagonal_support.x || phi.translation.y < diagonal_support.y) {
      return terminal; /* Occupied. */
    }
  }
  float matrix[2][2];
  island->build_transformation(scale, phi.rotation, matrix);
  float2 pivot_transformed;
  mul_v2_m2v2(pivot_transformed, matrix, island->pivot_);

  /* TODO: Support #ED_UVPACK_SHAPE_AABB. */

  /* TODO: If the PackIsland has the same shape as it's convex hull, we can trace the hull instead
   * of the individual triangles, which is faster and provides a better value of `extent`.
   */

  const float2 delta = phi.translation - pivot_transformed;
  const uint vert_count = uint(
      island->triangle_vertices_.size()); /* `uint` is faster than `int`. */
  for (uint i = 0; i < vert_count; i += 3) {
    const uint j = (i + triangle_hint_) % vert_count;
    float2 uv0;
    float2 uv1;
    float2 uv2;
    mul_v2_m2v2(uv0, matrix, island->triangle_vertices_[j]);
    mul_v2_m2v2(uv1, matrix, island->triangle_vertices_[j + 1]);
    mul_v2_m2v2(uv2, matrix, island->triangle_vertices_[j + 2]);
    const float extent = trace_triangle(uv0 + delta, uv1 + delta, uv2 + delta, margin, write);

    if (!write && extent >= 0.0f) {
      triangle_hint_ = j;
      return extent; /* Occupied. */
    }
  }
  return -1.0f; /* Available. */
}

static UVPhi find_best_fit_for_island(const PackIsland *island,
                                      const int scan_line,
                                      const Occupancy &occupancy,
                                      const float scale,
                                      const int angle_90_multiple,
                                      /* TODO: const bool reflect, */
                                      const float margin,
                                      const float target_aspect_y)
{
  /* Discussion: Different xatlas implementation make different choices here, either
   * fixing the output bitmap size before packing begins, or sometimes allowing
   * for non-square outputs which can make the resulting algorithm a little simpler.
   *
   * The current implementation is to grow using the "Alpaca Rules" as described above, with calls
   * to increase_scale() if the particular packing instance is badly conditioned.
   *
   * (This particular choice is largely a result of the way packing is used inside the Blender API,
   * and isn't strictly required by the xatlas algorithm.)
   *
   * One nice extension to the xatlas algorithm might be to grow in all 4 directions, i.e. both
   * increasing and *decreasing* in the horizontal and vertical axes. The `scan_line` parameter
   * would become a #rctf, the occupancy bitmap would be 4x larger, and there will be a translation
   * to move the origin back to `(0,0)` at the end.
   *
   * This `plus-atlas` algorithm, which grows in a "+" shape, will likely have better packing
   * efficiency for many real world inputs, at a cost of increased complexity and memory.
   */

  const float bitmap_scale = 1.0f / occupancy.bitmap_scale_reciprocal;

  /* TODO: If `target_aspect_y != 1.0f`, to avoid aliasing issues, we should probably iterate
   * Separately on `scan_line_x` and `scan_line_y`. See also: Bresenham's algorithm. */
  const float sqrt_target_aspect_y = sqrtf(target_aspect_y);
  const int scan_line_x = int(scan_line * sqrt_target_aspect_y);
  const int scan_line_y = int(scan_line / sqrt_target_aspect_y);

  UVPhi phi;
  phi.rotation = DEG2RADF(angle_90_multiple * 90);
  // phi.reflect = reflect;
  float matrix[2][2];
  island->build_transformation(scale, phi.rotation, matrix);

  /* Caution, margin is zero for `support_diagonal` as we're tracking the top-right corner. */
  float2 support_diagonal = island->get_diagonal_support(scale, phi.rotation, 0.0f);

  /* Scan using an "Alpaca"-style search, first horizontally using "less-than". */
  int t = int(ceilf((2 * support_diagonal.x + margin) * occupancy.bitmap_scale_reciprocal));
  while (t < scan_line_x) { /* "less-than" */
    phi.translation = float2(t * bitmap_scale, scan_line_y * bitmap_scale) - support_diagonal;
    const float extent = occupancy.trace_island(island, phi, scale, margin, false);
    if (extent < 0.0f) {
      return phi; /* Success. */
    }
    t = t + std::max(1, int(extent));
  }

  /* Then scan vertically using "less-than-or-equal" */
  t = int(ceilf((2 * support_diagonal.y + margin) * occupancy.bitmap_scale_reciprocal));
  while (t <= scan_line_y) { /* "less-than-or-equal" */
    phi.translation = float2(scan_line_x * bitmap_scale, t * bitmap_scale) - support_diagonal;
    const float extent = occupancy.trace_island(island, phi, scale, margin, false);
    if (extent < 0.0f) {
      return phi; /* Success. */
    }
    t = t + std::max(1, int(extent));
  }

  return UVPhi(); /* Unable to find a place to fit. */
}

static float guess_initial_scale(const Span<PackIsland *> islands,
                                 const float scale,
                                 const float margin)
{
  float sum = 1e-40f;
  for (int64_t i : islands.index_range()) {
    PackIsland *island = islands[i];
    sum += island->half_diagonal_.x * 2 * scale + 2 * margin;
    sum += island->half_diagonal_.y * 2 * scale + 2 * margin;
  }
  return sqrtf(sum) / 6.0f;
}

/** Helper to find the minimum enclosing square. */
class UVMinimumEnclosingSquareFinder {
 public:
  const float scale_;
  const float margin_;
  const UVPackIsland_Params *params_;

  float best_quad;
  float best_angle;
  rctf best_bounds;

  Vector<float2> points;
  Vector<int> indices;

  UVMinimumEnclosingSquareFinder(const float scale,
                                 const float margin,
                                 const UVPackIsland_Params *params)
      : scale_(scale), margin_(margin), params_(params)
  {
    best_angle = 0.0f;
    best_quad = 0.0f;
  }

  /** Calculates the square associated with a rotation of `angle`.
   * \return Size of square. */

  float update(const double angle)
  {
    float2 dir(cos(angle), sin(angle));

    /* TODO: Once convexhull_2d bugs are fixed, we can use "rotating calipers" to go faster. */
    rctf bounds;
    BLI_rctf_init_minmax(&bounds);
    for (const int64_t i : indices.index_range()) {
      const float2 &p = points[indices[i]];
      const float uv[2] = {p.x * dir.x + p.y * dir.y, -p.x * dir.y + p.y * dir.x};
      BLI_rctf_do_minmax_v(&bounds, uv);
    }
    bounds.xmin -= margin_;
    bounds.ymin -= margin_;
    bounds.xmax += margin_;
    bounds.ymax += margin_;
    const float current_quad = get_aspect_scaled_extent(bounds, *params_);
    if (best_quad > current_quad) {
      best_quad = current_quad;
      best_angle = angle;
      best_bounds = bounds;
    }
    return current_quad;
  }

  /** Search between `angle0` and `angle1`, looking for the smallest square. */
  void update_recursive(const float angle0,
                        const float quad0,
                        const float angle1,
                        const float quad1)
  {
    const float angle_mid = (angle0 + angle1) * 0.5f;
    const float quad_mid = update(angle_mid);
    const float angle_separation = angle1 - angle0;

    if (angle_separation < DEG2RADF(0.002f)) {
      return; /* Sufficient accuracy achieved. */
    }

    bool search_mode = DEG2RADF(10.0f) < angle_separation; /* In linear search mode. */

    /* TODO: Degenerate inputs could have poor performance here. */
    if (search_mode || (quad0 <= quad1)) {
      update_recursive(angle0, quad0, angle_mid, quad_mid);
    }
    if (search_mode || (quad1 <= quad0)) {
      update_recursive(angle_mid, quad_mid, angle1, quad1);
    }
  }
};

/**
 * Find the minimum bounding square that encloses the UVs as specified in `r_phis`.
 * If that square is smaller than `r_extent`, then update `r_phis` accordingly.
 * \return True if `r_phis` and `r_extent` are modified.
 */
static bool rotate_inside_square(const Span<std::unique_ptr<UVAABBIsland>> island_indices,
                                 const Span<PackIsland *> islands,
                                 const UVPackIsland_Params &params,
                                 const float scale,
                                 const float margin,
                                 MutableSpan<UVPhi> r_phis,
                                 rctf *r_extent)
{
  if (island_indices.is_empty()) {
    return false; /* Nothing to do. */
  }
  if (params.rotate_method != ED_UVPACK_ROTATION_ANY) {
    return false; /* Unable to rotate by arbitrary angle. */
  }
  if (params.shape_method == ED_UVPACK_SHAPE_AABB) {
    /* AABB margin calculations are not preserved under rotations. */
    if (island_indices.size() > 1) { /* Unless there's only one island. */

      if (params.target_aspect_y != 1.0f) {
        /* TODO: Check for possible 90 degree rotation. */
      }
      return false;
    }
  }

  UVMinimumEnclosingSquareFinder square_finder(scale, margin, &params);
  square_finder.best_quad = get_aspect_scaled_extent(*r_extent, params) * 0.999f;

  float matrix[2][2];

  const float aspect_y = 1.0f; /* TODO: Use `islands[0]->aspect_y`. */
  for (const int64_t j : island_indices.index_range()) {
    const int64_t i = island_indices[j]->index;
    const PackIsland *island = islands[i];
    if (island->aspect_y != aspect_y) {
      return false; /* Aspect ratios are not preserved under rotation. */
    }
    const float island_scale = island->can_scale_(params) ? scale : 1.0f;
    island->build_transformation(island_scale, r_phis[i].rotation, matrix);
    float2 pivot_transformed;
    mul_v2_m2v2(pivot_transformed, matrix, island->pivot_);
    float2 delta = r_phis[i].translation - pivot_transformed;

    for (const int64_t k : island->triangle_vertices_.index_range()) {
      float2 p = island->triangle_vertices_[k];
      mul_m2_v2(matrix, p);
      square_finder.points.append(p + delta);
    }
  }

  /* Now we have all the points in the correct space, compute the 2D convex hull. */
  square_finder.indices.resize(square_finder.points.size()); /* Allocate worst-case. */
  int convex_size = BLI_convexhull_2d(square_finder.points, square_finder.indices.data());
  square_finder.indices.resize(convex_size); /* Resize to actual size. */

  /* Run the computation to find the best angle. (Slow!) */
  const float quad_180 = square_finder.update(DEG2RADF(-180.0f));
  square_finder.update_recursive(DEG2RADF(-180.0f), quad_180, DEG2RADF(180.0f), quad_180);

  if (square_finder.best_angle == 0.0f) {
    return false; /* Nothing to do. */
  }

  /* Transform phis, rotate by best_angle, then translate back to the origin. No scale. */
  for (const int64_t j : island_indices.index_range()) {
    const int64_t i = island_indices[j]->index;
    const PackIsland *island = islands[i];
    const float identity_scale = 1.0f; /* Don't rescale the placement, just rotate. */
    island->build_transformation(identity_scale, square_finder.best_angle, matrix);
    r_phis[i].rotation += square_finder.best_angle;
    mul_m2_v2(matrix, r_phis[i].translation);
    r_phis[i].translation.x -= square_finder.best_bounds.xmin;
    r_phis[i].translation.y -= square_finder.best_bounds.ymin;
  }

  /* Write back new extent, translated to the origin. */
  r_extent->xmin = 0.0f;
  r_extent->ymin = 0.0f;
  r_extent->xmax = BLI_rctf_size_x(&square_finder.best_bounds);
  r_extent->ymax = BLI_rctf_size_y(&square_finder.best_bounds);
  return true; /* `r_phis` and `r_extent` were modified. */
}

/**
 * Pack irregular islands using the `xatlas` strategy, and optional D4 transforms.
 *
 * Loosely based on the 'xatlas' code by Jonathan Young
 * from https://github.com/jpcy/xatlas
 *
 * A brute force packer (BF-Packer) with accelerators:
 * - Uses a Bitmap Occupancy class.
 * - Uses a "Witness Pixel" and a "Triangle Hint".
 * - Write with `margin * 2`, read with `margin == 0`.
 * - Lazy resetting of BF search.
 *
 * Performance of "xatlas" would normally be `O(n^4)` (or worse!), however, in our
 * implementation, `bitmap_radix` is a constant, which reduces the time complexity to `O(n^3)`.
 * => if `n` can ever be large, `bitmap_radix` will need to vary accordingly.
 */

static int64_t pack_island_xatlas(const Span<std::unique_ptr<UVAABBIsland>> island_indices,
                                  const Span<PackIsland *> islands,
                                  const float scale,
                                  const float margin,
                                  const UVPackIsland_Params &params,
                                  MutableSpan<UVPhi> r_phis,
                                  rctf *r_extent)
{
  if (params.shape_method == ED_UVPACK_SHAPE_AABB) {
    return 0; /* Not yet supported. */
  }
  Array<UVPhi> phis(r_phis.size());
  Occupancy occupancy(guess_initial_scale(islands, scale, margin));
  rctf extent = {0.0f, 0.0f, 0.0f, 0.0f};

  /* A heuristic to improve final layout efficiency by making an
   * intermediate call to #rotate_inside_square. */
  int64_t square_milestone = sqrt(island_indices.size()) / 4 + 2;

  int scan_line = 0;      /* Current "scan_line" of occupancy bitmap. */
  int traced_islands = 0; /* Which islands are currently traced in `occupancy`. */
  int i = 0;
  bool placed_can_rotate = true;

  /* The following `while` loop is setting up a three-way race:
   * `for (scan_line = 0; scan_line < bitmap_radix; scan_line++)`
   * `for (i : island_indices.index_range())`
   * `while (bitmap_scale_reciprocal > 0) { bitmap_scale_reciprocal *= 0.5f; }`
   */

  while (i < island_indices.size()) {

    if (params.stop && G.is_break) {
      *params.stop = true;
    }
    if (params.isCancelled()) {
      break;
    }

    while (traced_islands < i) {
      /* Trace an island that's been solved. (Greedy.) */
      const int64_t island_index = island_indices[traced_islands]->index;
      PackIsland *island = islands[island_index];
      const float island_scale = island->can_scale_(params) ? scale : 1.0f;
      occupancy.trace_island(island, phis[island_index], island_scale, margin, true);
      traced_islands++;
    }

    PackIsland *island = islands[island_indices[i]->index];
    UVPhi phi; /* Create an identity transform. */

    if (!island->can_translate_(params)) {
      /* Move the pinned island into the correct coordinate system. */
      phi.translation = island->pivot_;
      sub_v2_v2(phi.translation, params.udim_base_offset);
      phi.rotation = 0.0f;
      phis[island_indices[i]->index] = phi;
      i++;
      placed_can_rotate = false; /* Further rotation will cause a translation. */
      continue;                  /* `island` is now completed. */
    }
    const float island_scale = island->can_scale_(params) ? scale : 1.0f;

    int max_90_multiple = 1;
    if (island->can_rotate_(params)) {
      if (i && (i < 50)) {
        max_90_multiple = 4;
      }
    }
    else {
      placed_can_rotate = false;
    }

    for (int angle_90_multiple = 0; angle_90_multiple < max_90_multiple; angle_90_multiple++) {
      phi = find_best_fit_for_island(island,
                                     scan_line,
                                     occupancy,
                                     island_scale,
                                     angle_90_multiple,
                                     margin,
                                     params.target_aspect_y);
      if (phi.is_valid()) {
        break;
      }
    }

    if (!phi.is_valid()) {
      /* Unable to find a fit on this scan_line. */

      island = nullptr; /* Just mark it as null, we won't use it further. */

      if (i < 10) {
        scan_line++;
      }
      else {
        /* Increasing by 2 here has the effect of changing the sampling pattern.
         * The parameter '2' is not "free" in the sense that changing it requires
         * a change to `bitmap_radix` and then re-tuning `alpaca_cutoff`.
         * Possible values here *could* be 1, 2 or 3, however the only *reasonable*
         * choice is 2. */
        scan_line += 2;
      }
      if (scan_line < occupancy.bitmap_radix *
                          sqrtf(std::min(params.target_aspect_y, 1.0f / params.target_aspect_y)))
      {
        continue; /* Try again on next scan_line. */
      }

      /* Enlarge search parameters. */
      scan_line = 0;
      occupancy.increase_scale();
      traced_islands = 0; /* Will trigger a re-trace of previously solved islands. */
      continue;
    }

    /* Place island. */
    phis[island_indices[i]->index] = phi;
    i++; /* Next island. */

    if (i == square_milestone && placed_can_rotate) {
      if (rotate_inside_square(
              island_indices.take_front(i), islands, params, scale, margin, phis, &extent))
      {
        scan_line = 0;
        traced_islands = 0;
        occupancy.clear();
        continue;
      }
    }

    /* Update top-right corner. */
    float2 top_right = island->get_diagonal_support(island_scale, phi.rotation, margin) +
                       phi.translation;
    extent.xmax = std::max(top_right.x, extent.xmax);
    extent.ymax = std::max(top_right.y, extent.ymax);

    if (!is_larger(*r_extent, extent, params)) {
      if (i >= square_milestone) {
        return 0; /* Early exit, we already have a better layout. */
      }
    }

    /* Heuristics to reduce size of brute-force search. */
    if (i < 128 || (i & 31) == 16) {
      scan_line = 0; /* Restart completely. */
    }
    else {
      scan_line = std::max(0, scan_line - 25); /* `-25` must by odd. */
    }

    if (params.progress) {
      /* We don't (yet) have a good model for how long the pack operation is going
       * to take, so just update the progress a little bit. */
      const float previous_progress = *params.progress;
      *params.do_update = true;
      const float reduction = island_indices.size() / (island_indices.size() + 0.5f);
      *params.progress = 1.0f - (1.0f - previous_progress) * reduction;
    }
  }

  /* TODO: if (i != island_indices.size()) { ??? } */

  if (!is_larger(*r_extent, extent, params)) {
    return 0;
  }

  /* Our pack is an improvement on the one passed in. Write it back. */
  *r_extent = extent;
  for (int64_t j = 0; j < i; j++) {
    const int64_t island_index = island_indices[j]->index;
    r_phis[island_index] = phis[island_index];
  }
  return i; /* Return the number of islands which were packed. */
}

/**
 * Pack islands using a mix of other strategies.
 * \param islands: The islands to be packed.
 * \param scale: Scale islands by `scale` before packing.
 * \param margin: Add `margin` units around islands before packing.
 * \param params: Additional parameters. Scale and margin information is ignored.
 * \param r_phis: Island layout information will be written here.
 * \return Size of square covering the resulting packed UVs. The maximum `u` or `v` coordinate.
 */
static float pack_islands_scale_margin(const Span<PackIsland *> islands,
                                       const float scale,
                                       const float margin,
                                       const UVPackIsland_Params &params,
                                       MutableSpan<UVPhi> r_phis)
{
  /* #BLI_box_pack_2d produces layouts with high packing efficiency, but has `O(n^3)`
   * time complexity, causing poor performance if there are lots of islands. See: #102843.
   * #pack_islands_alpaca_turbo is designed to be the fastest packing method, `O(nlogn)`,
   * but has poor packing efficiency if the AABBs have a spread of sizes and aspect ratios.
   * Here, we merge the best properties of both packers into one combined packer.
   *
   * The free tuning parameter, `alpaca_cutoff` will determine how many islands are packed
   * using each method.
   *
   * The current strategy is:
   * - Sort islands in size order.
   * - Try #pack_island_optimal_pack packer first
   * - Call #pack_island_xatlas on the first `alpaca_cutoff` islands.
   * - Also call #BLI_box_pack_2d on the first `alpaca_cutoff` islands.
   * - Choose the best layout so far.
   * - Rotate into the minimum bounding square.
   * - Call #pack_islands_alpaca_* on the remaining islands.
   */

  const bool all_can_rotate = can_rotate(islands, params);

  /* First, copy information from our input into the AABB structure. */
  Array<std::unique_ptr<UVAABBIsland>> aabbs(islands.size());
  for (const int64_t i : islands.index_range()) {
    PackIsland *pack_island = islands[i];
    float island_scale = scale;
    if (!pack_island->can_scale_(params)) {
      island_scale = 1.0f;
    }
    std::unique_ptr<UVAABBIsland> aabb = std::make_unique<UVAABBIsland>();
    aabb->index = i;
    aabb->uv_diagonal.x = pack_island->half_diagonal_.x * 2 * island_scale + 2 * margin;
    aabb->uv_diagonal.y = pack_island->half_diagonal_.y * 2 * island_scale + 2 * margin;
    aabb->aspect_y = pack_island->aspect_y;
    aabbs[i] = std::move(aabb);
  }

  /* Sort from "biggest" to "smallest". */

  if (all_can_rotate) {
    std::stable_sort(
        aabbs.begin(),
        aabbs.end(),
        [&](const std::unique_ptr<UVAABBIsland> &a, const std::unique_ptr<UVAABBIsland> &b) {
          const bool can_translate_a = islands[a->index]->can_translate_(params);
          const bool can_translate_b = islands[b->index]->can_translate_(params);
          if (can_translate_a != can_translate_b) {
            return can_translate_b; /* Locked islands are placed first. */
          }
          /* TODO: Fix when (params.target_aspect_y != 1.0f) */

          /* Choose the AABB with the longest large edge. */
          float a_u = a->uv_diagonal.x * a->aspect_y;
          float a_v = a->uv_diagonal.y;
          float b_u = b->uv_diagonal.x * b->aspect_y;
          float b_v = b->uv_diagonal.y;
          if (a_u > a_v) {
            std::swap(a_u, a_v);
          }
          if (b_u > b_v) {
            std::swap(b_u, b_v);
          }
          float diff_u = a_u - b_u;
          float diff_v = a_v - b_v;
          diff_v += diff_u * 0.05f; /* Robust sort, smooth over round-off errors. */
          if (diff_v == 0.0f) {     /* Tie break. */
            return diff_u > 0.0f;
          }
          return diff_v > 0.0f;
        });
  }
  else {
    std::stable_sort(
        aabbs.begin(),
        aabbs.end(),
        [&](const std::unique_ptr<UVAABBIsland> &a, const std::unique_ptr<UVAABBIsland> &b) {
          const bool can_translate_a = islands[a->index]->can_translate_(params);
          const bool can_translate_b = islands[b->index]->can_translate_(params);
          if (can_translate_a != can_translate_b) {
            return can_translate_b; /* Locked islands are placed first. */
          }

          /* Choose the AABB with larger rectangular area. */
          return b->uv_diagonal.x * b->uv_diagonal.y < a->uv_diagonal.x * a->uv_diagonal.y;
        });
  }

  /* If some of the islands are locked, we build a summary about them here. */
  rctf locked_bounds = {0.0f};     /* AABB of islands which can't translate. */
  int64_t locked_island_count = 0; /* Index of first non-locked island. */
  for (int64_t i = 0; i < islands.size(); i++) {
    PackIsland *pack_island = islands[aabbs[i]->index];
    if (pack_island->can_translate_(params)) {
      break;
    }
    float2 bottom_left = pack_island->pivot_ - pack_island->half_diagonal_;
    float2 top_right = pack_island->pivot_ + pack_island->half_diagonal_;
    if (i == 0) {
      locked_bounds.xmin = bottom_left.x;
      locked_bounds.xmax = top_right.x;
      locked_bounds.ymin = bottom_left.y;
      locked_bounds.ymax = top_right.y;
    }
    else {
      BLI_rctf_do_minmax_v(&locked_bounds, bottom_left);
      BLI_rctf_do_minmax_v(&locked_bounds, top_right);
    }

    UVPhi &phi = r_phis[aabbs[i]->index]; /* Lock in place. */
    phi.translation = pack_island->pivot_;
    sub_v2_v2(phi.translation, params.udim_base_offset);
    phi.rotation = 0.0f;

    locked_island_count = i + 1;
  }

  /* Partition `islands`, largest islands will go to a slow packer, the rest the fast packer.
   * See discussion above for details. */
  int64_t alpaca_cutoff = 1024; /* Regular situation, pack `32 * 32` islands with slow packer. */
  int64_t alpaca_cutoff_fast = 81; /* Reduce problem size, only `N = 9 * 9` with slow packer. */
  if (params.margin_method == ED_UVPACK_MARGIN_FRACTION) {
    if (margin > 0.0f) {
      alpaca_cutoff = alpaca_cutoff_fast;
    }
  }

  alpaca_cutoff = std::max(alpaca_cutoff, locked_island_count); /* ...TODO... */

  Span<std::unique_ptr<UVAABBIsland>> slow_aabbs = aabbs.as_span().take_front(
      std::min(alpaca_cutoff, islands.size()));
  rctf extent = {0.0f, 1e30f, 0.0f, 1e30f};

  /* Call the "fast" packer, which can sometimes give optimal results. */
  pack_islands_fast(locked_island_count,
                    locked_bounds,
                    slow_aabbs,
                    all_can_rotate,
                    params.target_aspect_y,
                    r_phis,
                    &extent);
  rctf fast_extent = extent; /* Remember how large the "fast" packer was. */

  /* Call the "optimal" packer. */
  if (locked_island_count == 0) {
    pack_islands_optimal_pack(slow_aabbs, params, r_phis, &extent);
  }

  /* Call box_pack_2d (slow for large N.) */
  if (locked_island_count == 0) { /* box_pack_2d doesn't yet support locked islands. */
    pack_island_box_pack_2d(slow_aabbs, params, r_phis, &extent);
  }

  /* Call xatlas (slow for large N.) */
  int64_t max_xatlas = pack_island_xatlas(
      slow_aabbs, islands, scale, margin, params, r_phis, &extent);
  if (max_xatlas) {
    slow_aabbs = aabbs.as_span().take_front(max_xatlas);
  }

  /* At this stage, `extent` contains the fast/optimal/box_pack/xatlas UVs. */

  /* If more islands remain to be packed, attempt to improve the layout further by finding the
   * minimal-bounding-square. Disabled for other cases as users often prefer to avoid diagonal
   * islands. */
  if (all_can_rotate && aabbs.size() > slow_aabbs.size()) {
    rotate_inside_square(slow_aabbs, islands, params, scale, margin, r_phis, &extent);
  }

  if (BLI_rctf_compare(&extent, &fast_extent, 0.0f)) {
    /* The fast packer was the best so far. Lets just use the fast packer for everything. */
    slow_aabbs = slow_aabbs.take_front(locked_island_count);
    extent = locked_bounds;
  }

  /* Call fast packer for remaining islands, excluding everything already placed. */
  rctf final_extent = {0.0f, 1e30f, 0.0f, 1e30f};
  pack_islands_fast(slow_aabbs.size(),
                    extent,
                    aabbs,
                    all_can_rotate,
                    params.target_aspect_y,
                    r_phis,
                    &final_extent);

  return get_aspect_scaled_extent(final_extent, params);
}

/**
 * Find the optimal scale to pack islands into the unit square.
 * returns largest scale that will pack `islands` into the unit square.
 */
static float pack_islands_margin_fraction(const Span<PackIsland *> islands,
                                          const float margin_fraction,
                                          const bool rescale_margin,
                                          const UVPackIsland_Params &params)
{
  /*
   * Root finding using a combined search / modified-secant method.
   * First, use a robust search procedure to bracket the root within a factor of 10.
   * Then, use a modified-secant method to converge.
   *
   * This is a specialized solver using domain knowledge to accelerate convergence. */

  float scale_low = 0.0f;
  float value_low = 0.0f;
  float scale_high = 0.0f;
  float value_high = 0.0f;

  Array<UVPhi> phis_a(islands.size());
  Array<UVPhi> phis_b(islands.size());
  Array<UVPhi> *phis_low = nullptr;

  /* Scaling smaller than `min_scale_roundoff` is unlikely to fit and
   * will destroy information in existing UVs. */
  const float min_scale_roundoff = 1e-5f;

  /* Certain inputs might have poor convergence properties.
   * Use `max_iteration` to prevent an infinite loop. */
  const int max_iteration = 25;
  for (int iteration = 0; iteration < max_iteration; iteration++) {
    float scale = 1.0f;

    if (iteration == 0) {
      BLI_assert(iteration == 0);
      BLI_assert(scale == 1.0f);
      BLI_assert(scale_low == 0.0f);
      BLI_assert(scale_high == 0.0f);
    }
    else if (scale_low == 0.0f) {
      BLI_assert(scale_high > 0.0f);
      /* Search mode, shrink layout until we can find a scale that fits. */
      scale = scale_high * 0.1f;
    }
    else if (scale_high == 0.0f) {
      BLI_assert(scale_low > 0.0f);
      /* Search mode, grow layout until we can find a scale that doesn't fit. */
      scale = scale_low * 10.0f;
    }
    else {
      /* Bracket mode, use modified secant method to find root. */
      BLI_assert(scale_low > 0.0f);
      BLI_assert(scale_high > 0.0f);
      BLI_assert(value_low <= 0.0f);
      BLI_assert(value_high >= 0.0f);
      if (scale_high < scale_low * 1.0001f) {
        /* Convergence. */
        break;
      }

      /* Secant method for area. */
      scale = (sqrtf(scale_low) * value_high - sqrtf(scale_high) * value_low) /
              (value_high - value_low);
      scale = scale * scale;

      if (iteration & 1) {
        /* Modified binary-search to improve robustness. */
        scale = sqrtf(scale * sqrtf(scale_low * scale_high));
      }

      BLI_assert(scale_low < scale);
      BLI_assert(scale < scale_high);
    }

    scale = std::max(scale, min_scale_roundoff);

    /* Evaluate our `f`. */
    Array<UVPhi> *phis_target = (phis_low == &phis_a) ? &phis_b : &phis_a;
    const float margin = rescale_margin ? margin_fraction * scale : margin_fraction;
    const float max_uv = pack_islands_scale_margin(islands, scale, margin, params, *phis_target) /
                         params.target_extent;
    const float value = sqrtf(max_uv) - 1.0f;

    if (value <= 0.0f) {
      scale_low = scale;
      value_low = value;
      phis_low = phis_target;
      if (value == 0.0f) {
        break; /* Target hit exactly. */
      }
    }
    else {
      scale_high = scale;
      value_high = value;
      if (scale == min_scale_roundoff) {
        /* Unable to pack without damaging UVs. */
        scale_low = scale;
        break;
      }
      if (!phis_low) {
        phis_low = phis_target; /* May as well do "something", even if it's wrong. */
      }
    }
  }

  if (phis_low) {
    /* Write back best pack as a side-effect. */
    for (const int64_t i : islands.index_range()) {
      PackIsland *island = islands[i];
      const float island_scale = island->can_scale_(params) ? scale_low : 1.0f;
      island->place_(island_scale, (*phis_low)[i]);
    }
  }
  return scale_low;
}

static float calc_margin_from_aabb_length_sum(const Span<PackIsland *> island_vector,
                                              const UVPackIsland_Params &params)
{
  /* Logic matches previous behavior from #geometry::uv_parametrizer_pack.
   * Attempt to give predictable results not dependent on current UV scale by using
   * `aabb_length_sum` (was "`area`") to multiply the margin by the length (was "area"). */
  double aabb_length_sum = 0.0f;
  for (PackIsland *island : island_vector) {
    float w = island->half_diagonal_.x * 2.0f;
    float h = island->half_diagonal_.y * 2.0f;
    aabb_length_sum += sqrtf(w * h);
  }
  return params.margin * aabb_length_sum * 0.1f;
}

/* -------------------------------------------------------------------- */
/** \name Implement `pack_islands`
 *
 * \{ */

static bool overlap_aabb(const float2 &pivot_a,
                         const float2 &half_diagonal_a,
                         const float2 &pivot_b,
                         const float2 &half_diagonal_b)
{
  if (pivot_a.x + half_diagonal_a.x <= pivot_b.x - half_diagonal_b.x) {
    return false;
  }
  if (pivot_a.y + half_diagonal_a.y <= pivot_b.y - half_diagonal_b.y) {
    return false;
  }
  if (pivot_b.x + half_diagonal_b.x <= pivot_a.x - half_diagonal_a.x) {
    return false;
  }
  if (pivot_b.y + half_diagonal_b.y <= pivot_a.y - half_diagonal_a.y) {
    return false;
  }
  return true;
}

class OverlapMerger {
 public:
  static bool overlap(PackIsland *a, PackIsland *b)
  {
    if (a->aspect_y != b->aspect_y) {
      return false; /* Cannot merge islands with different aspect ratios. */
    }
    if (!overlap_aabb(a->pivot_, a->half_diagonal_, b->pivot_, b->half_diagonal_)) {
      return false; /* AABBs are disjoint => islands are separate. */
    }
    for (int i = 0; i < a->triangle_vertices_.size(); i += 3) {
      for (int j = 0; j < b->triangle_vertices_.size(); j += 3) {
        if (isect_tri_tri_v2(a->triangle_vertices_[i + 0],
                             a->triangle_vertices_[i + 1],
                             a->triangle_vertices_[i + 2],
                             b->triangle_vertices_[j + 0],
                             b->triangle_vertices_[j + 1],
                             b->triangle_vertices_[j + 2]))
        {
          return true; /* Two triangles overlap => islands overlap. */
        }
      }
    }

    return false; /* Separate. */
  }

  static void add_geometry(PackIsland *dest, const PackIsland *source)
  {
    for (int64_t i = 0; i < source->triangle_vertices_.size(); i += 3) {
      dest->add_triangle(source->triangle_vertices_[i],
                         source->triangle_vertices_[i + 1],
                         source->triangle_vertices_[i + 2]);
    }
  }

  /** Return a new root of the binary tree, with `a` and `b` as leaves. */
  static PackIsland *merge_islands(const PackIsland *a, const PackIsland *b)
  {
    PackIsland *result = new PackIsland();
    result->aspect_y = sqrtf(a->aspect_y * b->aspect_y);
    result->caller_index = -1;
    result->pinned = a->pinned || b->pinned;
    add_geometry(result, a);
    add_geometry(result, b);
    result->calculate_pivot_();
    return result;
  }

  static float pack_islands_overlap(const Span<PackIsland *> islands,
                                    const UVPackIsland_Params &params)
  {

    /* Building the binary-tree of merges is complicated to do in a single pass if we proceed in
     * the forward order. Instead we'll continuously update the tree as we descend, with
     * `sub_islands` doing the work of our stack. See #merge_islands for details.
     *
     * Technically, performance is O(n^2). In practice, should be fast enough. */

    Vector<PackIsland *> sub_islands; /* Pack these islands instead. */
    Vector<PackIsland *> merge_trace; /* Trace merge information. */
    for (const int64_t i : islands.index_range()) {
      PackIsland *island = islands[i];
      island->calculate_pivot_();

      /* Loop backwards, building a binary tree of all merged islands as we descend. */
      for (int64_t j = sub_islands.size() - 1; j >= 0; j--) {
        if (overlap(island, sub_islands[j])) {
          merge_trace.append(island);
          merge_trace.append(sub_islands[j]);
          island = merge_islands(island, sub_islands[j]);
          merge_trace.append(island);
          sub_islands.remove(j);
        }
      }
      sub_islands.append(island);
    }

    /* Recursively call pack_islands with `merge_overlap = false`. */
    UVPackIsland_Params sub_params(params);
    sub_params.merge_overlap = false;
    const float result = pack_islands(sub_islands, sub_params);

    /* Must loop backwards, or we will miss sub-sub-islands. */
    for (int64_t i = merge_trace.size() - 3; i >= 0; i -= 3) {
      PackIsland *sub_a = merge_trace[i];
      PackIsland *sub_b = merge_trace[i + 1];
      PackIsland *merge = merge_trace[i + 2];

      /* Copy `angle`, `pre_translate` and `pre_rotate` from merged island to sub islands. */
      sub_a->angle = merge->angle;
      sub_b->angle = merge->angle;
      sub_a->pre_translate = merge->pre_translate;
      sub_b->pre_translate = merge->pre_translate;
      sub_a->pre_rotate_ = merge->pre_rotate_;
      sub_b->pre_rotate_ = merge->pre_rotate_;

      /* If the merged island is pinned, the sub-islands are also pinned to correct scaling. */
      if (merge->pinned) {
        sub_a->pinned = true;
        sub_b->pinned = true;
      }
      delete merge;
    }

    return result;
  }
};

static void finalize_geometry(const Span<PackIsland *> islands, const UVPackIsland_Params &params)
{
  MemArena *arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  Heap *heap = BLI_heap_new();
  for (const int64_t i : islands.index_range()) {
    islands[i]->finalize_geometry_(params, arena, heap);
    BLI_memarena_clear(arena);
  }

  BLI_heap_free(heap, nullptr);
  BLI_memarena_free(arena);
}

float pack_islands(const Span<PackIsland *> islands, const UVPackIsland_Params &params)
{
  BLI_assert(0.0f <= params.margin);
  BLI_assert(0.0f <= params.target_aspect_y);

  if (islands.is_empty()) {
    return 1.0f; /* Nothing to do, just create a safe default. */
  }

  if (params.merge_overlap) {
    return OverlapMerger::pack_islands_overlap(islands, params);
  }

  finalize_geometry(islands, params);

  /* Count the number of islands which can scale and which can translate. */
  int64_t can_scale_count = 0;
  int64_t can_translate_count = 0;
  for (const int64_t i : islands.index_range()) {
    if (islands[i]->can_scale_(params)) {
      can_scale_count++;
    }
    if (islands[i]->can_translate_(params)) {
      can_translate_count++;
    }
  }

  if (can_translate_count == 0) {
    return 1.0f; /* Nothing to do, all islands are locked. */
  }

  if (params.margin_method == ED_UVPACK_MARGIN_FRACTION && params.margin > 0.0f &&
      can_scale_count > 0)
  {
    /* Uses a line search on scale. ~10x slower than other method. */
    return pack_islands_margin_fraction(islands, params.margin, false, params);
  }

  float margin = params.margin;
  switch (params.margin_method) {
    case ED_UVPACK_MARGIN_ADD:    /* Default for Blender 2.8 and earlier. */
      break;                      /* Nothing to do. */
    case ED_UVPACK_MARGIN_SCALED: /* Default for Blender 3.3 and later. */
      margin = calc_margin_from_aabb_length_sum(islands, params);
      break;
    case ED_UVPACK_MARGIN_FRACTION: /* Added as an option in Blender 3.4. */
      /* Most other cases are handled above, unless pinning is involved. */
      break;
    default:
      BLI_assert_unreachable();
  }

  if (can_scale_count > 0 && can_scale_count != islands.size()) {
    /* Search for the best scale parameter. (slow) */
    return pack_islands_margin_fraction(islands, margin, true, params);
  }

  /* Either all of the islands can scale, or none of them can.
   * In either case, we pack them all tight to the origin. */
  Array<UVPhi> phis(islands.size());
  const float scale = 1.0f;
  const float max_uv = pack_islands_scale_margin(islands, scale, margin, params, phis);
  const float result = can_scale_count && max_uv > 1e-14f ? params.target_extent / max_uv : 1.0f;
  for (const int64_t i : islands.index_range()) {
    BLI_assert(result == 1.0f || islands[i]->can_scale_(params));
    islands[i]->place_(scale, phis[i]);
  }
  return result;
}

/** \} */

void PackIsland::build_transformation(const float scale,
                                      const double angle,
                                      float (*r_matrix)[2]) const
{
  const double cos_angle = cos(angle);
  const double sin_angle = sin(angle);
  r_matrix[0][0] = cos_angle * scale;
  r_matrix[0][1] = -sin_angle * scale * aspect_y;
  r_matrix[1][0] = sin_angle * scale / aspect_y;
  r_matrix[1][1] = cos_angle * scale;
#if 0
  if (reflect) {
    r_matrix[0][0] *= -1.0f;
    r_matrix[0][1] *= -1.0f;
  }
#endif
}

void PackIsland::build_inverse_transformation(const float scale,
                                              const double angle,
                                              float (*r_matrix)[2]) const
{
  const double cos_angle = cos(angle);
  const double sin_angle = sin(angle);

  r_matrix[0][0] = cos_angle / scale;
  r_matrix[0][1] = sin_angle / scale * aspect_y;
  r_matrix[1][0] = -sin_angle / scale / aspect_y;
  r_matrix[1][1] = cos_angle / scale;
#if 0
  if (reflect) {
    r_matrix[0][0] *= -1.0f;
    r_matrix[1][0] *= -1.0f;
  }
#endif
}

static bool can_rotate_with_method(const PackIsland &island,
                                   const UVPackIsland_Params &params,
                                   const eUVPackIsland_RotationMethod rotate_method)
{
  /* When axis aligned along X/Y coordinates, rotation is performed once early on,
   * but no rotation is allowed when packing. */
  if (ELEM(rotate_method,
           ED_UVPACK_ROTATION_NONE,
           ED_UVPACK_ROTATION_AXIS_ALIGNED_X,
           ED_UVPACK_ROTATION_AXIS_ALIGNED_Y))
  {
    return false;
  }
  if (!island.pinned) {
    return true;
  }
  switch (params.pin_method) {
    case ED_UVPACK_PIN_LOCK_ALL:
    case ED_UVPACK_PIN_LOCK_ROTATION:
    case ED_UVPACK_PIN_LOCK_ROTATION_SCALE:
      return false;
    default:
      return true;
  }
}

bool PackIsland::can_rotate_before_pack_(const UVPackIsland_Params &params) const
{
  eUVPackIsland_RotationMethod rotate_method = params.rotate_method;
  if (ELEM(rotate_method, ED_UVPACK_ROTATION_AXIS_ALIGNED_X, ED_UVPACK_ROTATION_AXIS_ALIGNED_Y)) {
    rotate_method = ED_UVPACK_ROTATION_AXIS_ALIGNED;
  }
  return can_rotate_with_method(*this, params, rotate_method);
}

bool PackIsland::can_rotate_(const UVPackIsland_Params &params) const
{
  return can_rotate_with_method(*this, params, params.rotate_method);
}

bool PackIsland::can_scale_(const UVPackIsland_Params &params) const
{
  if (!params.scale_to_fit) {
    return false;
  }
  if (!pinned) {
    return true;
  }
  switch (params.pin_method) {
    case ED_UVPACK_PIN_LOCK_ALL:
    case ED_UVPACK_PIN_LOCK_SCALE:
    case ED_UVPACK_PIN_LOCK_ROTATION_SCALE:
      return false;
    default:
      return true;
  }
}

bool PackIsland::can_translate_(const UVPackIsland_Params &params) const
{
  if (!pinned) {
    return true;
  }
  switch (params.pin_method) {
    case ED_UVPACK_PIN_LOCK_ALL:
      return false;
    default:
      return true;
  }
}

}  // namespace blender::geometry
