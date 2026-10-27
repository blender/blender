/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <algorithm>

#include "BLI_array.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_sys_types.hh"

#include "BKE_subdiv_eval.hh"

namespace blender {

struct Mesh;

namespace bke::subdiv {

/**
 * Describes how elements of the subdivided mesh correspond to the elements of the base mesh.
 * The subdivided elements are stored in groups for each base element:
 * - Vertices: The mesh starts with a vertex for every base mesh vertex, with the same index. Then
 *   `resolution - 2` vertices for every base mesh edge. Then the vertices inside of every base
 *   mesh face.
 * - Edges: First `resolution - 1` edges for every base mesh edge, then the edges inside of every
 *   base mesh face.
 * - Faces: The quads created for every base mesh face are contiguous.
 */
struct MeshLayout {
  /** Number of vertices along each side of the grid that a base mesh quad is subdivided into. */
  int resolution = 0;

  int verts_num = 0;
  int edges_num = 0;
  int faces_num = 0;
  int corners_num = 0;

  /** Index of the first vertex inside of a base mesh edge. */
  int edge_verts_start = 0;
  /** Index of the first vertex inside of a base mesh face. */
  int inner_verts_start = 0;
  /** Index of the first edge inside of a base mesh face. */
  int inner_edges_start = 0;

  /** Groups vertices inside of base mesh faces by face, relative to #inner_verts_start. */
  Array<int> inner_vert_offsets;
  /** Groups edges inside of base mesh faces by face, relative to #inner_edges_start. */
  Array<int> inner_edge_offsets;
  /** Groups subdivided faces by the base mesh face they come from. */
  Array<int> face_offsets;

  /** Vertices created inside of a base mesh edge, ordered from its first to second vertex. */
  IndexRange base_edge_verts(const int base_edge) const
  {
    const int verts_num = this->resolution - 2;
    return IndexRange(this->edge_verts_start + base_edge * verts_num, verts_num);
  }

  /** Edges created along a base mesh edge, ordered the edge's first vertex to its second. */
  IndexRange base_edge_edges(const int base_edge) const
  {
    const int edges_num = this->resolution - 1;
    return IndexRange(base_edge * edges_num, edges_num);
  }

  OffsetIndices<int> inner_verts_by_face() const
  {
    return this->inner_vert_offsets.as_span();
  }
  OffsetIndices<int> inner_edges_by_face() const
  {
    return this->inner_edge_offsets.as_span();
  }
  OffsetIndices<int> faces_by_base_face() const
  {
    return this->face_offsets.as_span();
  }

  /** The corners of the subdivided faces created for base mesh faces. */
  IndexRange base_face_corners(const IndexRange base_faces) const
  {
    const IndexRange faces = this->faces_by_base_face()[base_faces];
    return IndexRange(faces.start() * 4, faces.size() * 4);
  }
  IndexRange base_face_corners(const int base_face) const
  {
    const IndexRange faces = this->faces_by_base_face()[base_face];
    return IndexRange(faces.start() * 4, faces.size() * 4);
  }

  /**
   * A grain size for parallel loops over base mesh faces, so that each task processes a similar
   * number of subdivided elements regardless of the resolution.
   */
  int base_face_grain_size() const
  {
    return std::max(1, 4096 / (this->resolution * this->resolution));
  }
};

/**
 * Calculate how the elements of a mesh subdivided with the given resolution are indexed. See
 * #ToMeshSettings::resolution.
 */
MeshLayout build_mesh_layout(const Mesh &coarse_mesh, int resolution);

/**
 * Fill the vertex indices of the subdivided edges created along the selected base mesh edges, in
 * the order of the selection. See #MeshLayout::base_edge_edges().
 */
void fill_base_edge_edges(const MeshLayout &layout,
                          Span<int2> base_edges,
                          const IndexMask &selection,
                          MutableSpan<int2> edges);

/**
 * Fill the vertex indices of the subdivided edges inside of all base mesh faces.
 * \param edges: The edges inside of base mesh faces, see #MeshLayout::inner_edges_by_face().
 */
void fill_inner_edges(const MeshLayout &layout, const Mesh &base_mesh, MutableSpan<int2> edges);

/**
 * Fill vertex and edge indices of subdivided face corners of the given base mesh faces.
 * \param corner_verts, corner_edges: The corners for the base faces, see
 * #MeshLayout::base_face_corners(). Either may be empty to skip filling it.
 */
void fill_corner_verts_and_edges(const MeshLayout &layout,
                                 const Mesh &base_mesh,
                                 IndexRange base_faces,
                                 MutableSpan<int> corner_verts,
                                 MutableSpan<int> corner_edges);

/**
 * \param face_ptex_offset: ptex face coordinates of the corners created from the given base mesh
 * faces. See #face_ptex_offset_get().
 * \param corner_ptex_coords: The corners for the base faces, see #MeshLayout::base_face_corners().
 * \param face_base_corners: The subdivided faces for the base faces, as an index within the base
 * face. For quads that is the quadrant of the face that contains the subdivided face, for other
 * faces it corresponds to the ptex face. See #MeshLayout::faces_by_base_face().
 */
void fill_corner_ptex_coords(const MeshLayout &layout,
                             const Mesh &base_mesh,
                             Span<int> face_ptex_offset,
                             IndexRange base_faces,
                             MutableSpan<PtexCoord> corner_ptex_coords,
                             MutableSpan<int> face_base_corners);

/**
 * Fill ptex face coordinates of subdivided vertices inside the given base mesh faces.
 * \param face_ptex_offset: See #face_ptex_offset_get().
 * \param vert_ptex_coords: The vertices for the base faces, see
 * #MeshLayout::inner_verts_by_face().
 */
void fill_inner_vert_ptex_coords(const MeshLayout &layout,
                                 const Mesh &base_mesh,
                                 Span<int> face_ptex_offset,
                                 IndexRange base_faces,
                                 MutableSpan<PtexCoord> vert_ptex_coords);

/**
 * The ptex face coordinate of the vertex of a base mesh face corner, in that face.
 * \param face_ptex_offset: The first ptex face of the face, see #face_ptex_offset_get().
 * \param corner: The index of the corner within the face.
 */
PtexCoord face_corner_ptex_coord(IndexRange face, int face_ptex_offset, int corner);

/**
 * Fill the ptex face coordinates of the subdivided vertices inside of the edge of a base mesh face
 * corner (the edge from the corner's vertex to the next corner's vertex), in that face.
 * \param flip: Whether the corner's vertex is the edge's second vertex.
 * \param ptex_coords: Ordered like #MeshLayout::base_edge_verts().
 */
void fill_face_corner_edge_ptex_coords(const MeshLayout &layout,
                                       IndexRange face,
                                       int face_ptex_offset,
                                       int corner,
                                       bool flip,
                                       MutableSpan<PtexCoord> ptex_coords);

}  // namespace bke::subdiv
}  // namespace blender
