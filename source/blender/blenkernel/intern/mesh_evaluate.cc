/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bke
 *
 * Functions to evaluate mesh data.
 */

#include <climits>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_alloca.h"
#include "BLI_bitmap.h"
#include "BLI_edgehash.h"
#include "BLI_index_range.hh"
#include "BLI_math.h"
#include "BLI_span.hh"
#include "BLI_utildefines.h"
#include "BLI_virtual_array.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_mesh.hh"
#include "BKE_multires.h"

using blender::float3;
using blender::MutableSpan;
using blender::Span;
using blender::VArray;

/* -------------------------------------------------------------------- */
/** \name Polygon Calculations
 * \{ */

namespace blender::bke::mesh {

static float3 poly_center_calc_ngon(const Span<float3> vert_positions, const Span<int> poly_verts)
{
  const float w = 1.0f / float(poly_verts.size());

  float3 center(0);
  for (const int i : poly_verts.index_range()) {
    center += vert_positions[poly_verts[i]] * w;
  }
  return center;
}

float3 poly_center_calc(const Span<float3> vert_positions, const Span<int> poly_verts)
{
  if (poly_verts.size() == 3) {
    float3 center;
    mid_v3_v3v3v3(center,
                  vert_positions[poly_verts[0]],
                  vert_positions[poly_verts[1]],
                  vert_positions[poly_verts[2]]);
    return center;
  }
  if (poly_verts.size() == 4) {
    float3 center;
    mid_v3_v3v3v3v3(center,
                    vert_positions[poly_verts[0]],
                    vert_positions[poly_verts[1]],
                    vert_positions[poly_verts[2]],
                    vert_positions[poly_verts[3]]);
    return center;
  }
  return poly_center_calc_ngon(vert_positions, poly_verts);
}

}  // namespace blender::bke::mesh

void BKE_mesh_calc_poly_center(const int *poly_verts,
                               const int poly_size,
                               const float (*vert_positions)[3],
                               const int verts_num,
                               float r_cent[3])
{
  copy_v3_v3(r_cent,
             blender::bke::mesh::poly_center_calc(
                 {reinterpret_cast<const blender::float3 *>(vert_positions), verts_num},
                 {poly_verts, poly_size}));
}

namespace blender::bke::mesh {

float poly_area_calc(const Span<float3> vert_positions, const Span<int> poly_verts)
{
  if (poly_verts.size() == 3) {
    return area_tri_v3(vert_positions[poly_verts[0]],
                       vert_positions[poly_verts[1]],
                       vert_positions[poly_verts[2]]);
  }
  Array<float3, 32> poly_coords(poly_verts.size());
  for (const int i : poly_verts.index_range()) {
    poly_coords[i] = vert_positions[poly_verts[i]];
  }
  return area_poly_v3((const float(*)[3])poly_coords.data(), poly_verts.size());
}

}  // namespace blender::bke::mesh

float BKE_mesh_calc_poly_area(const int *poly_verts,
                              const int poly_size,
                              const float (*vert_positions)[3],
                              const int verts_num)
{
  return blender::bke::mesh::poly_area_calc(
      {reinterpret_cast<const float3 *>(vert_positions), verts_num}, {poly_verts, poly_size});
}

float BKE_mesh_calc_area(const Mesh *me)
{
  const Span<float3> positions = me->vert_positions();
  const blender::OffsetIndices polys = me->polys();
  const Span<int> corner_verts = me->corner_verts();

  float total_area = 0.0f;
  for (const int i : polys.index_range()) {
    total_area += blender::bke::mesh::poly_area_calc(positions, corner_verts.slice(polys[i]));
  }
  return total_area;
}

static float UNUSED_FUNCTION(mesh_calc_poly_volume_centroid)(const int *poly_verts,
                                                             const int poly_size,
                                                             const float (*positions)[3],
                                                             float r_cent[3])
{
  const float *v_pivot, *v_step1;
  float total_volume = 0.0f;

  zero_v3(r_cent);

  v_pivot = positions[poly_verts[0]];
  v_step1 = positions[poly_verts[1]];

  for (int i = 2; i < poly_size; i++) {
    const float *v_step2 = positions[poly_verts[i]];

    /* Calculate the 6x volume of the tetrahedron formed by the 3 vertices
     * of the triangle and the origin as the fourth vertex */
    const float tetra_volume = volume_tri_tetrahedron_signed_v3_6x(v_pivot, v_step1, v_step2);
    total_volume += tetra_volume;

    /* Calculate the centroid of the tetrahedron formed by the 3 vertices
     * of the triangle and the origin as the fourth vertex.
     * The centroid is simply the average of the 4 vertices.
     *
     * Note that the vector is 4x the actual centroid
     * so the division can be done once at the end. */
    for (uint j = 0; j < 3; j++) {
      r_cent[j] += tetra_volume * (v_pivot[j] + v_step1[j] + v_step2[j]);
    }

    v_step1 = v_step2;
  }

  return total_volume;
}

namespace blender::bke::mesh {

/**
 * A version of mesh_calc_poly_volume_centroid that takes an initial reference center,
 * use this to increase numeric stability as the quality of the result becomes
 * very low quality as the value moves away from 0.0, see: #65986.
 */
static float mesh_calc_poly_volume_centroid_with_reference_center(const Span<float3> positions,
                                                                  const Span<int> poly_verts,
                                                                  const float3 &reference_center,
                                                                  float r_cent[3])
{
  /* See: mesh_calc_poly_volume_centroid for comments. */
  float v_pivot[3], v_step1[3];
  float total_volume = 0.0f;
  zero_v3(r_cent);
  sub_v3_v3v3(v_pivot, positions[poly_verts[0]], reference_center);
  sub_v3_v3v3(v_step1, positions[poly_verts[1]], reference_center);
  for (int i = 2; i < poly_verts.size(); i++) {
    float v_step2[3];
    sub_v3_v3v3(v_step2, positions[poly_verts[i]], reference_center);
    const float tetra_volume = volume_tri_tetrahedron_signed_v3_6x(v_pivot, v_step1, v_step2);
    total_volume += tetra_volume;
    for (uint j = 0; j < 3; j++) {
      r_cent[j] += tetra_volume * (v_pivot[j] + v_step1[j] + v_step2[j]);
    }
    copy_v3_v3(v_step1, v_step2);
  }
  return total_volume;
}

/**
 * \note
 * - Results won't be correct if polygon is non-planar.
 * - This has the advantage over #mesh_calc_poly_volume_centroid
 *   that it doesn't depend on solid geometry, instead it weights the surface by volume.
 */
static float poly_area_centroid_calc(const Span<float3> positions,
                                     const Span<int> poly_verts,
                                     float r_cent[3])
{
  float total_area = 0.0f;
  float v1[3], v2[3], v3[3], tri_cent[3];

  const float3 normal = blender::bke::mesh::poly_normal_calc(positions, poly_verts);

  copy_v3_v3(v1, positions[poly_verts[0]]);
  copy_v3_v3(v2, positions[poly_verts[1]]);

  zero_v3(r_cent);

  for (int i = 2; i < poly_verts.size(); i++) {
    copy_v3_v3(v3, positions[poly_verts[i]]);

    float tri_area = area_tri_signed_v3(v1, v2, v3, normal);
    total_area += tri_area;

    mid_v3_v3v3v3(tri_cent, v1, v2, v3);
    madd_v3_v3fl(r_cent, tri_cent, tri_area);

    copy_v3_v3(v2, v3);
  }

  mul_v3_fl(r_cent, 1.0f / total_area);

  return total_area;
}

void poly_angles_calc(const Span<float3> vert_positions,
                      const Span<int> poly_verts,
                      MutableSpan<float> angles)
{
  float nor_prev[3];
  float nor_next[3];

  int i_this = poly_verts.size() - 1;
  int i_next = 0;

  sub_v3_v3v3(
      nor_prev, vert_positions[poly_verts[i_this - 1]], vert_positions[poly_verts[i_this]]);
  normalize_v3(nor_prev);

  while (i_next < poly_verts.size()) {
    sub_v3_v3v3(nor_next, vert_positions[poly_verts[i_this]], vert_positions[poly_verts[i_next]]);
    normalize_v3(nor_next);
    angles[i_this] = angle_normalized_v3v3(nor_prev, nor_next);

    /* step */
    copy_v3_v3(nor_prev, nor_next);
    i_this = i_next;
    i_next++;
  }
}

}  // namespace blender::bke::mesh

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Center Calculation
 * \{ */

bool BKE_mesh_center_median(const Mesh *me, float r_cent[3])
{
  const Span<float3> positions = me->vert_positions();
  zero_v3(r_cent);
  for (const int i : positions.index_range()) {
    add_v3_v3(r_cent, positions[i]);
  }
  /* otherwise we get NAN for 0 verts */
  if (me->totvert) {
    mul_v3_fl(r_cent, 1.0f / float(me->totvert));
  }
  return (me->totvert != 0);
}

bool BKE_mesh_center_median_from_polys(const Mesh *me, float r_cent[3])
{
  int tot = 0;
  const Span<float3> positions = me->vert_positions();
  const blender::OffsetIndices polys = me->polys();
  const Span<int> corner_verts = me->corner_verts();
  zero_v3(r_cent);
  for (const int i : polys.index_range()) {
    for (const int vert : corner_verts.slice(polys[i])) {
      add_v3_v3(r_cent, positions[vert]);
    }
    tot += polys[i].size();
  }
  /* otherwise we get NAN for 0 verts */
  if (me->totpoly) {
    mul_v3_fl(r_cent, 1.0f / float(tot));
  }
  return (me->totpoly != 0);
}

bool BKE_mesh_center_bounds(const Mesh *me, float r_cent[3])
{
  float min[3], max[3];
  INIT_MINMAX(min, max);
  if (BKE_mesh_minmax(me, min, max)) {
    mid_v3_v3v3(r_cent, min, max);
    return true;
  }

  return false;
}

bool BKE_mesh_center_of_surface(const Mesh *me, float r_cent[3])
{
  float poly_area;
  float total_area = 0.0f;
  float poly_cent[3];
  const Span<float3> positions = me->vert_positions();
  const blender::OffsetIndices polys = me->polys();
  const Span<int> corner_verts = me->corner_verts();

  zero_v3(r_cent);

  /* calculate a weighted average of polygon centroids */
  for (const int i : polys.index_range()) {
    poly_area = blender::bke::mesh::poly_area_centroid_calc(
        positions, corner_verts.slice(polys[i]), poly_cent);

    madd_v3_v3fl(r_cent, poly_cent, poly_area);
    total_area += poly_area;
  }
  /* otherwise we get NAN for 0 polys */
  if (me->totpoly) {
    mul_v3_fl(r_cent, 1.0f / total_area);
  }

  /* zero area faces cause this, fallback to median */
  if (UNLIKELY(!is_finite_v3(r_cent))) {
    return BKE_mesh_center_median(me, r_cent);
  }

  return (me->totpoly != 0);
}

bool BKE_mesh_center_of_volume(const Mesh *me, float r_cent[3])
{
  float poly_volume;
  float total_volume = 0.0f;
  float poly_cent[3];
  const Span<float3> positions = me->vert_positions();
  const blender::OffsetIndices polys = me->polys();
  const Span<int> corner_verts = me->corner_verts();

  /* Use an initial center to avoid numeric instability of geometry far away from the center. */
  float init_cent[3];
  const bool init_cent_result = BKE_mesh_center_median_from_polys(me, init_cent);

  zero_v3(r_cent);

  /* calculate a weighted average of polyhedron centroids */
  for (const int i : polys.index_range()) {
    poly_volume = blender::bke::mesh::mesh_calc_poly_volume_centroid_with_reference_center(
        positions, corner_verts.slice(polys[i]), init_cent, poly_cent);

    /* poly_cent is already volume-weighted, so no need to multiply by the volume */
    add_v3_v3(r_cent, poly_cent);
    total_volume += poly_volume;
  }
  /* otherwise we get NAN for 0 polys */
  if (total_volume != 0.0f) {
    /* multiply by 0.25 to get the correct centroid */
    /* no need to divide volume by 6 as the centroid is weighted by 6x the volume,
     * so it all cancels out. */
    mul_v3_fl(r_cent, 0.25f / total_volume);
  }

  /* this can happen for non-manifold objects, fallback to median */
  if (UNLIKELY(!is_finite_v3(r_cent))) {
    copy_v3_v3(r_cent, init_cent);
    return init_cent_result;
  }
  add_v3_v3(r_cent, init_cent);
  return (me->totpoly != 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Volume Calculation
 * \{ */

static bool mesh_calc_center_centroid_ex(const float (*positions)[3],
                                         int /*mverts_num*/,
                                         const MLoopTri *looptri,
                                         int looptri_num,
                                         const int *corner_verts,
                                         float r_center[3])
{

  zero_v3(r_center);

  if (looptri_num == 0) {
    return false;
  }

  float totweight = 0.0f;
  const MLoopTri *lt;
  int i;
  for (i = 0, lt = looptri; i < looptri_num; i++, lt++) {
    const float *v1 = positions[corner_verts[lt->tri[0]]];
    const float *v2 = positions[corner_verts[lt->tri[1]]];
    const float *v3 = positions[corner_verts[lt->tri[2]]];
    float area;

    area = area_tri_v3(v1, v2, v3);
    madd_v3_v3fl(r_center, v1, area);
    madd_v3_v3fl(r_center, v2, area);
    madd_v3_v3fl(r_center, v3, area);
    totweight += area;
  }
  if (totweight == 0.0f) {
    return false;
  }

  mul_v3_fl(r_center, 1.0f / (3.0f * totweight));

  return true;
}

void BKE_mesh_calc_volume(const float (*vert_positions)[3],
                          const int mverts_num,
                          const MLoopTri *looptri,
                          const int looptri_num,
                          const int *corner_verts,
                          float *r_volume,
                          float r_center[3])
{
  const MLoopTri *lt;
  float center[3];
  float totvol;
  int i;

  if (r_volume) {
    *r_volume = 0.0f;
  }
  if (r_center) {
    zero_v3(r_center);
  }

  if (looptri_num == 0) {
    return;
  }

  if (!mesh_calc_center_centroid_ex(
          vert_positions, mverts_num, looptri, looptri_num, corner_verts, center))
  {
    return;
  }

  totvol = 0.0f;

  for (i = 0, lt = looptri; i < looptri_num; i++, lt++) {
    const float *v1 = vert_positions[corner_verts[lt->tri[0]]];
    const float *v2 = vert_positions[corner_verts[lt->tri[1]]];
    const float *v3 = vert_positions[corner_verts[lt->tri[2]]];
    float vol;

    vol = volume_tetrahedron_signed_v3(center, v1, v2, v3);
    if (r_volume) {
      totvol += vol;
    }
    if (r_center) {
      /* averaging factor 1/3 is applied in the end */
      madd_v3_v3fl(r_center, v1, vol);
      madd_v3_v3fl(r_center, v2, vol);
      madd_v3_v3fl(r_center, v3, vol);
    }
  }

  /* NOTE: Depending on arbitrary centroid position,
   * totvol can become negative even for a valid mesh.
   * The true value is always the positive value.
   */
  if (r_volume) {
    *r_volume = fabsf(totvol);
  }
  if (r_center) {
    /* NOTE: Factor 1/3 is applied once for all vertices here.
     * This also automatically negates the vector if totvol is negative.
     */
    if (totvol != 0.0f) {
      mul_v3_fl(r_center, (1.0f / 3.0f) / totvol);
    }
  }
}

/** \} */

void BKE_mesh_mdisp_flip(MDisps *md, const bool use_loop_mdisp_flip)
{
  if (UNLIKELY(!md->totdisp || !md->disps)) {
    return;
  }

  const int sides = int(sqrt(md->totdisp));
  float(*co)[3] = md->disps;

  for (int x = 0; x < sides; x++) {
    float *co_a, *co_b;

    for (int y = 0; y < x; y++) {
      co_a = co[y * sides + x];
      co_b = co[x * sides + y];

      swap_v3_v3(co_a, co_b);
      std::swap(co_a[0], co_a[1]);
      std::swap(co_b[0], co_b[1]);

      if (use_loop_mdisp_flip) {
        co_a[2] *= -1.0f;
        co_b[2] *= -1.0f;
      }
    }

    co_a = co[x * sides + x];

    std::swap(co_a[0], co_a[1]);

    if (use_loop_mdisp_flip) {
      co_a[2] *= -1.0f;
    }
  }
}

void BKE_mesh_polygon_flip_ex(const int poly_offset,
                              const int poly_size,
                              int *corner_verts,
                              int *corner_edges,
                              CustomData *ldata,
                              float (*lnors)[3],
                              MDisps *mdisp,
                              const bool use_loop_mdisp_flip)
{
  int loopstart = poly_offset;
  int loopend = loopstart + poly_size - 1;
  const bool corner_verts_in_data = (CustomData_get_layer_named(
                                         ldata, CD_PROP_INT32, ".corner_vert") == corner_verts);
  const bool corner_edges_in_data = (CustomData_get_layer_named(
                                         ldata, CD_PROP_INT32, ".corner_edge") == corner_edges);

  if (mdisp) {
    for (int i = loopstart; i <= loopend; i++) {
      BKE_mesh_mdisp_flip(&mdisp[i], use_loop_mdisp_flip);
    }
  }

  /* Note that we keep same start vertex for flipped face. */

  /* We also have to update loops edge
   * (they will get their original 'other edge', that is,
   * the original edge of their original previous loop)... */
  int prev_edge_index = corner_edges[loopstart];
  corner_edges[loopstart] = corner_edges[loopend];

  for (loopstart++; loopend > loopstart; loopstart++, loopend--) {
    corner_edges[loopend] = corner_edges[loopend - 1];
    std::swap(corner_edges[loopstart], prev_edge_index);

    if (!corner_verts_in_data) {
      std::swap(corner_verts[loopstart], corner_verts[loopend]);
    }
    if (!corner_edges_in_data) {
      std::swap(corner_edges[loopstart], corner_edges[loopend]);
    }
    if (lnors) {
      swap_v3_v3(lnors[loopstart], lnors[loopend]);
    }
    CustomData_swap(ldata, loopstart, loopend);
  }
  /* Even if we did not swap the other 'pivot' loop, we need to set its swapped edge. */
  if (loopstart == loopend) {
    corner_edges[loopstart] = prev_edge_index;
  }
}

void BKE_mesh_polygon_flip(const int poly_offset,
                           const int poly_size,
                           int *corner_verts,
                           int *corner_edges,
                           CustomData *ldata,
                           const int totloop)
{
  MDisps *mdisp = (MDisps *)CustomData_get_layer_for_write(ldata, CD_MDISPS, totloop);
  BKE_mesh_polygon_flip_ex(
      poly_offset, poly_size, corner_verts, corner_edges, ldata, nullptr, mdisp, true);
}

void BKE_mesh_polys_flip(
    const int *poly_offsets, int *corner_verts, int *corner_edges, CustomData *ldata, int totpoly)
{
  const blender::OffsetIndices polys(blender::Span(poly_offsets, totpoly + 1));
  MDisps *mdisp = (MDisps *)CustomData_get_layer_for_write(ldata, CD_MDISPS, totpoly);
  for (const int i : polys.index_range()) {
    BKE_mesh_polygon_flip_ex(polys[i].start(),
                             polys[i].size(),
                             corner_verts,
                             corner_edges,
                             ldata,
                             nullptr,
                             mdisp,
                             true);
  }
}

/* -------------------------------------------------------------------- */
/** \name Mesh Flag Flushing
 * \{ */

void BKE_mesh_flush_hidden_from_verts(Mesh *me)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = me->attributes_for_write();

  const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT, false);
  if (hide_vert.is_single() && !hide_vert.get_internal_single()) {
    attributes.remove(".hide_edge");
    attributes.remove(".hide_poly");
    return;
  }
  const VArraySpan<bool> hide_vert_span{hide_vert};
  const Span<int2> edges = me->edges();
  const OffsetIndices polys = me->polys();
  const Span<int> corner_verts = me->corner_verts();

  /* Hide edges when either of their vertices are hidden. */
  SpanAttributeWriter<bool> hide_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_edge", ATTR_DOMAIN_EDGE);
  for (const int i : edges.index_range()) {
    const int2 &edge = edges[i];
    hide_edge.span[i] = hide_vert_span[edge[0]] || hide_vert_span[edge[1]];
  }
  hide_edge.finish();

  /* Hide polygons when any of their vertices are hidden. */
  SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE);
  for (const int i : polys.index_range()) {
    const Span<int> poly_verts = corner_verts.slice(polys[i]);
    hide_poly.span[i] = std::any_of(poly_verts.begin(), poly_verts.end(), [&](const int vert) {
      return hide_vert_span[vert];
    });
  }
  hide_poly.finish();
}

void BKE_mesh_flush_hidden_from_polys(Mesh *me)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = me->attributes_for_write();

  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);
  if (hide_poly.is_single() && !hide_poly.get_internal_single()) {
    attributes.remove(".hide_vert");
    attributes.remove(".hide_edge");
    return;
  }
  const VArraySpan<bool> hide_poly_span{hide_poly};
  const OffsetIndices polys = me->polys();
  const Span<int> corner_verts = me->corner_verts();
  const Span<int> corner_edges = me->corner_edges();
  SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT);
  SpanAttributeWriter<bool> hide_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_edge", ATTR_DOMAIN_EDGE);

  /* Hide all edges or vertices connected to hidden polygons. */
  for (const int i : polys.index_range()) {
    if (hide_poly_span[i]) {
      for (const int corner : polys[i]) {
        hide_vert.span[corner_verts[corner]] = true;
        hide_edge.span[corner_edges[corner]] = true;
      }
    }
  }
  /* Unhide vertices and edges connected to visible polygons. */
  for (const int i : polys.index_range()) {
    if (!hide_poly_span[i]) {
      for (const int corner : polys[i]) {
        hide_vert.span[corner_verts[corner]] = false;
        hide_edge.span[corner_edges[corner]] = false;
      }
    }
  }

  hide_vert.finish();
  hide_edge.finish();
}

void BKE_mesh_flush_select_from_polys(Mesh *me)
{
  using namespace blender::bke;
  MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> select_poly = *attributes.lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);
  if (select_poly.is_single() && !select_poly.get_internal_single()) {
    attributes.remove(".select_vert");
    attributes.remove(".select_edge");
    return;
  }
  SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_vert", ATTR_DOMAIN_POINT);
  SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_edge", ATTR_DOMAIN_EDGE);

  /* Use generic domain interpolation to read the polygon attribute on the other domains.
   * Assume selected faces are not hidden and none of their vertices/edges are hidden. */
  attributes.lookup_or_default<bool>(".select_poly", ATTR_DOMAIN_POINT, false)
      .varray.materialize(select_vert.span);
  attributes.lookup_or_default<bool>(".select_poly", ATTR_DOMAIN_EDGE, false)
      .varray.materialize(select_edge.span);

  select_vert.finish();
  select_edge.finish();
}

static void mesh_flush_select_from_verts(const Span<blender::int2> edges,
                                         const blender::OffsetIndices<int> polys,
                                         const Span<int> corner_verts,
                                         const VArray<bool> &hide_edge,
                                         const VArray<bool> &hide_poly,
                                         const VArray<bool> &select_vert,
                                         MutableSpan<bool> select_edge,
                                         MutableSpan<bool> select_poly)
{
  /* Select visible edges that have both of their vertices selected. */
  for (const int i : edges.index_range()) {
    if (!hide_edge[i]) {
      const blender::int2 &edge = edges[i];
      select_edge[i] = select_vert[edge[0]] && select_vert[edge[1]];
    }
  }

  /* Select visible faces that have all of their vertices selected. */
  for (const int i : polys.index_range()) {
    if (!hide_poly[i]) {
      const Span<int> poly_verts = corner_verts.slice(polys[i]);
      select_poly[i] = std::all_of(
          poly_verts.begin(), poly_verts.end(), [&](const int vert) { return select_vert[vert]; });
    }
  }
}

void BKE_mesh_flush_select_from_verts(Mesh *me)
{
  using namespace blender::bke;
  MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  if (select_vert.is_single() && !select_vert.get_internal_single()) {
    attributes.remove(".select_edge");
    attributes.remove(".select_poly");
    return;
  }
  SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_edge", ATTR_DOMAIN_EDGE);
  SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_poly", ATTR_DOMAIN_FACE);
  mesh_flush_select_from_verts(
      me->edges(),
      me->polys(),
      me->corner_verts(),
      *attributes.lookup_or_default<bool>(".hide_edge", ATTR_DOMAIN_EDGE, false),
      *attributes.lookup_or_default<bool>(".hide_poly", ATTR_DOMAIN_FACE, false),
      select_vert,
      select_edge.span,
      select_poly.span);
  select_edge.finish();
  select_poly.finish();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Spatial Calculation
 * \{ */

void BKE_mesh_calc_relative_deform(const int *poly_offsets,
                                   const int totpoly,
                                   const int *corner_verts,
                                   const int totvert,

                                   const float (*vert_cos_src)[3],
                                   const float (*vert_cos_dst)[3],

                                   const float (*vert_cos_org)[3],
                                   float (*vert_cos_new)[3])
{
  const blender::OffsetIndices<int> polys({poly_offsets, totpoly + 1});

  int *vert_accum = (int *)MEM_calloc_arrayN(size_t(totvert), sizeof(*vert_accum), __func__);

  memset(vert_cos_new, '\0', sizeof(*vert_cos_new) * size_t(totvert));

  for (const int i : polys.index_range()) {
    const blender::IndexRange poly = polys[i];
    const int *poly_verts = &corner_verts[poly.start()];

    for (int j = 0; j < poly.size(); j++) {
      const int v_prev = poly_verts[(poly.size() + (j - 1)) % poly.size()];
      const int v_curr = poly_verts[j];
      const int v_next = poly_verts[(j + 1) % poly.size()];

      float tvec[3];

      transform_point_by_tri_v3(tvec,
                                vert_cos_dst[v_curr],
                                vert_cos_org[v_prev],
                                vert_cos_org[v_curr],
                                vert_cos_org[v_next],
                                vert_cos_src[v_prev],
                                vert_cos_src[v_curr],
                                vert_cos_src[v_next]);

      add_v3_v3(vert_cos_new[v_curr], tvec);
      vert_accum[v_curr] += 1;
    }
  }

  for (int i = 0; i < totvert; i++) {
    if (vert_accum[i]) {
      mul_v3_fl(vert_cos_new[i], 1.0f / float(vert_accum[i]));
    }
    else {
      copy_v3_v3(vert_cos_new[i], vert_cos_org[i]);
    }
  }

  MEM_freeN(vert_accum);
}

/** \} */
