/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include "GEO_uv_pack.hh"

#include "BLI_array.hh"
#include "BLI_bounds.hh"
#include "BLI_boxpack_2d.h"
#include "BLI_convexhull_2d.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_rect.h"
#include "BLI_vector.hh"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

namespace blender::geometry {

/* Store information about an island's placement such as translation, rotation and reflection. */
class uv_phi {
 public:
  uv_phi();
  bool is_valid() const;

  float2 translation;
  float rotation;
  /* bool reflect; */
};

uv_phi::uv_phi() : translation(-1.0f, -1.0f), rotation(0.0f)
{
  /* Initialize invalid. */
}

bool uv_phi::is_valid() const
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
static float dist_signed_squared_to_edge(float2 probe, float2 uva, float2 uvb)
{
  const float2 edge = uvb - uva;
  const float2 side = probe - uva;

  const float edge_length_squared = blender::math::length_squared(edge);
  /* Tolerance here is to avoid division by zero later. */
  if (edge_length_squared < 1e-40f) {
    return blender::math::length_squared(side);
  }

  const float numerator = edge.x * side.y - edge.y * side.x; /* c.f. cross product. */
  const float numerator_ssq = numerator >= 0.0f ? numerator * numerator : -numerator * numerator;

  return numerator_ssq / edge_length_squared;
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

void PackIsland::add_polygon(const blender::Span<float2> uvs, MemArena *arena, Heap *heap)
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
  const float(*source)[2] = reinterpret_cast<const float(*)[2]>(uvs.data());

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

void PackIsland::finalize_geometry(const UVPackIsland_Params &params, MemArena *arena, Heap *heap)
{
  BLI_assert(triangle_vertices_.size() >= 3);

  const eUVPackIsland_ShapeMethod shape_method = params.shape_method;
  if (shape_method == ED_UVPACK_SHAPE_CONVEX) {
    /* Compute convex hull of existing triangles. */
    if (triangle_vertices_.size() <= 3) {
      calculate_pivot();
      return; /* Trivial case, calculate pivot only. */
    }

    int vert_count = int(triangle_vertices_.size());

    /* Allocate storage. */
    int *index_map = static_cast<int *>(
        BLI_memarena_alloc(arena, sizeof(*index_map) * vert_count));

    /* Prepare input for convex hull. */
    const float(*source)[2] = reinterpret_cast<const float(*)[2]>(triangle_vertices_.data());

    /* Compute convex hull. */
    int convex_len = BLI_convexhull_2d(source, vert_count, index_map);

    /* Write back. */
    triangle_vertices_.clear();
    blender::Array<float2> convexVertices(convex_len);
    for (int i = 0; i < convex_len; i++) {
      convexVertices[i] = source[index_map[i]];
    }
    add_polygon(convexVertices, arena, heap);

    BLI_heap_clear(heap, nullptr);
  }
  calculate_pivot();
}

void PackIsland::calculate_pivot()
{
  /* `pivot_` is calculated as the center of the AABB,
   * However `pivot_` cannot be outside of the convex hull. */
  Bounds<float2> triangle_bounds = *bounds::min_max(triangle_vertices_.as_span());
  pivot_ = (triangle_bounds.min + triangle_bounds.max) * 0.5f;
  half_diagonal_ = (triangle_bounds.max - triangle_bounds.min) * 0.5f;
}

void PackIsland::place_(const float scale, const uv_phi phi)
{
  angle = phi.rotation;

  float matrix_inverse[2][2];
  build_inverse_transformation(scale, phi.rotation, matrix_inverse);
  mul_v2_m2v2(pre_translate, matrix_inverse, phi.translation);
  pre_translate -= pivot_;
}

UVPackIsland_Params::UVPackIsland_Params()
{
  rotate = false;
  only_selected_uvs = false;
  only_selected_faces = false;
  use_seams = false;
  correct_aspect = false;
  ignore_pinned = false;
  pin_unselected = false;
  margin = 0.001f;
  margin_method = ED_UVPACK_MARGIN_SCALED;
  udim_base_offset[0] = 0.0f;
  udim_base_offset[1] = 0.0f;
  target_aspect_y = 1.0f;
  shape_method = ED_UVPACK_SHAPE_AABB;
}

/* Compact representation for AABB packers. */
class UVAABBIsland {
 public:
  float2 uv_diagonal;
  float2 uv_placement;
  int64_t index;
  float angle;
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
static void pack_islands_alpaca_turbo(const Span<UVAABBIsland *> islands,
                                      const float target_aspect_y,
                                      float *r_max_u,
                                      float *r_max_v)
{
  /* Exclude an initial AABB near the origin. */
  float next_u1 = *r_max_u;
  float next_v1 = *r_max_v;
  bool zigzag = next_u1 < next_v1 * target_aspect_y; /* Horizontal or Vertical strip? */

  float u0 = zigzag ? next_u1 : 0.0f;
  float v0 = zigzag ? 0.0f : next_v1;

  /* Visit every island in order. */
  for (UVAABBIsland *island : islands) {
    const float dsm_u = island->uv_diagonal.x;
    const float dsm_v = island->uv_diagonal.y;

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
    island->uv_placement.x = u0 + dsm_u * 0.5f;
    island->uv_placement.y = v0 + dsm_v * 0.5f;
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

  /* Write back total pack AABB. */
  *r_max_u = next_u1;
  *r_max_v = next_v1;
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
static void pack_islands_alpaca_rotate(const Span<UVAABBIsland *> islands,
                                       const float target_aspect_y,
                                       float *r_max_u,
                                       float *r_max_v)
{
  /* Exclude an initial AABB near the origin. */
  float next_u1 = *r_max_u;
  float next_v1 = *r_max_v;
  bool zigzag = next_u1 / target_aspect_y < next_v1; /* Horizontal or Vertical strip? */

  /* Track an AABB "hole" which may be filled at any time. */
  float2 hole(0.0f);
  float2 hole_diagonal(0.0f);
  bool hole_rotate = false;

  float u0 = zigzag ? next_u1 : 0.0f;
  float v0 = zigzag ? 0.0f : next_v1;

  /* Visit every island in order. */
  for (UVAABBIsland *island : islands) {
    const float uvdiag_x = island->uv_diagonal.x * island->aspect_y;
    float min_dsm = std::min(uvdiag_x, island->uv_diagonal.y);
    float max_dsm = std::max(uvdiag_x, island->uv_diagonal.y);

    if (min_dsm < hole_diagonal.x && max_dsm < hole_diagonal.y) {
      /* Place island in the hole. */
      island->uv_placement.x = hole[0];
      island->uv_placement.y = hole[1];
      if (hole_rotate == (min_dsm == island->uv_diagonal.x)) {
        island->angle = DEG2RADF(90.0f);
        island->uv_placement.x += island->uv_diagonal.y * 0.5f / island->aspect_y;
        island->uv_placement.y += island->uv_diagonal.x * 0.5f * island->aspect_y;
      }
      else {
        island->angle = 0.0f;
        island->uv_placement.x += island->uv_diagonal.x * 0.5f;
        island->uv_placement.y += island->uv_diagonal.y * 0.5f;
      }

      /* Update space left in the hole. */
      float p[6];
      p[0] = hole[0];
      p[1] = hole[1];
      p[2] = hole[0] + (hole_rotate ? max_dsm : min_dsm) / island->aspect_y;
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
      restart = (next_u1 < u0 + min_dsm / island->aspect_y);
    }
    if (restart) {
      update_hole_rotate(hole, hole_diagonal, hole_rotate, u0, v0, next_u1, next_v1);
      /* We're at the end of a strip. Restart from U axis or V axis. */
      zigzag = next_u1 / target_aspect_y < next_v1;
      u0 = zigzag ? next_u1 : 0.0f;
      v0 = zigzag ? 0.0f : next_v1;
    }

    /* Place the island. */
    island->uv_placement.x = u0;
    island->uv_placement.y = v0;
    if (zigzag == (min_dsm == uvdiag_x)) {
      island->angle = DEG2RADF(90.0f);
      island->uv_placement.x += island->uv_diagonal.y * 0.5f / island->aspect_y;
      island->uv_placement.y += island->uv_diagonal.x * 0.5f * island->aspect_y;
    }
    else {
      island->angle = 0.0f;
      island->uv_placement.x += island->uv_diagonal.x * 0.5f;
      island->uv_placement.y += island->uv_diagonal.y * 0.5f;
    }

    /* Move according to the "Alpaca rules", with rotation. */
    if (zigzag) {
      /* Move upwards. */
      v0 += min_dsm;
      next_u1 = max_ff(next_u1, u0 + max_dsm / island->aspect_y);
      next_v1 = max_ff(next_v1, v0);
    }
    else {
      /* Move sideways. */
      u0 += min_dsm / island->aspect_y;
      next_v1 = max_ff(next_v1, v0 + max_dsm);
      next_u1 = max_ff(next_u1, u0);
    }
  }

  /* Write back total pack AABB. */
  *r_max_u = next_u1;
  *r_max_v = next_v1;
}

/* Wrapper around #BLI_box_pack_2d. */
static void pack_island_box_pack_2d(const Span<UVAABBIsland *> aabbs,
                                    const Span<PackIsland *> islands,
                                    const float scale,
                                    const float margin,
                                    const float target_aspect_y,
                                    float *r_max_u,
                                    float *r_max_v)
{
  /* Allocate storage. */
  BoxPack *box_array = static_cast<BoxPack *>(
      MEM_mallocN(sizeof(*box_array) * islands.size(), __func__));

  /* Prepare for box_pack_2d. */
  for (const int64_t i : aabbs.index_range()) {
    PackIsland *island = islands[aabbs[i]->index];
    BoxPack *box = box_array + i;
    box->w = (island->half_diagonal_.x * 2 * scale + 2 * margin) / target_aspect_y;
    box->h = island->half_diagonal_.y * 2 * scale + 2 * margin;
  }

  const bool sort_boxes = false; /* Use existing ordering from `aabbs`. */

  /* \note Writes to `*r_max_u` and `*r_max_v`. */
  BLI_box_pack_2d(box_array, int(aabbs.size()), sort_boxes, r_max_u, r_max_v);

  *r_max_u *= target_aspect_y;

  /* Write back box_pack UVs. */
  for (const int64_t i : aabbs.index_range()) {
    PackIsland *island = islands[aabbs[i]->index];
    BoxPack *box = box_array + i;
    uv_phi phi;
    phi.rotation = 0.0f; /* #BLI_box_pack_2d never rotates. */
    phi.translation.x = (box->x + box->w * 0.5f) * target_aspect_y;
    phi.translation.y = (box->y + box->h * 0.5f);
    island->place_(scale, phi);
  }

  /* Housekeeping. */
  MEM_freeN(box_array);
}

/**
 * Helper class for the `xatlas` strategy.
 * Accelerates geometry queries by approximating exact queries with a bitmap.
 * Includes some book keeping variables to simplify the algorithm.
 */
class Occupancy {
 public:
  Occupancy(const float initial_scale);

  void increase_scale(); /* Resize the scale of the bitmap and clear it. */

  /* Write or Query a triangle on the bitmap. */
  float trace_triangle(const float2 &uv0,
                       const float2 &uv1,
                       const float2 &uv2,
                       const float margin,
                       const bool write) const;

  /* Write or Query an island on the bitmap. */
  float trace_island(const PackIsland *island,
                     const uv_phi phi,
                     const float scale,
                     const float margin,
                     const bool write) const;

  int bitmap_radix;              /* Width and Height of `bitmap`. */
  float bitmap_scale_reciprocal; /* == 1.0f / `bitmap_scale`. */
 private:
  mutable blender::Array<float> bitmap_;

  mutable float2 witness_;         /* Witness to a previously known occupied pixel. */
  mutable float witness_distance_; /* Signed distance to nearest placed island. */
  mutable uint triangle_hint_;     /* Hint to a previously suspected overlapping triangle. */

  const float terminal = 1048576.0f; /* 4 * bitmap_radix < terminal < INT_MAX / 4. */
};

Occupancy::Occupancy(const float initial_scale)
    : bitmap_radix(800), bitmap_(bitmap_radix * bitmap_radix, false)
{
  bitmap_scale_reciprocal = 1.0f; /* lint, prevent uninitialized memory access. */
  increase_scale();
  bitmap_scale_reciprocal = bitmap_radix / initial_scale; /* Actually set the value. */
}

void Occupancy::increase_scale()
{
  BLI_assert(bitmap_scale_reciprocal > 0.0f); /* TODO: Packing has failed, report error. */

  bitmap_scale_reciprocal *= 0.5f;
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
  result_ssq = std::min(result_ssq, blender::math::length_squared(probe - uv0));
  result_ssq = std::min(result_ssq, blender::math::length_squared(probe - uv1));
  result_ssq = std::min(result_ssq, blender::math::length_squared(probe - uv2));
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

float2 PackIsland::get_diagonal_support_d4(const float scale,
                                           const float rotation,
                                           const float margin) const
{
  if (rotation == 0.0f) {
    return half_diagonal_ * scale + margin; /* Fast path for common case. */
  }

  if (rotation == DEG2RADF(180.0f)) {
    return get_diagonal_support_d4(scale, 0.0f, margin); /* Same as 0.0f */
  }

  /* TODO: BLI_assert rotation is a "Dihedral Group D4" transform. */
  float matrix[2][2];
  build_transformation(scale, rotation, matrix);

  float diagonal_rotated[2];
  mul_v2_m2v2(diagonal_rotated, matrix, half_diagonal_);
  return float2(fabsf(diagonal_rotated[0]) + margin, fabsf(diagonal_rotated[1]) + margin);
}

float2 PackIsland::get_diagonal_support(const float scale,
                                        const float rotation,
                                        const float margin) const
{
  /* Only "D4" transforms are currently supported. */
  return get_diagonal_support_d4(scale, rotation, margin);
}

float Occupancy::trace_island(const PackIsland *island,
                              const uv_phi phi,
                              const float scale,
                              const float margin,
                              const bool write) const
{
  float2 diagonal_support = island->get_diagonal_support(scale, phi.rotation, margin);

  if (!write) {
    if (phi.translation.x < diagonal_support.x || phi.translation.y < diagonal_support.y) {
      return terminal; /* Occupied. */
    }
  }
  float matrix[2][2];
  island->build_transformation(scale, phi.rotation, matrix);
  float2 pivot_transformed;
  mul_v2_m2v2(pivot_transformed, matrix, island->pivot_);

  float2 delta = phi.translation - pivot_transformed;
  uint vert_count = uint(island->triangle_vertices_.size()); /* `uint` is faster than `int`. */
  for (uint i = 0; i < vert_count; i += 3) {
    uint j = (i + triangle_hint_) % vert_count;
    float2 uv0;
    float2 uv1;
    float2 uv2;
    mul_v2_m2v2(uv0, matrix, island->triangle_vertices_[j]);
    mul_v2_m2v2(uv1, matrix, island->triangle_vertices_[j + 1]);
    mul_v2_m2v2(uv2, matrix, island->triangle_vertices_[j + 2]);
    float extent = trace_triangle(uv0 + delta, uv1 + delta, uv2 + delta, margin, write);

    if (!write && extent >= 0.0f) {
      triangle_hint_ = j;
      return extent; /* Occupied. */
    }
  }
  return -1.0f; /* Available. */
}

static uv_phi find_best_fit_for_island(const PackIsland *island,
                                       const int scan_line,
                                       Occupancy &occupancy,
                                       const float scale,
                                       const int angle_90_multiple,
                                       const float margin,
                                       const float target_aspect_y)
{
  const float bitmap_scale = 1.0f / occupancy.bitmap_scale_reciprocal;

  const float sqrt_target_aspect_y = sqrtf(target_aspect_y);
  const int scan_line_x = int(scan_line * sqrt_target_aspect_y);
  const int scan_line_y = int(scan_line / sqrt_target_aspect_y);

  uv_phi phi;
  phi.rotation = DEG2RADF(angle_90_multiple * 90);
  float matrix[2][2];
  island->build_transformation(scale, phi.rotation, matrix);

  /* Caution, margin is zero for support_diagonal as we're tracking the top-right corner. */
  float2 support_diagonal = island->get_diagonal_support_d4(scale, phi.rotation, 0.0f);

  /* Scan using an "Alpaca"-style search, first horizontally using "less-than". */
  int t = int(ceilf((2 * support_diagonal.x + margin) * occupancy.bitmap_scale_reciprocal));
  while (t < scan_line_x) {
    phi.translation = float2(t * bitmap_scale, scan_line_y * bitmap_scale) - support_diagonal;
    const float extent = occupancy.trace_island(island, phi, scale, margin, false);
    if (extent < 0.0f) {
      return phi; /* Success. */
    }
    t = t + std::max(1, int(extent));
  }

  /* Then scan vertically using "less-than-or-equal" */
  t = int(ceilf((2 * support_diagonal.y + margin) * occupancy.bitmap_scale_reciprocal));
  while (t <= scan_line_y) {
    phi.translation = float2(scan_line_x * bitmap_scale, t * bitmap_scale) - support_diagonal;
    const float extent = occupancy.trace_island(island, phi, scale, margin, false);
    if (extent < 0.0f) {
      return phi; /* Success. */
    }
    t = t + std::max(1, int(extent));
  }

  return uv_phi(); /* Unable to find a place to fit. */
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
 * Performance would normally be `O(n^4)`, however the occupancy
 * bitmap_radix is fixed, which gives a reduced time complexity of `O(n^3)`.
 */
static void pack_island_xatlas(const Span<UVAABBIsland *> island_indices,
                               const Span<PackIsland *> islands,
                               const float scale,
                               const float margin,
                               const UVPackIsland_Params &params,
                               float *r_max_u,
                               float *r_max_v)
{
  Occupancy occupancy(guess_initial_scale(islands, scale, margin));
  float max_u = 0.0f;
  float max_v = 0.0f;

  blender::Array<uv_phi> phis(island_indices.size());
  int scan_line = 0;
  int i = 0;

  /* The following `while` loop is setting up a three-way race:
   * `for (scan_line = 0; scan_line < bitmap_radix; scan_line++)`
   * `for (i : island_indices.index_range())`
   * `while (bitmap_scale_reciprocal > 0) { bitmap_scale_reciprocal *= 0.5f; }`
   */

  while (i < island_indices.size()) {
    PackIsland *island = islands[island_indices[i]->index];
    uv_phi phi;

    int max_90_multiple = params.rotate && (i < 50) ? 4 : 1;
    for (int angle_90_multiple = 0; angle_90_multiple < max_90_multiple; angle_90_multiple++) {
      phi = find_best_fit_for_island(
          island, scan_line, occupancy, scale, angle_90_multiple, margin, params.target_aspect_y);
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
                          sqrtf(std::min(params.target_aspect_y, 1.0f / params.target_aspect_y))) {
        continue; /* Try again on next scan_line. */
      }

      /* Enlarge search parameters. */
      scan_line = 0;
      occupancy.increase_scale();

      /* Redraw already placed islands. (Greedy.) */
      for (int j = 0; j < i; j++) {
        occupancy.trace_island(islands[island_indices[j]->index], phis[j], scale, margin, true);
      }
      continue;
    }

    /* Place island. */
    phis[i] = phi;
    island->place_(scale, phi);
    occupancy.trace_island(island, phi, scale, margin, true);
    i++; /* Next island. */

    /* Update top-right corner. */
    float2 top_right = island->get_diagonal_support(scale, phi.rotation, margin) + phi.translation;
    max_u = std::max(top_right.x, max_u);
    max_v = std::max(top_right.y, max_v);

    /* Heuristics to reduce size of brute-force search. */
    if (i < 128 || (i & 31) == 16) {
      scan_line = 0; /* Restart completely. */
    }
    else {
      scan_line = std::max(0, scan_line - 25); /* `-25` must by odd. */
    }
  }

  *r_max_u = max_u;
  *r_max_v = max_v;
}

/**
 * Pack islands using a mix of other strategies.
 * \param islands: The islands to be packed. Will be modified with results.
 * \param scale: Scale islands by `scale` before packing.
 * \param margin: Add `margin` units around islands before packing.
 * \param params: Additional parameters. Scale and margin information is ignored.
 * \return Size of square covering the resulting packed UVs. The maximum `u` or `v` co-ordinate.
 */
static float pack_islands_scale_margin(const Span<PackIsland *> islands,
                                       const float scale,
                                       const float margin,
                                       const UVPackIsland_Params &params)
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
   * - Call #BLI_box_pack_2d on the first `alpaca_cutoff` islands.
   * - Call #pack_islands_alpaca_* on the remaining islands.
   * - Combine results.
   */

  /* First, copy information from our input into the AABB structure. */
  Array<UVAABBIsland *> aabbs(islands.size());
  for (const int64_t i : islands.index_range()) {
    PackIsland *pack_island = islands[i];
    UVAABBIsland *aabb = new UVAABBIsland();
    aabb->index = i;
    aabb->uv_diagonal.x = pack_island->half_diagonal_.x * 2 * scale + 2 * margin;
    aabb->uv_diagonal.y = pack_island->half_diagonal_.y * 2 * scale + 2 * margin;
    aabb->aspect_y = pack_island->aspect_y;
    aabbs[i] = aabb;
  }

  /* Sort from "biggest" to "smallest". */

  if (params.rotate) {
    std::stable_sort(aabbs.begin(), aabbs.end(), [](const UVAABBIsland *a, const UVAABBIsland *b) {
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

    std::stable_sort(aabbs.begin(), aabbs.end(), [](const UVAABBIsland *a, const UVAABBIsland *b) {
      /* Choose the AABB with larger rectangular area. */
      return b->uv_diagonal.x * b->uv_diagonal.y < a->uv_diagonal.x * a->uv_diagonal.y;
    });
  }

  /* Partition `islands`, largest will go to a slow packer, the rest alpaca_turbo.
   * See discussion above for details. */
  int64_t alpaca_cutoff = 1024;    /* Regular situation, pack 1024 islands with slow packer. */
  int64_t alpaca_cutoff_fast = 80; /* Reduced problem size, only 80 islands with slow packer. */
  if (params.margin_method == ED_UVPACK_MARGIN_FRACTION) {
    if (margin > 0.0f) {
      alpaca_cutoff = alpaca_cutoff_fast;
    }
  }
  const int64_t max_box_pack = std::min(alpaca_cutoff, islands.size());

  /* Call box_pack_2d (slow for large N.) */
  float max_u = 0.0f;
  float max_v = 0.0f;
  switch (params.shape_method) {
    case ED_UVPACK_SHAPE_CONVEX:
    case ED_UVPACK_SHAPE_CONCAVE:
      pack_island_xatlas(aabbs.as_span().take_front(max_box_pack),
                         islands,
                         scale,
                         margin,
                         params,
                         &max_u,
                         &max_v);
      break;
    default:
      pack_island_box_pack_2d(aabbs.as_span().take_front(max_box_pack),
                              islands,
                              scale,
                              margin,
                              params.target_aspect_y,
                              &max_u,
                              &max_v);
      break;
  }

  /* At this stage, `max_u` and `max_v` contain the box_pack/xatlas UVs. */

  /* Call Alpaca. */
  if (params.rotate) {
    pack_islands_alpaca_rotate(
        aabbs.as_mutable_span().drop_front(max_box_pack), params.target_aspect_y, &max_u, &max_v);
  }
  else {
    pack_islands_alpaca_turbo(
        aabbs.as_mutable_span().drop_front(max_box_pack), params.target_aspect_y, &max_u, &max_v);
  }

  /* Write back Alpaca UVs. */
  for (int64_t i = max_box_pack; i < aabbs.size(); i++) {
    UVAABBIsland *aabb = aabbs[i];
    PackIsland *pack_island = islands[aabb->index];
    pack_island->angle = aabb->angle;
    float matrix_inverse[2][2];
    pack_island->build_inverse_transformation(scale, pack_island->angle, matrix_inverse);
    mul_v2_m2v2(pack_island->pre_translate, matrix_inverse, aabb->uv_placement);
    pack_island->pre_translate -= pack_island->pivot_;
  }

  /* Memory management. */
  for (int64_t i : aabbs.index_range()) {
    UVAABBIsland *aabb = aabbs[i];
    aabbs[i] = nullptr;
    delete aabb;
  }

  return std::max(max_u / params.target_aspect_y, max_v);
}

/** Find the optimal scale to pack islands into the unit square.
 * returns largest scale that will pack `islands` into the unit square.
 */
static float pack_islands_margin_fraction(const Span<PackIsland *> &island_vector,
                                          const float margin_fraction,
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
  float scale_last = 0.0f;

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
    }

    scale = std::max(scale, min_scale_roundoff);

    /* Evaluate our `f`. */
    scale_last = scale;
    const float max_uv = pack_islands_scale_margin(
        island_vector, scale_last, margin_fraction, params);
    const float value = sqrtf(max_uv) - 1.0f;

    if (value <= 0.0f) {
      scale_low = scale;
      value_low = value;
    }
    else {
      scale_high = scale;
      value_high = value;
      if (scale == min_scale_roundoff) {
        /* Unable to pack without damaging UVs. */
        scale_low = scale;
        break;
      }
    }
  }

  const bool flush = true;
  if (flush) {
    /* Write back best pack as a side-effect. */
    if (scale_last != scale_low) {
      scale_last = scale_low;
      const float max_uv = pack_islands_scale_margin(
          island_vector, scale_last, margin_fraction, params);
      BLI_assert(max_uv == value_low);
      UNUSED_VARS(max_uv);
      /* TODO (?): `if (max_uv < 1.0f) { scale_last /= max_uv; }` */
    }
  }
  return scale_last;
}

static float calc_margin_from_aabb_length_sum(const Span<PackIsland *> &island_vector,
                                              const UVPackIsland_Params &params)
{
  /* Logic matches behavior from #geometry::uv_parametrizer_pack.
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

void pack_islands(const Span<PackIsland *> &islands,
                  const UVPackIsland_Params &params,
                  float r_scale[2])
{
  if (params.margin == 0.0f) {
    /* Special case for zero margin. Margin_method is ignored as all formulas give same result. */
    const float max_uv = pack_islands_scale_margin(islands, 1.0f, 0.0f, params);
    r_scale[0] = 1.0f / max_uv;
    r_scale[1] = r_scale[0];
    return;
  }

  if (params.margin_method == ED_UVPACK_MARGIN_FRACTION) {
    /* Uses a line search on scale. ~10x slower than other method. */
    const float scale = pack_islands_margin_fraction(islands, params.margin, params);
    r_scale[0] = scale;
    r_scale[1] = scale;
    return;
  }

  float margin = params.margin;
  switch (params.margin_method) {
    case ED_UVPACK_MARGIN_ADD:    /* Default for Blender 2.8 and earlier. */
      break;                      /* Nothing to do. */
    case ED_UVPACK_MARGIN_SCALED: /* Default for Blender 3.3 and later. */
      margin = calc_margin_from_aabb_length_sum(islands, params);
      break;
    case ED_UVPACK_MARGIN_FRACTION: /* Added as an option in Blender 3.4. */
      BLI_assert_unreachable();     /* Handled above. */
      break;
    default:
      BLI_assert_unreachable();
  }

  const float max_uv = pack_islands_scale_margin(islands, 1.0f, margin, params);
  r_scale[0] = 1.0f / max_uv;
  r_scale[1] = r_scale[0];
}

/** \} */

void PackIsland::build_transformation(const float scale,
                                      const float angle,
                                      float (*r_matrix)[2]) const
{
  const float cos_angle = cosf(angle);
  const float sin_angle = sinf(angle);
  r_matrix[0][0] = cos_angle * scale;
  r_matrix[0][1] = -sin_angle * scale * aspect_y;
  r_matrix[1][0] = sin_angle * scale / aspect_y;
  r_matrix[1][1] = cos_angle * scale;
  /*
  if (reflect) {
    r_matrix[0][0] *= -1.0f;
    r_matrix[0][1] *= -1.0f;
  }
  */
}

void PackIsland::build_inverse_transformation(const float scale,
                                              const float angle,
                                              float (*r_matrix)[2]) const
{
  const float cos_angle = cosf(angle);
  const float sin_angle = sinf(angle);

  r_matrix[0][0] = cos_angle / scale;
  r_matrix[0][1] = sin_angle / scale * aspect_y;
  r_matrix[1][0] = -sin_angle / scale / aspect_y;
  r_matrix[1][1] = cos_angle / scale;
  /*
  if (reflect) {
    r_matrix[0][0] *= -1.0f;
    r_matrix[1][0] *= -1.0f;
  }
  */
}

}  // namespace blender::geometry
