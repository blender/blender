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

#include "BKE_customdata.h"

#include "BKE_attribute.hh"
#include "BKE_mesh.h"
#include "BKE_multires.h"

using blender::float3;
using blender::MutableSpan;
using blender::Span;
using blender::VArray;

/* -------------------------------------------------------------------- */
/** \name Polygon Calculations
 * \{ */

static void mesh_calc_ngon_center(const MPoly *mpoly,
                                  const MLoop *loopstart,
                                  const float (*positions)[3],
                                  float cent[3])
{
  const float w = 1.0f / float(mpoly->totloop);

  zero_v3(cent);

  for (int i = 0; i < mpoly->totloop; i++) {
    madd_v3_v3fl(cent, positions[(loopstart++)->v], w);
  }
}

void BKE_mesh_calc_poly_center(const MPoly *mpoly,
                               const MLoop *loopstart,
                               const float (*vert_positions)[3],
                               float r_cent[3])
{
  if (mpoly->totloop == 3) {
    mid_v3_v3v3v3(r_cent,
                  vert_positions[loopstart[0].v],
                  vert_positions[loopstart[1].v],
                  vert_positions[loopstart[2].v]);
  }
  else if (mpoly->totloop == 4) {
    mid_v3_v3v3v3v3(r_cent,
                    vert_positions[loopstart[0].v],
                    vert_positions[loopstart[1].v],
                    vert_positions[loopstart[2].v],
                    vert_positions[loopstart[3].v]);
  }
  else {
    mesh_calc_ngon_center(mpoly, loopstart, vert_positions, r_cent);
  }
}

float BKE_mesh_calc_poly_area(const MPoly *mpoly,
                              const MLoop *loopstart,
                              const float (*vert_positions)[3])
{
  if (mpoly->totloop == 3) {
    return area_tri_v3(vert_positions[loopstart[0].v],
                       vert_positions[loopstart[1].v],
                       vert_positions[loopstart[2].v]);
  }

  const MLoop *l_iter = loopstart;
  float(*vertexcos)[3] = (float(*)[3])BLI_array_alloca(vertexcos, size_t(mpoly->totloop));

  /* pack vertex cos into an array for area_poly_v3 */
  for (int i = 0; i < mpoly->totloop; i++, l_iter++) {
    copy_v3_v3(vertexcos[i], vert_positions[l_iter->v]);
  }

  /* finally calculate the area */
  float area = area_poly_v3((const float(*)[3])vertexcos, uint(mpoly->totloop));

  return area;
}

float BKE_mesh_calc_area(const Mesh *me)
{
  const Span<float3> positions = me->vert_positions();
  const Span<MPoly> polys = me->polys();
  const Span<MLoop> loops = me->loops();

  float total_area = 0.0f;
  for (const MPoly &poly : polys) {
    total_area += BKE_mesh_calc_poly_area(
        &poly, &loops[poly.loopstart], reinterpret_cast<const float(*)[3]>(positions.data()));
  }
  return total_area;
}

static float UNUSED_FUNCTION(mesh_calc_poly_volume_centroid)(const MPoly *mpoly,
                                                             const MLoop *loopstart,
                                                             const float (*positions)[3],
                                                             float r_cent[3])
{
  const float *v_pivot, *v_step1;
  float total_volume = 0.0f;

  zero_v3(r_cent);

  v_pivot = positions[loopstart[0].v];
  v_step1 = positions[loopstart[1].v];

  for (int i = 2; i < mpoly->totloop; i++) {
    const float *v_step2 = positions[loopstart[i].v];

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

/**
 * A version of mesh_calc_poly_volume_centroid that takes an initial reference center,
 * use this to increase numeric stability as the quality of the result becomes
 * very low quality as the value moves away from 0.0, see: T65986.
 */
static float mesh_calc_poly_volume_centroid_with_reference_center(const MPoly *mpoly,
                                                                  const MLoop *loopstart,
                                                                  const Span<float3> positions,
                                                                  const float reference_center[3],
                                                                  float r_cent[3])
{
  /* See: mesh_calc_poly_volume_centroid for comments. */
  float v_pivot[3], v_step1[3];
  float total_volume = 0.0f;
  zero_v3(r_cent);
  sub_v3_v3v3(v_pivot, positions[loopstart[0].v], reference_center);
  sub_v3_v3v3(v_step1, positions[loopstart[1].v], reference_center);
  for (int i = 2; i < mpoly->totloop; i++) {
    float v_step2[3];
    sub_v3_v3v3(v_step2, positions[loopstart[i].v], reference_center);
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
static float mesh_calc_poly_area_centroid(const MPoly *mpoly,
                                          const MLoop *loopstart,
                                          const float (*positions)[3],
                                          float r_cent[3])
{
  float total_area = 0.0f;
  float v1[3], v2[3], v3[3], normal[3], tri_cent[3];

  BKE_mesh_calc_poly_normal(mpoly, loopstart, positions, normal);
  copy_v3_v3(v1, positions[loopstart[0].v]);
  copy_v3_v3(v2, positions[loopstart[1].v]);
  zero_v3(r_cent);

  for (int i = 2; i < mpoly->totloop; i++) {
    copy_v3_v3(v3, positions[loopstart[i].v]);

    float tri_area = area_tri_signed_v3(v1, v2, v3, normal);
    total_area += tri_area;

    mid_v3_v3v3v3(tri_cent, v1, v2, v3);
    madd_v3_v3fl(r_cent, tri_cent, tri_area);

    copy_v3_v3(v2, v3);
  }

  mul_v3_fl(r_cent, 1.0f / total_area);

  return total_area;
}

void BKE_mesh_calc_poly_angles(const MPoly *mpoly,
                               const MLoop *loopstart,
                               const float (*vert_positions)[3],
                               float angles[])
{
  float nor_prev[3];
  float nor_next[3];

  int i_this = mpoly->totloop - 1;
  int i_next = 0;

  sub_v3_v3v3(
      nor_prev, vert_positions[loopstart[i_this - 1].v], vert_positions[loopstart[i_this].v]);
  normalize_v3(nor_prev);

  while (i_next < mpoly->totloop) {
    sub_v3_v3v3(
        nor_next, vert_positions[loopstart[i_this].v], vert_positions[loopstart[i_next].v]);
    normalize_v3(nor_next);
    angles[i_this] = angle_normalized_v3v3(nor_prev, nor_next);

    /* step */
    copy_v3_v3(nor_prev, nor_next);
    i_this = i_next;
    i_next++;
  }
}

void BKE_mesh_poly_edgehash_insert(EdgeHash *ehash, const MPoly *mp, const MLoop *mloop)
{
  const MLoop *ml, *ml_next;
  int i = mp->totloop;

  ml_next = mloop;      /* first loop */
  ml = &ml_next[i - 1]; /* last loop */

  while (i-- != 0) {
    BLI_edgehash_reinsert(ehash, ml->v, ml_next->v, nullptr);

    ml = ml_next;
    ml_next++;
  }
}

void BKE_mesh_poly_edgebitmap_insert(uint *edge_bitmap, const MPoly *mp, const MLoop *mloop)
{
  const MLoop *ml;
  int i = mp->totloop;

  ml = mloop;

  while (i-- != 0) {
    BLI_BITMAP_ENABLE(edge_bitmap, ml->e);
    ml++;
  }
}

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
  const Span<MPoly> polys = me->polys();
  const Span<MLoop> loops = me->loops();
  zero_v3(r_cent);
  for (const MPoly &poly : polys) {
    int loopend = poly.loopstart + poly.totloop;
    for (int j = poly.loopstart; j < loopend; j++) {
      add_v3_v3(r_cent, positions[loops[j].v]);
    }
    tot += poly.totloop;
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
  int i = me->totpoly;
  const MPoly *mpoly;
  float poly_area;
  float total_area = 0.0f;
  float poly_cent[3];
  const float(*positions)[3] = BKE_mesh_vert_positions(me);
  const MPoly *polys = BKE_mesh_polys(me);
  const MLoop *loops = BKE_mesh_loops(me);

  zero_v3(r_cent);

  /* calculate a weighted average of polygon centroids */
  for (mpoly = polys; i--; mpoly++) {
    poly_area = mesh_calc_poly_area_centroid(
        mpoly, loops + mpoly->loopstart, positions, poly_cent);

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
  int i = me->totpoly;
  const MPoly *mpoly;
  float poly_volume;
  float total_volume = 0.0f;
  float poly_cent[3];
  const Span<float3> positions = me->vert_positions();
  const MPoly *polys = BKE_mesh_polys(me);
  const MLoop *loops = BKE_mesh_loops(me);

  /* Use an initial center to avoid numeric instability of geometry far away from the center. */
  float init_cent[3];
  const bool init_cent_result = BKE_mesh_center_median_from_polys(me, init_cent);

  zero_v3(r_cent);

  /* calculate a weighted average of polyhedron centroids */
  for (mpoly = polys; i--; mpoly++) {
    poly_volume = mesh_calc_poly_volume_centroid_with_reference_center(
        mpoly, loops + mpoly->loopstart, positions, init_cent, poly_cent);

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
                                         const MLoop *mloop,
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
    const float *v1 = positions[mloop[lt->tri[0]].v];
    const float *v2 = positions[mloop[lt->tri[1]].v];
    const float *v3 = positions[mloop[lt->tri[2]].v];
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
                          const MLoop *mloop,
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
          vert_positions, mverts_num, looptri, looptri_num, mloop, center)) {
    return;
  }

  totvol = 0.0f;

  for (i = 0, lt = looptri; i < looptri_num; i++, lt++) {
    const float *v1 = vert_positions[mloop[lt->tri[0]].v];
    const float *v2 = vert_positions[mloop[lt->tri[1]].v];
    const float *v3 = vert_positions[mloop[lt->tri[2]].v];
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

void BKE_mesh_polygon_flip_ex(const MPoly *mpoly,
                              MLoop *mloop,
                              CustomData *ldata,
                              float (*lnors)[3],
                              MDisps *mdisp,
                              const bool use_loop_mdisp_flip)
{
  int loopstart = mpoly->loopstart;
  int loopend = loopstart + mpoly->totloop - 1;
  const bool loops_in_ldata = (CustomData_get_layer(ldata, CD_MLOOP) == mloop);

  if (mdisp) {
    for (int i = loopstart; i <= loopend; i++) {
      BKE_mesh_mdisp_flip(&mdisp[i], use_loop_mdisp_flip);
    }
  }

  /* Note that we keep same start vertex for flipped face. */

  /* We also have to update loops edge
   * (they will get their original 'other edge', that is,
   * the original edge of their original previous loop)... */
  uint prev_edge_index = mloop[loopstart].e;
  mloop[loopstart].e = mloop[loopend].e;

  for (loopstart++; loopend > loopstart; loopstart++, loopend--) {
    mloop[loopend].e = mloop[loopend - 1].e;
    std::swap(mloop[loopstart].e, prev_edge_index);

    if (!loops_in_ldata) {
      std::swap(mloop[loopstart], mloop[loopend]);
    }
    if (lnors) {
      swap_v3_v3(lnors[loopstart], lnors[loopend]);
    }
    CustomData_swap(ldata, loopstart, loopend);
  }
  /* Even if we did not swap the other 'pivot' loop, we need to set its swapped edge. */
  if (loopstart == loopend) {
    mloop[loopstart].e = prev_edge_index;
  }
}

void BKE_mesh_polygon_flip(const MPoly *mpoly, MLoop *mloop, CustomData *ldata)
{
  MDisps *mdisp = (MDisps *)CustomData_get_layer(ldata, CD_MDISPS);
  BKE_mesh_polygon_flip_ex(mpoly, mloop, ldata, nullptr, mdisp, true);
}

void BKE_mesh_polys_flip(const MPoly *mpoly, MLoop *mloop, CustomData *ldata, int totpoly)
{
  MDisps *mdisp = (MDisps *)CustomData_get_layer(ldata, CD_MDISPS);
  const MPoly *mp;
  int i;

  for (mp = mpoly, i = 0; i < totpoly; mp++, i++) {
    BKE_mesh_polygon_flip_ex(mp, mloop, ldata, nullptr, mdisp, true);
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

  const VArray<bool> hide_vert = attributes.lookup_or_default<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT, false);
  if (hide_vert.is_single() && !hide_vert.get_internal_single()) {
    attributes.remove(".hide_edge");
    attributes.remove(".hide_poly");
    return;
  }
  const VArraySpan<bool> hide_vert_span{hide_vert};
  const Span<MEdge> edges = me->edges();
  const Span<MPoly> polys = me->polys();
  const Span<MLoop> loops = me->loops();

  /* Hide edges when either of their vertices are hidden. */
  SpanAttributeWriter<bool> hide_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_edge", ATTR_DOMAIN_EDGE);
  for (const int i : edges.index_range()) {
    const MEdge &edge = edges[i];
    hide_edge.span[i] = hide_vert_span[edge.v1] || hide_vert_span[edge.v2];
  }
  hide_edge.finish();

  /* Hide polygons when any of their vertices are hidden. */
  SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE);
  for (const int i : polys.index_range()) {
    const MPoly &poly = polys[i];
    const Span<MLoop> poly_loops = loops.slice(poly.loopstart, poly.totloop);
    hide_poly.span[i] = std::any_of(poly_loops.begin(), poly_loops.end(), [&](const MLoop &loop) {
      return hide_vert_span[loop.v];
    });
  }
  hide_poly.finish();
}

void BKE_mesh_flush_hidden_from_polys(Mesh *me)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = me->attributes_for_write();

  const VArray<bool> hide_poly = attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);
  if (hide_poly.is_single() && !hide_poly.get_internal_single()) {
    attributes.remove(".hide_vert");
    attributes.remove(".hide_edge");
    return;
  }
  const VArraySpan<bool> hide_poly_span{hide_poly};
  const Span<MPoly> polys = me->polys();
  const Span<MLoop> loops = me->loops();
  SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT);
  SpanAttributeWriter<bool> hide_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_edge", ATTR_DOMAIN_EDGE);

  /* Hide all edges or vertices connected to hidden polygons. */
  for (const int i : polys.index_range()) {
    if (hide_poly_span[i]) {
      const MPoly &poly = polys[i];
      for (const MLoop &loop : loops.slice(poly.loopstart, poly.totloop)) {
        hide_vert.span[loop.v] = true;
        hide_edge.span[loop.e] = true;
      }
    }
  }
  /* Unhide vertices and edges connected to visible polygons. */
  for (const int i : polys.index_range()) {
    if (!hide_poly_span[i]) {
      const MPoly &poly = polys[i];
      for (const MLoop &loop : loops.slice(poly.loopstart, poly.totloop)) {
        hide_vert.span[loop.v] = false;
        hide_edge.span[loop.e] = false;
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
  const VArray<bool> select_poly = attributes.lookup_or_default<bool>(
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
      .materialize(select_vert.span);
  attributes.lookup_or_default<bool>(".select_poly", ATTR_DOMAIN_EDGE, false)
      .materialize(select_edge.span);

  select_vert.finish();
  select_edge.finish();
}

static void mesh_flush_select_from_verts(const Span<MEdge> edges,
                                         const Span<MPoly> polys,
                                         const Span<MLoop> loops,
                                         const VArray<bool> &hide_edge,
                                         const VArray<bool> &hide_poly,
                                         const VArray<bool> &select_vert,
                                         MutableSpan<bool> select_edge,
                                         MutableSpan<bool> select_poly)
{
  /* Select visible edges that have both of their vertices selected. */
  for (const int i : edges.index_range()) {
    if (!hide_edge[i]) {
      const MEdge &edge = edges[i];
      select_edge[i] = select_vert[edge.v1] && select_vert[edge.v2];
    }
  }

  /* Select visible faces that have all of their vertices selected. */
  for (const int i : polys.index_range()) {
    if (!hide_poly[i]) {
      const MPoly &poly = polys[i];
      const Span<MLoop> poly_loops = loops.slice(poly.loopstart, poly.totloop);
      select_poly[i] = std::all_of(poly_loops.begin(), poly_loops.end(), [&](const MLoop &loop) {
        return select_vert[loop.v];
      });
    }
  }
}

void BKE_mesh_flush_select_from_verts(Mesh *me)
{
  using namespace blender::bke;
  MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> select_vert = attributes.lookup_or_default<bool>(
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
      me->loops(),
      attributes.lookup_or_default<bool>(".hide_edge", ATTR_DOMAIN_EDGE, false),
      attributes.lookup_or_default<bool>(".hide_poly", ATTR_DOMAIN_FACE, false),
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

void BKE_mesh_calc_relative_deform(const MPoly *mpoly,
                                   const int totpoly,
                                   const MLoop *mloop,
                                   const int totvert,

                                   const float (*vert_cos_src)[3],
                                   const float (*vert_cos_dst)[3],

                                   const float (*vert_cos_org)[3],
                                   float (*vert_cos_new)[3])
{
  const MPoly *mp;
  int i;

  int *vert_accum = (int *)MEM_calloc_arrayN(size_t(totvert), sizeof(*vert_accum), __func__);

  memset(vert_cos_new, '\0', sizeof(*vert_cos_new) * size_t(totvert));

  for (i = 0, mp = mpoly; i < totpoly; i++, mp++) {
    const MLoop *loopstart = mloop + mp->loopstart;

    for (int j = 0; j < mp->totloop; j++) {
      uint v_prev = loopstart[(mp->totloop + (j - 1)) % mp->totloop].v;
      uint v_curr = loopstart[j].v;
      uint v_next = loopstart[(j + 1) % mp->totloop].v;

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

  for (i = 0; i < totvert; i++) {
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
