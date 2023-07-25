/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_mesh.h"

namespace blender::bke::mesh {

/* -------------------------------------------------------------------- */
/** \name Polygon Data Evaluation
 * \{ */

/** Calculate the up direction for the face, depending on its winding direction. */
float3 face_normal_calc(Span<float3> vert_positions, Span<int> face_verts);

/**
 * Calculate tessellation into #MLoopTri which exist only for this purpose.
 */
void looptris_calc(Span<float3> vert_positions,
                   OffsetIndices<int> faces,
                   Span<int> corner_verts,
                   MutableSpan<MLoopTri> looptris);
/**
 * A version of #looptris_calc which takes pre-calculated face normals
 * (used to avoid having to calculate the face normal for NGON tessellation).
 *
 * \note Only use this function if normals have already been calculated, there is no need
 * to calculate normals just to use this function.
 */
void looptris_calc_with_normals(Span<float3> vert_positions,
                                OffsetIndices<int> faces,
                                Span<int> corner_verts,
                                Span<float3> face_normals,
                                MutableSpan<MLoopTri> looptris);

void looptris_calc_face_indices(OffsetIndices<int> faces, MutableSpan<int> looptri_faces);

/** Calculate the average position of the vertices in the face. */
float3 face_center_calc(Span<float3> vert_positions, Span<int> face_verts);

/** Calculate the surface area of the face described by the indexed vertices. */
float face_area_calc(Span<float3> vert_positions, Span<int> face_verts);

/** Calculate the angles at each of the faces corners. */
void face_angles_calc(Span<float3> vert_positions,
                      Span<int> face_verts,
                      MutableSpan<float> angles);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Medium-Level Normals Calculation
 * \{ */

/**
 * Calculate face normals directly into a result array.
 *
 * \note Usually #Mesh::face_normals() is the preferred way to access face normals,
 * since they may already be calculated and cached on the mesh.
 */
void normals_calc_faces(Span<float3> vert_positions,
                        OffsetIndices<int> faces,
                        Span<int> corner_verts,
                        MutableSpan<float3> face_normals);

/**
 * Calculate face and vertex normals directly into result arrays.
 *
 * \note Usually #Mesh::vert_normals() is the preferred way to access vertex normals,
 * since they may already be calculated and cached on the mesh.
 */
void normals_calc_face_vert(Span<float3> vert_positions,
                            OffsetIndices<int> faces,
                            Span<int> corner_verts,
                            MutableSpan<float3> face_normals,
                            MutableSpan<float3> vert_normals);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Corner Normal Calculation
 * \{ */

/**
 * Combined with the automatically calculated face corner normal, this gives a dimensional
 * coordinate space used to convert normals between the "custom normal" #short2 representation and
 * a regular #float3 format.
 */
struct CornerNormalSpace {
  /** The automatically computed face corner normal, not including influence of custom normals. */
  float3 vec_lnor;
  /** Reference vector, orthogonal to #vec_lnor. */
  float3 vec_ref;
  /** Third vector, orthogonal to #vec_lnor and #vec_ref. */
  float3 vec_ortho;
  /** Reference angle around #vec_ortho, in [0, pi] range (0.0 marks space as invalid). */
  float ref_alpha;
  /** Reference angle around #vec_lnor, in [0, 2pi] range (0.0 marks space as invalid). */
  float ref_beta;
};

/**
 * Storage for corner fan coordinate spaces for an entire mesh.
 */
struct CornerNormalSpaceArray {
  /**
   * The normal coordinate spaces, potentially shared between multiple face corners in a smooth fan
   * connected to a vertex (and not per face corner). Depending on the mesh (the amount of sharing
   * / number of sharp edges / size of each fan), there may be many fewer spaces than face corners,
   * so they are stored in a separate array.
   */
  Array<CornerNormalSpace> spaces;

  /**
   * The index of the data in the #spaces array for each face corner (the array size is the
   * same as #Mesh::totloop). Rare -1 values define face corners without a coordinate space.
   */
  Array<int> corner_space_indices;

  /**
   * A map containing the face corners that make up each space,
   * in the order that they were processed (winding around a vertex).
   */
  Array<Array<int>> corners_by_space;
  /** Whether to create the above map when calculating normals. */
  bool create_corners_by_space = false;
};

short2 lnor_space_custom_normal_to_data(const CornerNormalSpace &lnor_space,
                                        const float3 &custom_lnor);

/**
 * Compute split normals, i.e. vertex normals associated with each face (hence 'loop normals').
 * Useful to materialize sharp edges (or non-smooth faces) without actually modifying the geometry
 * (splitting edges).
 *
 * \param loop_to_face_map: Optional pre-created map from corners to their face.
 * \param sharp_edges: Optional array of sharp edge tags, used to split the evaluated normals on
 * each side of the edge.
 * \param r_lnors_spacearr: Optional return data filled with information about the custom
 * normals spaces for each grouped fan of face corners.
 */
void normals_calc_loop(Span<float3> vert_positions,
                       Span<int2> edges,
                       OffsetIndices<int> faces,
                       Span<int> corner_verts,
                       Span<int> corner_edges,
                       Span<int> loop_to_face_map,
                       Span<float3> vert_normals,
                       Span<float3> face_normals,
                       const bool *sharp_edges,
                       const bool *sharp_faces,
                       bool use_split_normals,
                       float split_angle,
                       short2 *clnors_data,
                       CornerNormalSpaceArray *r_lnors_spacearr,
                       MutableSpan<float3> r_loop_normals);

void normals_loop_custom_set(Span<float3> vert_positions,
                             Span<int2> edges,
                             OffsetIndices<int> faces,
                             Span<int> corner_verts,
                             Span<int> corner_edges,
                             Span<float3> vert_normals,
                             Span<float3> face_normals,
                             const bool *sharp_faces,
                             MutableSpan<bool> sharp_edges,
                             MutableSpan<float3> r_custom_loop_normals,
                             MutableSpan<short2> r_clnors_data);

void normals_loop_custom_set_from_verts(Span<float3> vert_positions,
                                        Span<int2> edges,
                                        OffsetIndices<int> faces,
                                        Span<int> corner_verts,
                                        Span<int> corner_edges,
                                        Span<float3> vert_normals,
                                        Span<float3> face_normals,
                                        const bool *sharp_faces,
                                        MutableSpan<bool> sharp_edges,
                                        MutableSpan<float3> r_custom_vert_normals,
                                        MutableSpan<short2> r_clnors_data);

/**
 * Define sharp edges as needed to mimic 'autosmooth' from angle threshold.
 *
 * Used when defining an empty custom loop normals data layer,
 * to keep same shading as with auto-smooth!
 *
 * \param sharp_faces: Optional array used to mark specific faces for sharp shading.
 */
void edges_sharp_from_angle_set(OffsetIndices<int> faces,
                                Span<int> corner_verts,
                                Span<int> corner_edges,
                                Span<float3> face_normals,
                                const bool *sharp_faces,
                                const float split_angle,
                                MutableSpan<bool> sharp_edges);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Topology Queries
 * \{ */

/**
 * Find the index of the next corner in the face, looping to the start if necessary.
 * The indices are into the entire corners array, not just the face's corners.
 */
inline int face_corner_prev(const IndexRange face, const int corner)
{
  return corner - 1 + (corner == face.start()) * face.size();
}

/**
 * Find the index of the previous corner in the face, looping to the end if necessary.
 * The indices are into the entire corners array, not just the face's corners.
 */
inline int face_corner_next(const IndexRange face, const int corner)
{
  if (corner == face.last()) {
    return face.start();
  }
  return corner + 1;
}

/**
 * Find the index of the corner in the face that uses the given vertex.
 * The index is into the entire corners array, not just the face's corners.
 */
inline int face_find_corner_from_vert(const IndexRange face,
                                      const Span<int> corner_verts,
                                      const int vert)
{
  return face[corner_verts.slice(face).first_index(vert)];
}

/**
 * Return the vertex indices on either side of the given vertex, ordered based on the winding
 * direction of the face. The vertex must be in the face.
 */
inline int2 face_find_adjecent_verts(const IndexRange face,
                                     const Span<int> corner_verts,
                                     const int vert)
{
  const int corner = face_find_corner_from_vert(face, corner_verts, vert);
  return {corner_verts[face_corner_prev(face, corner)],
          corner_verts[face_corner_next(face, corner)]};
}

/**
 * Return the index of the edge's vertex that is not the \a vert.
 * If neither edge vertex is equal to \a v, returns -1.
 */
inline int edge_other_vert(const int2 &edge, const int vert)
{
  if (edge[0] == vert) {
    return edge[1];
  }
  if (edge[1] == vert) {
    return edge[0];
  }
  return -1;
}

/** \} */

}  // namespace blender::bke::mesh

/* -------------------------------------------------------------------- */
/** \name Inline Mesh Data Access
 * \{ */

inline blender::Span<blender::float3> Mesh::vert_positions() const
{
  return {static_cast<const blender::float3 *>(
              CustomData_get_layer_named(&this->vert_data, CD_PROP_FLOAT3, "position")),
          this->totvert};
}
inline blender::MutableSpan<blender::float3> Mesh::vert_positions_for_write()
{
  return {static_cast<blender::float3 *>(CustomData_get_layer_named_for_write(
              &this->vert_data, CD_PROP_FLOAT3, "position", this->totvert)),
          this->totvert};
}

inline blender::Span<blender::int2> Mesh::edges() const
{
  return {static_cast<const blender::int2 *>(
              CustomData_get_layer_named(&this->edge_data, CD_PROP_INT32_2D, ".edge_verts")),
          this->totedge};
}
inline blender::MutableSpan<blender::int2> Mesh::edges_for_write()
{
  return {static_cast<blender::int2 *>(CustomData_get_layer_named_for_write(
              &this->edge_data, CD_PROP_INT32_2D, ".edge_verts", this->totedge)),
          this->totedge};
}

inline blender::OffsetIndices<int> Mesh::faces() const
{
  return blender::Span(this->face_offset_indices, this->faces_num + 1);
}
inline blender::Span<int> Mesh::face_offsets() const
{
  if (this->faces_num == 0) {
    return {};
  }
  return {this->face_offset_indices, this->faces_num + 1};
}

inline blender::Span<int> Mesh::corner_verts() const
{
  return {static_cast<const int *>(
              CustomData_get_layer_named(&this->loop_data, CD_PROP_INT32, ".corner_vert")),
          this->totloop};
}
inline blender::MutableSpan<int> Mesh::corner_verts_for_write()
{
  return {static_cast<int *>(CustomData_get_layer_named_for_write(
              &this->loop_data, CD_PROP_INT32, ".corner_vert", this->totloop)),
          this->totloop};
}

inline blender::Span<int> Mesh::corner_edges() const
{
  return {static_cast<const int *>(
              CustomData_get_layer_named(&this->loop_data, CD_PROP_INT32, ".corner_edge")),
          this->totloop};
}
inline blender::MutableSpan<int> Mesh::corner_edges_for_write()
{
  return {static_cast<int *>(CustomData_get_layer_named_for_write(
              &this->loop_data, CD_PROP_INT32, ".corner_edge", this->totloop)),
          this->totloop};
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
