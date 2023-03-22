/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include "GEO_uv_pack.hh"

#include "BLI_array.hh"
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
  float(*source)[2] = static_cast<float(*)[2]>(
      BLI_memarena_alloc(arena, sizeof(*source) * size_t(vert_count)));

  /* Copy input. */
  for (int i = 0; i < vert_count; i++) {
    copy_v2_v2(source[i], uvs[i]);
  }

  /* Triangulate. */
  BLI_polyfill_calc_arena(source, vert_count, 0, tris, arena);

  /* Beautify improves performance of packer. (Optional)
   * Long thin triangles, especially at 45 degree angles,
   * can trigger worst-case performance in #trace_triangle.
   * Using `Beautify` brings more inputs into average-case.
   */
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
      return; /* Trivial case, nothing to do. */
    }

    int vert_count = int(triangle_vertices_.size());

    /* Allocate storage. */
    int *index_map = static_cast<int *>(
        BLI_memarena_alloc(arena, sizeof(*index_map) * vert_count));

    /* Prepare input for convex hull. */
    float(*source)[2] = reinterpret_cast<float(*)[2]>(triangle_vertices_.data());

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
}

UVPackIsland_Params::UVPackIsland_Params()
{
  /* TEMPORARY, set every thing to "zero" for backwards compatibility. */
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
  shape_method = ED_UVPACK_SHAPE_AABB;
}

/* Compact representation for AABB packers. */
class UVAABBIsland {
 public:
  float2 uv_diagonal;
  float2 uv_placement;
  int64_t index;
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
                                      float *r_max_u,
                                      float *r_max_v)
{
  /* Exclude an initial AABB near the origin. */
  float next_u1 = *r_max_u;
  float next_v1 = *r_max_v;
  bool zigzag = next_u1 < next_v1; /* Horizontal or Vertical strip? */

  float u0 = zigzag ? next_u1 : 0.0f;
  float v0 = zigzag ? 0.0f : next_v1;

  /* Visit every island in order. */
  for (UVAABBIsland *island : islands) {
    float dsm_u = island->uv_diagonal.x;
    float dsm_v = island->uv_diagonal.y;

    bool restart = false;
    if (zigzag) {
      restart = (next_v1 < v0 + dsm_v);
    }
    else {
      restart = (next_u1 < u0 + dsm_u);
    }
    if (restart) {
      /* We're at the end of a strip. Restart from U axis or V axis. */
      zigzag = next_u1 < next_v1;
      u0 = zigzag ? next_u1 : 0.0f;
      v0 = zigzag ? 0.0f : next_v1;
    }

    /* Place the island. */
    island->uv_placement.x = u0;
    island->uv_placement.y = v0;
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
  float trace_island(PackIsland *island,
                     const float scale,
                     const float margin,
                     const float2 &uv,
                     const bool write) const;

  int bitmap_radix;              /* Width and Height of `bitmap`. */
  float bitmap_scale_reciprocal; /* == 1.0f / `bitmap_scale`. */
 private:
  mutable blender::Array<float> bitmap_;

  mutable float2 witness_;         /* Witness to a previously known occupied pixel. */
  mutable float witness_distance_; /* Signed distance to nearest placed island. */
  mutable uint triangle_hint_;     /* Hint to a previously suspected overlapping triangle. */

  const float terminal = 1048576.0f; /* A "very" large number, much bigger than 4 * bitmap_radix */
};

Occupancy::Occupancy(const float initial_scale)
    : bitmap_radix(800), bitmap_(bitmap_radix * bitmap_radix, false)
{
  increase_scale();
  bitmap_scale_reciprocal = bitmap_radix / initial_scale;
}

void Occupancy::increase_scale()
{
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
        if (extent > 0.0f) {
          return extent; /* Witness observes occupied. */
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

float Occupancy::trace_island(PackIsland *island,
                              const float scale,
                              const float margin,
                              const float2 &uv,
                              const bool write) const
{

  if (!write) {
    if (uv.x <= 0.0f || uv.y <= 0.0f) {
      return std::max(-uv.x, -uv.y); /* Occupied. */
    }
  }
  const float2 origin(island->bounds_rect.xmin, island->bounds_rect.ymin);
  const float2 delta = uv - origin * scale;
  uint vert_count = uint(island->triangle_vertices_.size());
  for (uint i = 0; i < vert_count; i += 3) {
    uint j = (i + triangle_hint_) % vert_count;
    float extent = trace_triangle(delta + island->triangle_vertices_[j] * scale,
                                  delta + island->triangle_vertices_[j + 1] * scale,
                                  delta + island->triangle_vertices_[j + 2] * scale,
                                  margin,
                                  write);

    if (!write && extent >= 0.0f) {
      triangle_hint_ = j;
      return extent; /* Occupied. */
    }
  }
  return -1.0f; /* Available. */
}

static float2 find_best_fit_for_island(
    PackIsland *island, int scan_line, Occupancy &occupancy, const float scale, const float margin)
{
  const float scan_line_bscaled = scan_line / occupancy.bitmap_scale_reciprocal;
  const float size_x_scaled = BLI_rctf_size_x(&island->bounds_rect) * scale;
  const float size_y_scaled = BLI_rctf_size_y(&island->bounds_rect) * scale;

  int t = 0;
  while (t <= scan_line) {
    const float t_bscaled = t / occupancy.bitmap_scale_reciprocal;

    /* Scan using an "Alpaca"-style search, both horizontally and vertically at the same time. */

    const float2 horiz(scan_line_bscaled - size_x_scaled, t_bscaled - size_y_scaled);
    const float extentH = occupancy.trace_island(island, scale, margin, horiz, false);
    if (extentH < 0.0f) {
      return horiz;
    }

    const float2 vert(t_bscaled - size_x_scaled, scan_line_bscaled - size_y_scaled);
    const float extentV = occupancy.trace_island(island, scale, margin, vert, false);
    if (extentV < 0.0f) {
      return vert;
    }

    const float min_extent = std::min(extentH, extentV);

    t = t + std::max(1, int(min_extent));
  }
  return float2(-1, -1);
}

static float guess_initial_scale(const Span<PackIsland *> islands,
                                 const float scale,
                                 const float margin)
{
  float sum = 1e-40f;
  for (int64_t i : islands.index_range()) {
    PackIsland *island = islands[i];
    sum += BLI_rctf_size_x(&island->bounds_rect) * scale + 2 * margin;
    sum += BLI_rctf_size_y(&island->bounds_rect) * scale + 2 * margin;
  }
  return sqrtf(sum) / 3.0f;
}

/**
 * Pack irregular islands using the `xatlas` strategy, with no rotation.
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
                               BoxPack *box_array,
                               const float scale,
                               const float margin,
                               float *r_max_u,
                               float *r_max_v)
{
  Occupancy occupancy(guess_initial_scale(islands, scale, margin));
  float max_u = 0.0f;
  float max_v = 0.0f;

  int scan_line = 0;
  int i = 0;

  /* The following `while` loop is setting up a three-way race:
   * `for (scan_line = 0; scan_line < bitmap_radix; scan_line++)`
   * `for (i : island_indices.index_range())`
   * `while (bitmap_scale_reciprocal > 0) { bitmap_scale_reciprocal *= 0.5f; }`
   */

  while (i < island_indices.size()) {
    PackIsland *island = islands[island_indices[i]->index];
    const float2 best = find_best_fit_for_island(island, scan_line, occupancy, scale, margin);

    if (best.x <= -1.0f) {
      /* Unable to find a fit on this scan_line. */

      if (i < 10) {
        scan_line++;
      }
      else {
        /* Increasing by 2 here has the effect of changing the sampling pattern.
         * The parameter '2' is not "free" in the sense that changing it requires
         * a change to `bitmap_radix` and then returning `alpaca_cutoff`.
         * Possible values here *could* be 1, 2 or 3, however the only *reasonable*
         * choice is 2. */
        scan_line += 2;
      }
      if (scan_line < occupancy.bitmap_radix) {
        continue; /* Try again on next scan_line. */
      }

      /* Enlarge search parameters. */
      scan_line = 0;
      occupancy.increase_scale();

      /* Redraw already placed islands. (Greedy.) */
      for (int j = 0; j < i; j++) {
        BoxPack *box = box_array + j;
        occupancy.trace_island(
            islands[island_indices[j]->index], scale, margin, float2(box->x, box->y), true);
      }
      continue;
    }

    /* Place island. */
    BoxPack *box = box_array + i;
    box->x = best.x;
    box->y = best.y;
    max_u = std::max(box->x + BLI_rctf_size_x(&island->bounds_rect) * scale + 2 * margin, max_u);
    max_v = std::max(box->y + BLI_rctf_size_y(&island->bounds_rect) * scale + 2 * margin, max_v);
    occupancy.trace_island(island, scale, margin, float2(box->x, box->y), true);
    i++; /* Next island. */

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

static float pack_islands_scale_margin(const Span<PackIsland *> islands,
                                       BoxPack *box_array,
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
   * - Call #pack_islands_alpaca_turbo on the remaining islands.
   * - Combine results.
   */

  /* First, copy information from our input into the AABB structure. */
  Array<UVAABBIsland *> aabbs(islands.size());
  for (const int64_t i : islands.index_range()) {
    PackIsland *pack_island = islands[i];
    UVAABBIsland *aabb = new UVAABBIsland();
    aabb->index = i;
    aabb->uv_diagonal.x = BLI_rctf_size_x(&pack_island->bounds_rect) * scale + 2 * margin;
    aabb->uv_diagonal.y = BLI_rctf_size_y(&pack_island->bounds_rect) * scale + 2 * margin;
    aabbs[i] = aabb;
  }

  /* Sort from "biggest" to "smallest". */
  std::stable_sort(aabbs.begin(), aabbs.end(), [](const UVAABBIsland *a, const UVAABBIsland *b) {
    /* Just choose the AABB with larger rectangular area. */
    return b->uv_diagonal.x * b->uv_diagonal.y < a->uv_diagonal.x * a->uv_diagonal.y;
  });

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

  /* Prepare for box_pack_2d. */
  for (const int64_t i : islands.index_range()) {
    UVAABBIsland *aabb = aabbs[i];
    BoxPack *box = &box_array[i];
    box->index = int(aabb->index);
    box->w = aabb->uv_diagonal.x;
    box->h = aabb->uv_diagonal.y;
  }

  /* Call box_pack_2d (slow for large N.) */
  float max_u = 0.0f;
  float max_v = 0.0f;
  switch (params.shape_method) {
    case ED_UVPACK_SHAPE_CONVEX:
    case ED_UVPACK_SHAPE_CONCAVE:
      pack_island_xatlas(aabbs.as_span().take_front(max_box_pack),
                         islands,
                         box_array,
                         scale,
                         margin,
                         &max_u,
                         &max_v);
      break;
    default:
      BLI_box_pack_2d(box_array, int(max_box_pack), &max_u, &max_v);
      break;
  }

  /* At this stage, `max_u` and `max_v` contain the box_pack UVs. */

  /* Call Alpaca. */
  pack_islands_alpaca_turbo(aabbs.as_span().drop_front(max_box_pack), &max_u, &max_v);

  /* Write back Alpaca UVs. */
  for (int64_t index = max_box_pack; index < aabbs.size(); index++) {
    UVAABBIsland *aabb = aabbs[index];
    BoxPack *box = &box_array[index];
    box->x = aabb->uv_placement.x;
    box->y = aabb->uv_placement.y;
  }

  /* Memory management. */
  for (int64_t i : aabbs.index_range()) {
    UVAABBIsland *aabb = aabbs[i];
    aabbs[i] = nullptr;
    delete aabb;
  }

  return std::max(max_u, max_v);
}

static float pack_islands_margin_fraction(const Span<PackIsland *> &island_vector,
                                          BoxPack *box_array,
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
        island_vector, box_array, scale_last, margin_fraction, params);
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
    /* Write back best pack as a side-effect. First get best pack. */
    if (scale_last != scale_low) {
      scale_last = scale_low;
      const float max_uv = pack_islands_scale_margin(
          island_vector, box_array, scale_last, margin_fraction, params);
      UNUSED_VARS(max_uv);
      /* TODO (?): `if (max_uv < 1.0f) { scale_last /= max_uv; }` */
    }

    /* Then expand FaceIslands by the correct amount. */
    for (const int64_t index : island_vector.index_range()) {
      BoxPack *box = &box_array[index];
      box->x /= scale_last;
      box->y /= scale_last;
      PackIsland *island = island_vector[index];
      BLI_rctf_pad(
          &island->bounds_rect, margin_fraction / scale_last, margin_fraction / scale_last);
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
    float w = BLI_rctf_size_x(&island->bounds_rect);
    float h = BLI_rctf_size_y(&island->bounds_rect);
    aabb_length_sum += sqrtf(w * h);
  }
  return params.margin * aabb_length_sum * 0.1f;
}

static BoxPack *pack_islands_box_array(const Span<PackIsland *> &island_vector,
                                       const UVPackIsland_Params &params,
                                       float r_scale[2])
{
  BoxPack *box_array = static_cast<BoxPack *>(
      MEM_mallocN(sizeof(*box_array) * island_vector.size(), __func__));

  if (params.margin == 0.0f) {
    /* Special case for zero margin. Margin_method is ignored as all formulas give same result. */
    const float max_uv = pack_islands_scale_margin(island_vector, box_array, 1.0f, 0.0f, params);
    r_scale[0] = 1.0f / max_uv;
    r_scale[1] = r_scale[0];
    return box_array;
  }

  if (params.margin_method == ED_UVPACK_MARGIN_FRACTION) {
    /* Uses a line search on scale. ~10x slower than other method. */
    const float scale = pack_islands_margin_fraction(
        island_vector, box_array, params.margin, params);
    r_scale[0] = scale;
    r_scale[1] = scale;
    /* pack_islands_margin_fraction will pad FaceIslands, return early. */
    return box_array;
  }

  float margin = params.margin;
  switch (params.margin_method) {
    case ED_UVPACK_MARGIN_ADD:    /* Default for Blender 2.8 and earlier. */
      break;                      /* Nothing to do. */
    case ED_UVPACK_MARGIN_SCALED: /* Default for Blender 3.3 and later. */
      margin = calc_margin_from_aabb_length_sum(island_vector, params);
      break;
    case ED_UVPACK_MARGIN_FRACTION: /* Added as an option in Blender 3.4. */
      BLI_assert_unreachable();     /* Handled above. */
      break;
    default:
      BLI_assert_unreachable();
  }

  const float max_uv = pack_islands_scale_margin(island_vector, box_array, 1.0f, margin, params);
  r_scale[0] = 1.0f / max_uv;
  r_scale[1] = r_scale[0];

  for (int index = 0; index < island_vector.size(); index++) {
    PackIsland *island = island_vector[index];
    BLI_rctf_pad(&island->bounds_rect, margin, margin);
  }
  return box_array;
}

void pack_islands(const Span<PackIsland *> &islands,
                  const UVPackIsland_Params &params,
                  float r_scale[2])
{
  BoxPack *box_array = pack_islands_box_array(islands, params, r_scale);

  for (int64_t i : islands.index_range()) {
    BoxPack *box = box_array + i;
    PackIsland *island = islands[box->index];
    island->pre_translate.x = box->x - island->bounds_rect.xmin;
    island->pre_translate.y = box->y - island->bounds_rect.ymin;
  }

  MEM_freeN(box_array);
}

}  // namespace blender::geometry
