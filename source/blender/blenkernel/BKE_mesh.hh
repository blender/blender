/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_index_mask_fwd.hh"
#include "BLI_offset_indices.hh"

#include "BKE_mesh.h"
#include "BKE_mesh_types.hh"

struct ModifierData;

namespace blender::bke {

enum class AttrDomain : int8_t;
class AttributeIDRef;

namespace mesh {
/* -------------------------------------------------------------------- */
/** \name Polygon Data Evaluation
 * \{ */

/** Calculate the up direction for the face, depending on its winding direction. */
float3 face_normal_calc(Span<float3> vert_positions, Span<int> face_verts);

void corner_tris_calc(Span<float3> vert_positions,
                      OffsetIndices<int> faces,
                      Span<int> corner_verts,
                      MutableSpan<int3> corner_tris);

/**
 * A version of #corner_tris_calc which takes pre-calculated face normals
 * (used to avoid having to calculate the face normal for NGON tessellation).
 *
 * \note Only use this function if normals have already been calculated, there is no need
 * to calculate normals just to use this function.
 */
void corner_tris_calc_with_normals(Span<float3> vert_positions,
                                   OffsetIndices<int> faces,
                                   Span<int> corner_verts,
                                   Span<float3> face_normals,
                                   MutableSpan<int3> corner_tris);

void corner_tris_calc_face_indices(OffsetIndices<int> faces, MutableSpan<int> tri_faces);

/**
 * Convert triangles encoded as face corner indices to triangles encoded as vertex indices.
 */
void vert_tris_from_corner_tris(Span<int> corner_verts,
                                Span<int3> corner_tris,
                                MutableSpan<int3> vert_tris);

/** Return the triangle's three edge indices they are real edges, otherwise -1. */
int3 corner_tri_get_real_edges(Span<int2> edges,
                               Span<int> corner_verts,
                               Span<int> corner_edges,
                               const int3 &corner_tri);

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
 * Calculate vertex normals directly into the result array.
 *
 * \note Vertex and face normals can be calculated at the same time with
 * #normals_calc_faces_and_verts, which can have performance benefits in some cases.
 *
 * \note Usually #Mesh::vert_normals() is the preferred way to access vertex normals,
 * since they may already be calculated and cached on the mesh.
 */
void normals_calc_verts(Span<float3> vert_positions,
                        OffsetIndices<int> faces,
                        Span<int> corner_verts,
                        GroupedSpan<int> vert_to_face_map,
                        Span<float3> face_normals,
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
   * same as #Mesh::corners_num). Rare -1 values define face corners without a coordinate space.
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

short2 corner_space_custom_normal_to_data(const CornerNormalSpace &lnor_space,
                                          const float3 &custom_lnor);

/**
 * Compute split normals, i.e. vertex normals associated with each face. Used to visualize sharp
 * edges (or non-smooth faces) without actually modifying the geometry (splitting edges).
 *
 * \param sharp_edges: Optional array of sharp edge tags, used to split the evaluated normals on
 * each side of the edge.
 * \param sharp_faces: Optional array of sharp face tags, used to split the evaluated normals on
 * the face's edges.
 * \param r_lnors_spacearr: Optional return data filled with information about the custom
 * normals spaces for each grouped fan of face corners.
 */
void normals_calc_corners(Span<float3> vert_positions,
                          Span<int2> edges,
                          OffsetIndices<int> faces,
                          Span<int> corner_verts,
                          Span<int> corner_edges,
                          Span<int> corner_to_face_map,
                          Span<float3> vert_normals,
                          Span<float3> face_normals,
                          Span<bool> sharp_edges,
                          Span<bool> sharp_faces,
                          const short2 *clnors_data,
                          CornerNormalSpaceArray *r_lnors_spacearr,
                          MutableSpan<float3> r_corner_normals);

/**
 * \param sharp_faces: Optional array used to mark specific faces for sharp shading.
 */
void normals_corner_custom_set(Span<float3> vert_positions,
                               Span<int2> edges,
                               OffsetIndices<int> faces,
                               Span<int> corner_verts,
                               Span<int> corner_edges,
                               Span<float3> vert_normals,
                               Span<float3> face_normals,
                               Span<bool> sharp_faces,
                               MutableSpan<bool> sharp_edges,
                               MutableSpan<float3> r_custom_corner_normals,
                               MutableSpan<short2> r_clnors_data);

/**
 * \param sharp_faces: Optional array used to mark specific faces for sharp shading.
 */
void normals_corner_custom_set_from_verts(Span<float3> vert_positions,
                                          Span<int2> edges,
                                          OffsetIndices<int> faces,
                                          Span<int> corner_verts,
                                          Span<int> corner_edges,
                                          Span<float3> vert_normals,
                                          Span<float3> face_normals,
                                          Span<bool> sharp_faces,
                                          MutableSpan<bool> sharp_edges,
                                          MutableSpan<float3> r_custom_vert_normals,
                                          MutableSpan<short2> r_clnors_data);

/**
 * Define sharp edges as needed to mimic 'autosmooth' from angle threshold.
 *
 * Used when defining an empty custom corner normals data layer,
 * to keep same shading as with auto-smooth!
 *
 * \param sharp_faces: Optional array used to mark specific faces for sharp shading.
 */
void edges_sharp_from_angle_set(OffsetIndices<int> faces,
                                Span<int> corner_verts,
                                Span<int> corner_edges,
                                Span<float3> face_normals,
                                Span<int> corner_to_face,
                                Span<bool> sharp_faces,
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
inline int2 face_find_adjacent_verts(const IndexRange face,
                                     const Span<int> corner_verts,
                                     const int vert)
{
  const int corner = face_find_corner_from_vert(face, corner_verts, vert);
  return {corner_verts[face_corner_prev(face, corner)],
          corner_verts[face_corner_next(face, corner)]};
}

/**
 * Return the number of triangles needed to tessellate a face with \a face_size corners.
 */
inline int face_triangles_num(const int face_size)
{
  BLI_assert(face_size > 2);
  return face_size - 2;
}

/**
 * Return the range of triangles that belong to the given face.
 */
inline IndexRange face_triangles_range(OffsetIndices<int> faces, int face_i)
{
  const IndexRange face = faces[face_i];
  /* This is the same as #poly_to_tri_count which is not included here. */
  const int start_triangle = face.start() - face_i * 2;
  return IndexRange(start_triangle, face_triangles_num(face.size()));
}

/**
 * Return the index of the edge's vertex that is not the \a vert.
 */
inline int edge_other_vert(const int2 edge, const int vert)
{
  BLI_assert(ELEM(vert, edge[0], edge[1]));
  BLI_assert(edge[0] >= 0);
  BLI_assert(edge[1] >= 0);
  /* Order is important to avoid overflow. */
  return (edge[0] - vert) + edge[1];
}

/** \} */

}  // namespace mesh

/** Create a mesh with no built-in attributes. */
Mesh *mesh_new_no_attributes(int verts_num, int edges_num, int faces_num, int corners_num);

/** Calculate edges from faces. */
void mesh_calc_edges(Mesh &mesh, bool keep_existing_edges, bool select_new_edges);

void mesh_flip_faces(Mesh &mesh, const IndexMask &selection);

void mesh_ensure_required_data_layers(Mesh &mesh);

/** Set mesh vertex normals to known-correct values, avoiding future lazy computation. */
void mesh_vert_normals_assign(Mesh &mesh, Span<float3> vert_normals);

/** Set mesh vertex normals to known-correct values, avoiding future lazy computation. */
void mesh_vert_normals_assign(Mesh &mesh, Vector<float3> vert_normals);

void mesh_smooth_set(Mesh &mesh, bool use_smooth, bool keep_sharp_edges = false);
void mesh_sharp_edges_set_from_angle(Mesh &mesh, float angle, bool keep_sharp_edges = false);

/**
 * Calculate edge visibility based on vertex visibility, hides an edge when either of its
 * vertices are hidden. */
void mesh_edge_hide_from_vert(Span<int2> edges, Span<bool> hide_vert, MutableSpan<bool> hide_edge);

/* Hide faces when any of their vertices are hidden. */
void mesh_face_hide_from_vert(OffsetIndices<int> faces,
                              Span<int> corner_verts,
                              Span<bool> hide_vert,
                              MutableSpan<bool> hide_poly);

/** Make edge and face visibility consistent with vertices. */
void mesh_hide_vert_flush(Mesh &mesh);
/** Make vertex and edge visibility consistent with faces. */
void mesh_hide_face_flush(Mesh &mesh);

/** Make edge and face selection consistent with vertices. */
void mesh_select_vert_flush(Mesh &mesh);
/** Make vertex and face selection consistent with edges. */
void mesh_select_edge_flush(Mesh &mesh);
/** Make vertex and edge selection consistent with faces. */
void mesh_select_face_flush(Mesh &mesh);

/** Set the default name when adding a color attribute if there is no default yet. */
void mesh_ensure_default_color_attribute_on_add(Mesh &mesh,
                                                const AttributeIDRef &id,
                                                AttrDomain domain,
                                                eCustomDataType data_type);

void mesh_data_update(Depsgraph &depsgraph,
                      const Scene &scene,
                      Object &ob,
                      const CustomData_MeshMasks &dataMask);

}  // namespace blender::bke
