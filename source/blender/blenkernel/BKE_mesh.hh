/* SPDX-License-Identifier: GPL-2.0-or-later */

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
float3 poly_normal_calc(Span<float3> vert_positions, Span<int> poly_verts);

/**
 * Calculate tessellation into #MLoopTri which exist only for this purpose.
 */
void looptris_calc(Span<float3> vert_positions,
                   OffsetIndices<int> polys,
                   Span<int> corner_verts,
                   MutableSpan<MLoopTri> looptris);
/**
 * A version of #looptris_calc which takes pre-calculated polygon normals
 * (used to avoid having to calculate the face normal for NGON tessellation).
 *
 * \note Only use this function if normals have already been calculated, there is no need
 * to calculate normals just to use this function.
 */
void looptris_calc_with_normals(Span<float3> vert_positions,
                                OffsetIndices<int> polys,
                                Span<int> corner_verts,
                                Span<float3> poly_normals,
                                MutableSpan<MLoopTri> looptris);

/** Calculate the average position of the vertices in the polygon. */
float3 poly_center_calc(Span<float3> vert_positions, Span<int> poly_verts);

/** Calculate the surface area of the polygon described by the indexed vertices. */
float poly_area_calc(Span<float3> vert_positions, Span<int> poly_verts);

/** Calculate the angles at each of the polygons corners. */
void poly_angles_calc(Span<float3> vert_positions,
                      Span<int> poly_verts,
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
                        OffsetIndices<int> polys,
                        Span<int> corner_verts,
                        MutableSpan<float3> poly_normals);

/**
 * Calculate face and vertex normals directly into result arrays.
 *
 * \note Usually #Mesh::vert_normals() is the preferred way to access vertex normals,
 * since they may already be calculated and cached on the mesh.
 */
void normals_calc_poly_vert(Span<float3> vert_positions,
                            OffsetIndices<int> polys,
                            Span<int> corner_verts,
                            MutableSpan<float3> poly_normals,
                            MutableSpan<float3> vert_normals);

/**
 * Compute split normals, i.e. vertex normals associated with each poly (hence 'loop normals').
 * Useful to materialize sharp edges (or non-smooth faces) without actually modifying the geometry
 * (splitting edges).
 *
 * \param loop_to_poly_map: Optional pre-created map from corners to their polygon.
 * \param sharp_edges: Optional array of sharp edge tags, used to split the evaluated normals on
 * each side of the edge.
 */
void normals_calc_loop(Span<float3> vert_positions,
                       Span<MEdge> edges,
                       OffsetIndices<int> polys,
                       Span<int> corner_verts,
                       Span<int> corner_edges,
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
                             OffsetIndices<int> polys,
                             Span<int> corner_verts,
                             Span<int> corner_edges,
                             Span<float3> vert_normals,
                             Span<float3> poly_normals,
                             const bool *sharp_faces,
                             MutableSpan<bool> sharp_edges,
                             MutableSpan<float3> r_custom_loop_normals,
                             short (*r_clnors_data)[2]);

void normals_loop_custom_set_from_verts(Span<float3> vert_positions,
                                        Span<MEdge> edges,
                                        OffsetIndices<int> polys,
                                        Span<int> corner_verts,
                                        Span<int> corner_edges,
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
void edges_sharp_from_angle_set(OffsetIndices<int> polys,
                                Span<int> corner_verts,
                                Span<int> corner_edges,
                                Span<float3> poly_normals,
                                const bool *sharp_faces,
                                const float split_angle,
                                MutableSpan<bool> sharp_edges);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Topology Queries
 * \{ */

/**
 * Find the index of the next corner in the polygon, looping to the start if necessary.
 * The indices are into the entire corners array, not just the polygon's corners.
 */
inline int poly_corner_prev(const IndexRange poly, const int corner)
{
  return corner - 1 + (corner == poly.start()) * poly.size();
}

/**
 * Find the index of the previous corner in the polygon, looping to the end if necessary.
 * The indices are into the entire corners array, not just the polygon's corners.
 */
inline int poly_corner_next(const IndexRange poly, const int corner)
{
  if (corner == poly.last()) {
    return poly.start();
  }
  return corner + 1;
}

/**
 * Find the index of the corner in the polygon that uses the given vertex.
 * The index is into the entire corners array, not just the polygon's corners.
 */
inline int poly_find_corner_from_vert(const IndexRange poly,
                                      const Span<int> corner_verts,
                                      const int vert)
{
  return poly[corner_verts.slice(poly).first_index(vert)];
}

/**
 * Return the vertex indices on either side of the given vertex, ordered based on the winding
 * direction of the polygon. The vertex must be in the polygon.
 */
inline int2 poly_find_adjecent_verts(const IndexRange poly,
                                     const Span<int> corner_verts,
                                     const int vert)
{
  const int corner = poly_find_corner_from_vert(poly, corner_verts, vert);
  return {corner_verts[poly_corner_prev(poly, corner)],
          corner_verts[poly_corner_next(poly, corner)]};
}

/**
 * Return the index of the edge's vertex that is not the \a vert.
 * If neither edge vertex is equal to \a v, returns -1.
 */
inline int edge_other_vert(const MEdge &edge, const int vert)
{
  if (edge.v1 == vert) {
    return edge.v2;
  }
  if (edge.v2 == vert) {
    return edge.v1;
  }
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline Mesh Data Access
 * \{ */

}  // namespace blender::bke::mesh

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

inline blender::OffsetIndices<int> Mesh::polys() const
{
  return blender::Span(BKE_mesh_poly_offsets(this), this->totpoly + 1);
}
inline blender::Span<int> Mesh::poly_offsets() const
{
  if (this->totpoly == 0) {
    return {};
  }
  return {BKE_mesh_poly_offsets(this), this->totpoly + 1};
}
inline blender::MutableSpan<int> Mesh::poly_offsets_for_write()
{
  if (this->totpoly == 0) {
    return {};
  }
  return {BKE_mesh_poly_offsets_for_write(this), this->totpoly + 1};
}

inline blender::Span<int> Mesh::corner_verts() const
{
  return {BKE_mesh_corner_verts(this), this->totloop};
}
inline blender::MutableSpan<int> Mesh::corner_verts_for_write()
{
  return {BKE_mesh_corner_verts_for_write(this), this->totloop};
}

inline blender::Span<int> Mesh::corner_edges() const
{
  return {BKE_mesh_corner_edges(this), this->totloop};
}
inline blender::MutableSpan<int> Mesh::corner_edges_for_write()
{
  return {BKE_mesh_corner_edges_for_write(this), this->totloop};
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

/** \} */
