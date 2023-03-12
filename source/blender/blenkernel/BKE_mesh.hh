/* SPDX-License-Identifier: GPL-2.0-or-later. */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_mesh.h"

namespace blender::bke::mesh {

/* -------------------------------------------------------------------- */
/** \name Polygon Data Evaluation
 * \{ */

/** Calculate the up direction for the polygon, depending on its winding direction. */
float3 poly_normal_calc(Span<float3> vert_positions, Span<MLoop> poly_loops);

/**
 * Calculate tessellation into #MLoopTri which exist only for this purpose.
 */
void looptris_calc(Span<float3> vert_positions,
                   Span<MPoly> polys,
                   Span<MLoop> loops,
                   MutableSpan<MLoopTri> looptris);
/**
 * A version of #looptris_calc which takes pre-calculated polygon normals
 * (used to avoid having to calculate the face normal for NGON tessellation).
 *
 * \note Only use this function if normals have already been calculated, there is no need
 * to calculate normals just to use this function.
 */
void looptris_calc_with_normals(Span<float3> vert_positions,
                                Span<MPoly> polys,
                                Span<MLoop> loops,
                                Span<float3> poly_normals,
                                MutableSpan<MLoopTri> looptris);

/** Calculate the average position of the vertices in the polygon. */
float3 poly_center_calc(Span<float3> vert_positions, Span<MLoop> poly_loops);

/** Calculate the surface area of the polygon described by the indexed vertices. */
float poly_area_calc(Span<float3> vert_positions, Span<MLoop> poly_loops);

/** Calculate the angles at each of the polygons's corners. */
void poly_angles_calc(Span<float3> vert_positions,
                      Span<MLoop> poly_loops,
                      MutableSpan<float> angles);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Medium-Level Normals Calculation
 * \{ */

/**
 * Calculate face normals directly into a result array.
 *
 * \note Usually #Mesh::poly_normals() is the preferred way to access face normals,
 * since they may already be calculated and cached on the mesh.
 */
void normals_calc_polys(Span<float3> vert_positions,
                        Span<MPoly> polys,
                        Span<MLoop> loops,
                        MutableSpan<float3> poly_normals);

/**
 * Calculate face and vertex normals directly into result arrays.
 *
 * \note Usually #Mesh::vert_normals() is the preferred way to access vertex normals,
 * since they may already be calculated and cached on the mesh.
 */
void normals_calc_poly_vert(Span<float3> vert_positions,
                            Span<MPoly> polys,
                            Span<MLoop> loops,
                            MutableSpan<float3> poly_normals,
                            MutableSpan<float3> vert_normals);

/**
 * Compute split normals, i.e. vertex normals associated with each poly (hence 'loop normals').
 * Useful to materialize sharp edges (or non-smooth faces) without actually modifying the geometry
 * (splitting edges).
 *
 * \param loop_to_poly_map: Optional pre-created map from loops to their polygon.
 * \param sharp_edges: Optional array of sharp edge tags, used to split the evaluated normals on
 * each side of the edge.
 */
void normals_calc_loop(Span<float3> vert_positions,
                       Span<MEdge> edges,
                       Span<MPoly> polys,
                       Span<MLoop> loops,
                       Span<int> loop_to_poly_map,
                       Span<float3> vert_normals,
                       Span<float3> poly_normals,
                       const bool *sharp_edges,
                       const bool *sharp_faces,
                       bool use_split_normals,
                       float split_angle,
                       short (*clnors_data)[2],
                       MLoopNorSpaceArray *r_lnors_spacearr,
                       MutableSpan<float3> r_loop_normals);

void normals_loop_custom_set(Span<float3> vert_positions,
                             Span<MEdge> edges,
                             Span<MPoly> polys,
                             Span<MLoop> loops,
                             Span<float3> vert_normals,
                             Span<float3> poly_normals,
                             const bool *sharp_faces,
                             MutableSpan<bool> sharp_edges,
                             MutableSpan<float3> r_custom_loop_normals,
                             short (*r_clnors_data)[2]);

void normals_loop_custom_set_from_verts(Span<float3> vert_positions,
                                        Span<MEdge> edges,
                                        Span<MPoly> polys,
                                        Span<MLoop> loops,
                                        Span<float3> vert_normals,
                                        Span<float3> poly_normals,
                                        const bool *sharp_faces,
                                        MutableSpan<bool> sharp_edges,
                                        MutableSpan<float3> r_custom_vert_normals,
                                        short (*r_clnors_data)[2]);

/**
 * Define sharp edges as needed to mimic 'autosmooth' from angle threshold.
 *
 * Used when defining an empty custom loop normals data layer,
 * to keep same shading as with auto-smooth!
 *
 * \param sharp_faces: Optional array used to mark specific faces for sharp shading.
 */
void edges_sharp_from_angle_set(Span<MPoly> polys,
                                Span<MLoop> loops,
                                Span<float3> poly_normals,
                                const bool *sharp_faces,
                                const float split_angle,
                                MutableSpan<bool> sharp_edges);

}  // namespace blender::bke::mesh

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline Mesh Data Access
 * \{ */

inline blender::Span<blender::float3> Mesh::vert_positions() const
{
  return {reinterpret_cast<const blender::float3 *>(BKE_mesh_vert_positions(this)), this->totvert};
}
inline blender::MutableSpan<blender::float3> Mesh::vert_positions_for_write()
{
  return {reinterpret_cast<blender::float3 *>(BKE_mesh_vert_positions_for_write(this)),
          this->totvert};
}

inline blender::Span<MEdge> Mesh::edges() const
{
  return {BKE_mesh_edges(this), this->totedge};
}
inline blender::MutableSpan<MEdge> Mesh::edges_for_write()
{
  return {BKE_mesh_edges_for_write(this), this->totedge};
}

inline blender::Span<MPoly> Mesh::polys() const
{
  return {BKE_mesh_polys(this), this->totpoly};
}
inline blender::MutableSpan<MPoly> Mesh::polys_for_write()
{
  return {BKE_mesh_polys_for_write(this), this->totpoly};
}

inline blender::Span<MLoop> Mesh::loops() const
{
  return {BKE_mesh_loops(this), this->totloop};
}
inline blender::MutableSpan<MLoop> Mesh::loops_for_write()
{
  return {BKE_mesh_loops_for_write(this), this->totloop};
}

inline blender::Span<MDeformVert> Mesh::deform_verts() const
{
  const MDeformVert *dverts = BKE_mesh_deform_verts(this);
  if (!dverts) {
    return {};
  }
  return {dverts, this->totvert};
}
inline blender::MutableSpan<MDeformVert> Mesh::deform_verts_for_write()
{
  return {BKE_mesh_deform_verts_for_write(this), this->totvert};
}

inline blender::Span<blender::float3> Mesh::poly_normals() const
{
  return {reinterpret_cast<const blender::float3 *>(BKE_mesh_poly_normals_ensure(this)),
          this->totpoly};
}

inline blender::Span<blender::float3> Mesh::vert_normals() const
{
  return {reinterpret_cast<const blender::float3 *>(BKE_mesh_vert_normals_ensure(this)),
          this->totvert};
}

/** \} */
