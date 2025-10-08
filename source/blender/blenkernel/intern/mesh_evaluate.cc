/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions to evaluate mesh data.
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_array_utils.hh"
#include "BLI_index_range.hh"
#include "BLI_math_geom.h"
#include "BLI_span.hh"
#include "BLI_utildefines.h"
#include "BLI_virtual_array.hh"

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"

using blender::float3;
using blender::int2;
using blender::MutableSpan;
using blender::OffsetIndices;
using blender::Span;
using blender::VArray;

/* -------------------------------------------------------------------- */
/** \name Polygon Calculations
 * \{ */

namespace blender::bke::mesh {

static float3 face_center_calc_ngon(const Span<float3> vert_positions, const Span<int> face_verts)
{
  const float w = 1.0f / float(face_verts.size());

  float3 center(0);
  for (const int i : face_verts.index_range()) {
    center += vert_positions[face_verts[i]] * w;
  }
  return center;
}

float3 face_center_calc(const Span<float3> vert_positions, const Span<int> face_verts)
{
  if (face_verts.size() == 3) {
    float3 center;
    mid_v3_v3v3v3(center,
                  vert_positions[face_verts[0]],
                  vert_positions[face_verts[1]],
                  vert_positions[face_verts[2]]);
    return center;
  }
  if (face_verts.size() == 4) {
    float3 center;
    mid_v3_v3v3v3v3(center,
                    vert_positions[face_verts[0]],
                    vert_positions[face_verts[1]],
                    vert_positions[face_verts[2]],
                    vert_positions[face_verts[3]]);
    return center;
  }
  return face_center_calc_ngon(vert_positions, face_verts);
}

float face_area_calc(const Span<float3> vert_positions, const Span<int> face_verts)
{
  if (face_verts.size() == 3) {
    return area_tri_v3(vert_positions[face_verts[0]],
                       vert_positions[face_verts[1]],
                       vert_positions[face_verts[2]]);
  }
  Array<float3, 32> coords(face_verts.size());
  for (const int i : face_verts.index_range()) {
    coords[i] = vert_positions[face_verts[i]];
  }
  return area_poly_v3((const float (*)[3])coords.data(), face_verts.size());
}

}  // namespace blender::bke::mesh

float BKE_mesh_calc_area(const Mesh *mesh)
{
  const Span<float3> positions = mesh->vert_positions();
  const blender::OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  float total_area = 0.0f;
  for (const int i : faces.index_range()) {
    total_area += blender::bke::mesh::face_area_calc(positions, corner_verts.slice(faces[i]));
  }
  return total_area;
}

static float UNUSED_FUNCTION(mesh_calc_face_volume_centroid)(const int *face_verts,
                                                             const int face_size,
                                                             const float (*positions)[3],
                                                             float r_cent[3])
{
  const float *v_pivot, *v_step1;
  float total_volume = 0.0f;

  zero_v3(r_cent);

  v_pivot = positions[face_verts[0]];
  v_step1 = positions[face_verts[1]];

  for (int i = 2; i < face_size; i++) {
    const float *v_step2 = positions[face_verts[i]];

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
 * A version of mesh_calc_face_volume_centroid that takes an initial reference center,
 * use this to increase numeric stability as the quality of the result becomes
 * very low quality as the value moves away from 0.0, see: #65986.
 */
static float mesh_calc_face_volume_centroid_with_reference_center(const Span<float3> positions,
                                                                  const Span<int> face_verts,
                                                                  const float3 &reference_center,
                                                                  float r_cent[3])
{
  /* See: mesh_calc_face_volume_centroid for comments. */
  float v_pivot[3], v_step1[3];
  float total_volume = 0.0f;
  zero_v3(r_cent);
  sub_v3_v3v3(v_pivot, positions[face_verts[0]], reference_center);
  sub_v3_v3v3(v_step1, positions[face_verts[1]], reference_center);
  for (int i = 2; i < face_verts.size(); i++) {
    float v_step2[3];
    sub_v3_v3v3(v_step2, positions[face_verts[i]], reference_center);
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
 * - Results won't be correct if face is non-planar.
 * - This has the advantage over #mesh_calc_face_volume_centroid
 *   that it doesn't depend on solid geometry, instead it weights the surface by volume.
 */
static float face_area_centroid_calc(const Span<float3> positions,
                                     const Span<int> face_verts,
                                     float r_cent[3])
{
  float total_area = 0.0f;
  float v1[3], v2[3], v3[3], tri_cent[3];

  const float3 normal = blender::bke::mesh::face_normal_calc(positions, face_verts);

  copy_v3_v3(v1, positions[face_verts[0]]);
  copy_v3_v3(v2, positions[face_verts[1]]);

  zero_v3(r_cent);

  for (int i = 2; i < face_verts.size(); i++) {
    copy_v3_v3(v3, positions[face_verts[i]]);

    float tri_area = area_tri_signed_v3(v1, v2, v3, normal);
    total_area += tri_area;

    mid_v3_v3v3v3(tri_cent, v1, v2, v3);
    madd_v3_v3fl(r_cent, tri_cent, tri_area);

    copy_v3_v3(v2, v3);
  }

  mul_v3_fl(r_cent, 1.0f / total_area);

  return total_area;
}

void face_angles_calc(const Span<float3> vert_positions,
                      const Span<int> face_verts,
                      MutableSpan<float> angles)
{
  float nor_prev[3];
  float nor_next[3];

  int i_this = face_verts.size() - 1;
  int i_next = 0;

  sub_v3_v3v3(
      nor_prev, vert_positions[face_verts[i_this - 1]], vert_positions[face_verts[i_this]]);
  normalize_v3(nor_prev);

  while (i_next < face_verts.size()) {
    sub_v3_v3v3(nor_next, vert_positions[face_verts[i_this]], vert_positions[face_verts[i_next]]);
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

bool BKE_mesh_center_median(const Mesh *mesh, float r_cent[3])
{
  float3 center = blender::array_utils::compute_sum<float3>(mesh->vert_positions());

  /* otherwise we get NAN for 0 verts */
  if (mesh->verts_num) {
    mul_v3_fl(center, 1.0 / float(mesh->verts_num));
  }

  copy_v3_v3(r_cent, center);

  return (mesh->verts_num != 0);
}

bool BKE_mesh_center_median_from_faces(const Mesh *mesh, float r_cent[3])
{
  int tot = 0;
  const Span<float3> positions = mesh->vert_positions();
  const blender::OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  zero_v3(r_cent);
  for (const int i : faces.index_range()) {
    for (const int vert : corner_verts.slice(faces[i])) {
      add_v3_v3(r_cent, positions[vert]);
    }
    tot += faces[i].size();
  }
  /* otherwise we get NAN for 0 verts */
  if (mesh->faces_num) {
    mul_v3_fl(r_cent, 1.0f / float(tot));
  }
  return (mesh->faces_num != 0);
}

bool BKE_mesh_center_of_surface(const Mesh *mesh, float r_cent[3])
{
  float face_area;
  float total_area = 0.0f;
  float face_cent[3];
  const Span<float3> positions = mesh->vert_positions();
  const blender::OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  zero_v3(r_cent);

  /* calculate a weighted average of face centroids */
  for (const int i : faces.index_range()) {
    face_area = blender::bke::mesh::face_area_centroid_calc(
        positions, corner_verts.slice(faces[i]), face_cent);

    madd_v3_v3fl(r_cent, face_cent, face_area);
    total_area += face_area;
  }
  /* otherwise we get NAN for 0 faces */
  if (mesh->faces_num) {
    mul_v3_fl(r_cent, 1.0f / total_area);
  }

  /* zero area faces cause this, fallback to median */
  if (UNLIKELY(!is_finite_v3(r_cent))) {
    return BKE_mesh_center_median(mesh, r_cent);
  }

  return (mesh->faces_num != 0);
}

bool BKE_mesh_center_of_volume(const Mesh *mesh, float r_cent[3])
{
  float face_volume;
  float total_volume = 0.0f;
  float face_cent[3];
  const Span<float3> positions = mesh->vert_positions();
  const blender::OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  /* Use an initial center to avoid numeric instability of geometry far away from the center. */
  float init_cent[3];
  const bool init_cent_result = BKE_mesh_center_median_from_faces(mesh, init_cent);

  zero_v3(r_cent);

  /* calculate a weighted average of polyhedron centroids */
  for (const int i : faces.index_range()) {
    face_volume = blender::bke::mesh::mesh_calc_face_volume_centroid_with_reference_center(
        positions, corner_verts.slice(faces[i]), init_cent, face_cent);

    /* face_cent is already volume-weighted, so no need to multiply by the volume */
    add_v3_v3(r_cent, face_cent);
    total_volume += face_volume;
  }
  /* otherwise we get NAN for 0 faces */
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
  return (mesh->faces_num != 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Volume Calculation
 * \{ */

static bool mesh_calc_center_centroid_ex(const float (*positions)[3],
                                         int /*mverts_num*/,
                                         const blender::int3 *corner_tris,
                                         int corner_tris_num,
                                         const int *corner_verts,
                                         float r_center[3])
{

  zero_v3(r_center);

  if (corner_tris_num == 0) {
    return false;
  }

  float totweight = 0.0f;
  int i;
  for (i = 0; i < corner_tris_num; i++) {
    const float *v1 = positions[corner_verts[corner_tris[i][0]]];
    const float *v2 = positions[corner_verts[corner_tris[i][1]]];
    const float *v3 = positions[corner_verts[corner_tris[i][2]]];
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
                          const blender::int3 *corner_tris,
                          const int corner_tris_num,
                          const int *corner_verts,
                          float *r_volume,
                          float r_center[3])
{
  float center[3];
  float totvol;
  int i;

  if (r_volume) {
    *r_volume = 0.0f;
  }
  if (r_center) {
    zero_v3(r_center);
  }

  if (corner_tris_num == 0) {
    return;
  }

  if (!mesh_calc_center_centroid_ex(
          vert_positions, mverts_num, corner_tris, corner_tris_num, corner_verts, center))
  {
    return;
  }

  totvol = 0.0f;

  for (i = 0; i < corner_tris_num; i++) {
    const float *v1 = vert_positions[corner_verts[corner_tris[i][0]]];
    const float *v2 = vert_positions[corner_verts[corner_tris[i][1]]];
    const float *v3 = vert_positions[corner_verts[corner_tris[i][2]]];
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

/* -------------------------------------------------------------------- */
/** \name Mesh Displacement Data Flip
 * \{ */

void BKE_mesh_mdisp_flip(MDisps *md, const bool use_loop_mdisp_flip)
{
  if (UNLIKELY(!md->totdisp || !md->disps)) {
    return;
  }

  const int sides = int(sqrt(md->totdisp));
  float (*co)[3] = md->disps;

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Visibility Interpolation
 * \{ */

namespace blender::bke {

void mesh_edge_hide_from_vert(const Span<int2> edges,
                              const Span<bool> hide_vert,
                              MutableSpan<bool> hide_edge)
{
  using namespace blender;
  threading::parallel_for(edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      hide_edge[i] = hide_vert[edges[i][0]] || hide_vert[edges[i][1]];
    }
  });
}

void mesh_face_hide_from_vert(const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              const Span<bool> hide_vert,
                              MutableSpan<bool> hide_poly)
{
  using namespace blender;
  threading::parallel_for(faces.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const Span<int> face_verts = corner_verts.slice(faces[i]);
      hide_poly[i] = std::any_of(
          face_verts.begin(), face_verts.end(), [&](const int vert) { return hide_vert[vert]; });
    }
  });
}

void mesh_hide_vert_flush(Mesh &mesh)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();

  const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", AttrDomain::Point, false);
  if (hide_vert.is_single() && !hide_vert.get_internal_single()) {
    attributes.remove(".hide_edge");
    attributes.remove(".hide_poly");
    return;
  }
  const VArraySpan<bool> hide_vert_span{hide_vert};

  SpanAttributeWriter<bool> hide_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_edge", AttrDomain::Edge);
  SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_poly", AttrDomain::Face);

  mesh_edge_hide_from_vert(mesh.edges(), hide_vert_span, hide_edge.span);
  mesh_face_hide_from_vert(mesh.faces(), mesh.corner_verts(), hide_vert_span, hide_poly.span);

  hide_edge.finish();
  hide_poly.finish();
}

void mesh_hide_face_flush(Mesh &mesh)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();

  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", AttrDomain::Face, false);
  if (hide_poly.is_single() && !hide_poly.get_internal_single()) {
    attributes.remove(".hide_vert");
    attributes.remove(".hide_edge");
    return;
  }
  const VArraySpan<bool> hide_poly_span{hide_poly};
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();
  SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_vert", AttrDomain::Point);
  SpanAttributeWriter<bool> hide_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".hide_edge", AttrDomain::Edge);

  /* Hide all edges or vertices connected to hidden polygons. */
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      if (hide_poly_span[i]) {
        hide_vert.span.fill_indices(corner_verts.slice(faces[i]), true);
        hide_edge.span.fill_indices(corner_edges.slice(faces[i]), true);
      }
    }
  });
  /* Unhide vertices and edges connected to visible polygons. */
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      if (!hide_poly_span[i]) {
        hide_vert.span.fill_indices(corner_verts.slice(faces[i]), false);
        hide_edge.span.fill_indices(corner_edges.slice(faces[i]), false);
      }
    }
  });

  hide_vert.finish();
  hide_edge.finish();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selection Interpolation
 * \{ */

void mesh_select_face_flush(Mesh &mesh)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const VArray<bool> select_poly = *attributes.lookup_or_default<bool>(
      ".select_poly", AttrDomain::Face, false);
  if (select_poly.is_single() && !select_poly.get_internal_single()) {
    attributes.remove(".select_vert");
    attributes.remove(".select_edge");
    return;
  }
  SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_vert", AttrDomain::Point);
  SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_edge", AttrDomain::Edge);

  /* Use generic domain interpolation to read the face attribute on the other domains.
   * Assume selected faces are not hidden and none of their vertices/edges are hidden. */
  array_utils::copy(*attributes.lookup_or_default<bool>(".select_poly", AttrDomain::Point, false),
                    select_vert.span);
  array_utils::copy(*attributes.lookup_or_default<bool>(".select_poly", AttrDomain::Edge, false),
                    select_edge.span);

  select_vert.finish();
  select_edge.finish();
}

void mesh_select_vert_flush(Mesh &mesh)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
      ".select_vert", AttrDomain::Point, false);
  if (select_vert.is_single() && !select_vert.get_internal_single()) {
    attributes.remove(".select_edge");
    attributes.remove(".select_poly");
    return;
  }
  SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_edge", AttrDomain::Edge);
  SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_poly", AttrDomain::Face);
  {
    IndexMaskMemory memory;
    const VArray<bool> hide_edge = *attributes.lookup_or_default<bool>(
        ".hide_edge", AttrDomain::Edge, false);
    array_utils::copy(
        *attributes.lookup_or_default<bool>(".select_vert", AttrDomain::Edge, false),
        IndexMask::from_bools(hide_edge, memory).complement(hide_edge.index_range(), memory),
        select_edge.span);
  }
  {
    IndexMaskMemory memory;
    const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
        ".hide_poly", AttrDomain::Face, false);
    array_utils::copy(
        *attributes.lookup_or_default<bool>(".select_vert", AttrDomain::Face, false),
        IndexMask::from_bools(hide_poly, memory).complement(hide_poly.index_range(), memory),
        select_poly.span);
  }
  select_edge.finish();
  select_poly.finish();
}

void mesh_select_edge_flush(Mesh &mesh)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const VArray<bool> select_edge = *attributes.lookup_or_default<bool>(
      ".select_edge", AttrDomain::Point, false);
  if (select_edge.is_single() && !select_edge.get_internal_single()) {
    attributes.remove(".select_vert");
    attributes.remove(".select_poly");
    return;
  }
  SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_vert", AttrDomain::Point);
  SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_only_span<bool>(
      ".select_poly", AttrDomain::Face);
  {
    IndexMaskMemory memory;
    const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
        ".hide_vert", AttrDomain::Point, false);
    array_utils::copy(
        *attributes.lookup_or_default<bool>(".select_edge", AttrDomain::Point, false),
        IndexMask::from_bools(hide_vert, memory).complement(hide_vert.index_range(), memory),
        select_vert.span);
  }
  {
    IndexMaskMemory memory;
    const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
        ".hide_poly", AttrDomain::Face, false);
    array_utils::copy(
        *attributes.lookup_or_default<bool>(".select_edge", AttrDomain::Face, false),
        IndexMask::from_bools(hide_poly, memory).complement(hide_poly.index_range(), memory),
        select_poly.span);
  }
  select_vert.finish();
  select_poly.finish();
}

}  // namespace blender::bke

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Spatial Calculation
 * \{ */

void BKE_mesh_calc_relative_deform(const int *face_offsets,
                                   const int faces_num,
                                   const int *corner_verts,
                                   const int totvert,

                                   const float (*vert_cos_src)[3],
                                   const float (*vert_cos_dst)[3],

                                   const float (*vert_cos_org)[3],
                                   float (*vert_cos_new)[3])
{
  const blender::OffsetIndices<int> faces({face_offsets, faces_num + 1});

  int *vert_accum = MEM_calloc_arrayN<int>(totvert, __func__);

  memset(vert_cos_new, '\0', sizeof(*vert_cos_new) * size_t(totvert));

  for (const int i : faces.index_range()) {
    const blender::IndexRange face = faces[i];
    const int *face_verts = &corner_verts[face.start()];

    for (int j = 0; j < face.size(); j++) {
      const int v_prev = face_verts[(face.size() + (j - 1)) % face.size()];
      const int v_curr = face_verts[j];
      const int v_next = face_verts[(j + 1) % face.size()];

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
