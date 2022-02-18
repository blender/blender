/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_dual_mesh_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>("Keep Boundaries")
      .default_value(false)
      .description(
          "Keep non-manifold boundaries of the input mesh in place by avoiding the dual "
          "transformation there");
  b.add_output<decl::Geometry>("Dual Mesh");
}

enum class EdgeType : int8_t {
  Loose = 0,       /* No polygons connected to it. */
  Boundary = 1,    /* An edge connected to exactly one polygon. */
  Normal = 2,      /* A normal edge (connected to two polygons). */
  NonManifold = 3, /* An edge connected to more than two polygons. */
};

static EdgeType get_edge_type_with_added_neighbor(EdgeType old_type)
{
  switch (old_type) {
    case EdgeType::Loose:
      return EdgeType::Boundary;
    case EdgeType::Boundary:
      return EdgeType::Normal;
    case EdgeType::Normal:
    case EdgeType::NonManifold:
      return EdgeType::NonManifold;
  }
  BLI_assert_unreachable();
  return EdgeType::Loose;
}

enum class VertexType : int8_t {
  Loose = 0,       /* Either no edges connected or only loose edges connected. */
  Normal = 1,      /* A normal vertex. */
  Boundary = 2,    /* A vertex on a boundary edge. */
  NonManifold = 3, /* A vertex on a non-manifold edge. */
};

static VertexType get_vertex_type_with_added_neighbor(VertexType old_type)
{
  switch (old_type) {
    case VertexType::Loose:
      return VertexType::Normal;
    case VertexType::Normal:
      return VertexType::Boundary;
    case VertexType::Boundary:
    case VertexType::NonManifold:
      return VertexType::NonManifold;
  }
  BLI_assert_unreachable();
  return VertexType::Loose;
}

/* Copy only where vertex_types is 'normal'. If keep boundaries is selected, also copy from
 * boundary vertices. */
template<typename T>
static void copy_data_based_on_vertex_types(Span<T> data,
                                            MutableSpan<T> r_data,
                                            const Span<VertexType> vertex_types,
                                            const bool keep_boundaries)
{
  if (keep_boundaries) {
    int out_i = 0;
    for (const int i : data.index_range()) {
      if (ELEM(vertex_types[i], VertexType::Normal, VertexType::Boundary)) {
        r_data[out_i] = data[i];
        out_i++;
      }
    }
  }
  else {
    int out_i = 0;
    for (const int i : data.index_range()) {
      if (vertex_types[i] == VertexType::Normal) {
        r_data[out_i] = data[i];
        out_i++;
      }
    }
  }
}

template<typename T>
static void copy_data_based_on_pairs(Span<T> data,
                                     MutableSpan<T> r_data,
                                     const Span<std::pair<int, int>> new_to_old_map)
{
  for (const std::pair<int, int> &pair : new_to_old_map) {
    r_data[pair.first] = data[pair.second];
  }
}

/* Copy using the map. */
template<typename T>
static void copy_data_based_on_new_to_old_map(Span<T> data,
                                              MutableSpan<T> r_data,
                                              const Span<int> new_to_old_map)
{
  for (const int i : r_data.index_range()) {
    const int old_i = new_to_old_map[i];
    r_data[i] = data[old_i];
  }
}

/**
 * Transfers the attributes from the original mesh to the new mesh using the following logic:
 * - If the attribute was on the face domain it is now on the point domain, and this is true
 *   for all faces, so we can just copy these.
 * - If the attribute was on the vertex domain there are three cases:
 *   - It was a 'bad' vertex so it is not in the dual mesh, and we can just ignore it
 *   - It was a normal vertex so it has a corresponding face in the dual mesh to which we can
 *     transfer.
 *   - It was a boundary vertex so it has a corresponding face, if keep_boundaries is true.
 *     Otherwise we can just ignore it.
 * - If the attribute was on the edge domain we lookup for the new edges which edge it originated
 *   from using `new_to_old_edges_map`. We have to do it in this reverse order, because there can
 *   be more edges in the new mesh if keep boundaries is on.
 * - We do the same thing for face corners as we do for edges.
 *
 * Some of the vertices (on the boundary) in the dual mesh don't come from faces, but from edges or
 * vertices. For these the `boundary_vertex_to_relevant_face_map` is used, which maps them to the
 * closest face.
 */
static void transfer_attributes(
    const Map<AttributeIDRef, AttributeKind> &attributes,
    const Span<VertexType> vertex_types,
    const bool keep_boundaries,
    const Span<int> new_to_old_edges_map,
    const Span<int> new_to_old_face_corners_map,
    const Span<std::pair<int, int>> boundary_vertex_to_relevant_face_map,
    const GeometryComponent &src_component,
    GeometryComponent &dst_component)
{
  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    ReadAttributeLookup src_attribute = src_component.attribute_try_get_for_read(attribute_id);
    if (!src_attribute) {
      continue;
    }

    AttributeDomain out_domain;
    if (src_attribute.domain == ATTR_DOMAIN_FACE) {
      out_domain = ATTR_DOMAIN_POINT;
    }
    else if (src_attribute.domain == ATTR_DOMAIN_POINT) {
      out_domain = ATTR_DOMAIN_FACE;
    }
    else {
      /* Edges and Face Corners. */
      out_domain = src_attribute.domain;
    }
    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(
        src_attribute.varray.type());
    OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
        attribute_id, out_domain, data_type);

    if (!dst_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArray_Span<T> span{src_attribute.varray.typed<T>()};
      MutableSpan<T> dst_span = dst_attribute.as_span<T>();
      if (src_attribute.domain == ATTR_DOMAIN_FACE) {
        dst_span.take_front(span.size()).copy_from(span);
        if (keep_boundaries) {
          copy_data_based_on_pairs(span, dst_span, boundary_vertex_to_relevant_face_map);
        }
      }
      else if (src_attribute.domain == ATTR_DOMAIN_POINT) {
        copy_data_based_on_vertex_types(span, dst_span, vertex_types, keep_boundaries);
      }
      else if (src_attribute.domain == ATTR_DOMAIN_EDGE) {
        copy_data_based_on_new_to_old_map(span, dst_span, new_to_old_edges_map);
      }
      else {
        copy_data_based_on_new_to_old_map(span, dst_span, new_to_old_face_corners_map);
      }
    });
    dst_attribute.save();
  }
}

/**
 * Calculates the boundaries of the mesh. Boundary polygons are not computed since we don't need
 * them later on. We use the following definitions:
 * - An edge is on a boundary if it is connected to only one polygon.
 * - A vertex is on a boundary if it is on an edge on a boundary.
 */
static void calc_boundaries(const Mesh &mesh,
                            MutableSpan<VertexType> r_vertex_types,
                            MutableSpan<EdgeType> r_edge_types)
{
  BLI_assert(r_vertex_types.size() == mesh.totvert);
  BLI_assert(r_edge_types.size() == mesh.totedge);
  r_vertex_types.fill(VertexType::Loose);
  r_edge_types.fill(EdgeType::Loose);

  /* Add up the number of polys connected to each edge. */
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly = mesh.mpoly[i];
    for (const MLoop &loop : Span<MLoop>(&mesh.mloop[poly.loopstart], poly.totloop)) {
      r_edge_types[loop.e] = get_edge_type_with_added_neighbor(r_edge_types[loop.e]);
    }
  }

  /* Update vertices. */
  for (const int i : IndexRange(mesh.totedge)) {
    const EdgeType edge_type = r_edge_types[i];
    if (edge_type == EdgeType::Loose) {
      continue;
    }
    const MEdge &edge = mesh.medge[i];
    if (edge_type == EdgeType::Boundary) {
      r_vertex_types[edge.v1] = get_vertex_type_with_added_neighbor(r_vertex_types[edge.v1]);
      r_vertex_types[edge.v2] = get_vertex_type_with_added_neighbor(r_vertex_types[edge.v2]);
    }
    else if (edge_type >= EdgeType::NonManifold) {
      r_vertex_types[edge.v1] = VertexType::NonManifold;
      r_vertex_types[edge.v2] = VertexType::NonManifold;
    }
  }

  /* Normal verts are on a normal edge, and not on boundary edges or non-manifold edges. */
  for (const int i : IndexRange(mesh.totedge)) {
    const EdgeType edge_type = r_edge_types[i];
    if (edge_type == EdgeType::Normal) {
      const MEdge &edge = mesh.medge[i];
      if (r_vertex_types[edge.v1] == VertexType::Loose) {
        r_vertex_types[edge.v1] = VertexType::Normal;
      }
      if (r_vertex_types[edge.v2] == VertexType::Loose) {
        r_vertex_types[edge.v2] = VertexType::Normal;
      }
    }
  }
}

/**
 * Stores the indices of the polygons connected to each vertex.
 */
static void create_vertex_poly_map(const Mesh &mesh,
                                   MutableSpan<Vector<int>> r_vertex_poly_indices)
{
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly = mesh.mpoly[i];
    for (const MLoop &loop : Span<MLoop>(&mesh.mloop[poly.loopstart], poly.totloop)) {
      r_vertex_poly_indices[loop.v].append(i);
    }
  }
}

/**
 * Sorts the polygons connected to the given vertex based on polygon adjacency. The ordering is
 * so such that the normals point in the same way as the original mesh. If the vertex is a
 * boundary vertex, the first and last polygon have a boundary edge connected to the vertex. The
 * `r_shared_edges` array at index i is set to the index of the shared edge between the i-th and
 * `(i+1)-th` sorted polygon. Similarly the `r_sorted_corners` array at index i is set to the
 * corner in the i-th sorted polygon. If the polygons couldn't be sorted, `false` is returned.
 *
 * How the faces are sorted (see diagrams below):
 * (For this explanation we'll assume all faces are oriented clockwise)
 * (The vertex whose connected polygons we need to sort is "v0")
 *
 * \code{.unparsed}
 *     Normal case:                    Boundary Vertex case:
 *       v1 ----- v2 ----- v3              |       |             |
 *       |   f3   |   f0   |               v2 ---- v4 --------- v3---
 *       |        |        |               |      /          ,-' |
 *       v8 ----- v0 ----- v4              | f0  /   f1   ,-'    |
 *       |   f2   |   f1   |               |    /      ,-'       |
 *       |        |        |               |   /    ,-'          |
 *       v7 ----- v6 ----- v5              |  /  ,-'     f2      |
 *                                         | /,-'                |
 *                                         v0 ------------------ v1---
 * \endcode
 *
 * - First we get the two corners of each face that have an edge which contains v0. A corner is
 *   simply a vertex followed by an edge. In this case for the face "f0" for example, we'd end up
 *   with the corners (v: v4, e: v4<->v0) and (v: v0, e: v0<->v2). Note that if the face was
 *   oriented counter-clockwise we'd end up with the corners (v: v0, e: v0<->v4) and (v: v2, e:
 *   v0<->v2) instead.
 * - Then we need to choose one polygon as our first. If "v0" is not on a boundary we can just
 *   choose any polygon. If it is on a boundary some more care needs to be taken. Here we need to
 *   pick a polygon which lies on the boundary (in the diagram either f0 or f2). To choose between
 *   the two we need the next step.
 * - In the normal case we use this polygon to set `shared_edge_i` which indicates the index of the
 *   shared edge between this polygon and the next one. There are two possible choices: v0<->v4 and
 *   v2<->v0. To choose we look at the corners. Since the edge v0<->v2 lies on the corner which has
 *   v0, we set `shared_edge_i` to the other edge (v0<->v4), such that the next face will be "f1"
 *   which is the next face in clockwise order.
 * - In the boundary vertex case, we do something similar, but we are also forced to choose the
 *   edge which is not on the boundary. If this doesn't line up with orientation of the polygon, we
 *   know we'll need to choose the other boundary polygon as our first polygon. If the orientations
 *   don't line up there as well, it means that the mesh normals are not consistent, and we just
 *   have to force an orientation for ourselves. (Imagine if f0 is oriented counter-clockwise and
 *   f2 is oriented clockwise for example)
 * - Next comes a loop where we look at the other faces and find the one which has the shared
 *   edge. Then we set the next shared edge to the other edge on the polygon connected to "v0", and
 *   continue. Because of the way we've chosen the first shared edge the order of the faces will
 *   have the same orientation as that of the first polygon.
 *   (In this case we'd have f0 -> f1 -> f2 -> f3 which also goes around clockwise).
 * - Every time we determine a shared edge, we can also add a corner to `r_sorted_corners`. This
 *   will simply be the corner which doesn't contain the shared edge.
 * - Finally if we are in the normal case we also need to add the last "shared edge" to close the
 *   loop.
 */
static bool sort_vertex_polys(const Mesh &mesh,
                              const int vertex_index,
                              const bool boundary_vertex,
                              const Span<EdgeType> edge_types,
                              MutableSpan<int> connected_polygons,
                              MutableSpan<int> r_shared_edges,
                              MutableSpan<int> r_sorted_corners)
{
  if (connected_polygons.size() <= 2 && (!boundary_vertex || connected_polygons.size() == 0)) {
    return true;
  }

  /* For each polygon store the two corners whose edge contains the vertex. */
  Array<std::pair<int, int>> poly_vertex_corners(connected_polygons.size());
  for (const int i : connected_polygons.index_range()) {
    const MPoly &poly = mesh.mpoly[connected_polygons[i]];
    bool first_edge_done = false;
    for (const int loop_index : IndexRange(poly.loopstart, poly.totloop)) {
      const MLoop &loop = mesh.mloop[loop_index];
      if (mesh.medge[loop.e].v1 == vertex_index || mesh.medge[loop.e].v2 == vertex_index) {
        if (!first_edge_done) {
          poly_vertex_corners[i].first = loop_index;
          first_edge_done = true;
        }
        else {
          poly_vertex_corners[i].second = loop_index;
          break;
        }
      }
    }
  }

  int shared_edge_i = -1;
  /* Determine first polygon and orientation. For now the orientation of the whole loop depends
   * on the one polygon we chose as first. It's probably not worth it to check every polygon in
   * the loop to determine the 'average' orientation. */
  if (boundary_vertex) {
    /* Our first polygon needs to be one which has a boundary edge. */
    for (const int i : connected_polygons.index_range()) {
      const MLoop &first_loop = mesh.mloop[poly_vertex_corners[i].first];
      const MLoop &second_loop = mesh.mloop[poly_vertex_corners[i].second];
      if (edge_types[first_loop.e] == EdgeType::Boundary && first_loop.v == vertex_index) {
        shared_edge_i = second_loop.e;
        r_sorted_corners[0] = poly_vertex_corners[i].first;
        std::swap(connected_polygons[i], connected_polygons[0]);
        std::swap(poly_vertex_corners[i], poly_vertex_corners[0]);
        break;
      }
      if (edge_types[second_loop.e] == EdgeType::Boundary && second_loop.v == vertex_index) {
        shared_edge_i = first_loop.e;
        r_sorted_corners[0] = poly_vertex_corners[i].second;
        std::swap(connected_polygons[i], connected_polygons[0]);
        std::swap(poly_vertex_corners[i], poly_vertex_corners[0]);
        break;
      }
    }
    if (shared_edge_i == -1) {
      /* The rotation is inconsistent between the two polygons on the boundary. Just choose one
       * of the polygon's orientation. */
      for (const int i : connected_polygons.index_range()) {
        const MLoop &first_loop = mesh.mloop[poly_vertex_corners[i].first];
        const MLoop &second_loop = mesh.mloop[poly_vertex_corners[i].second];
        if (edge_types[first_loop.e] == EdgeType::Boundary) {
          shared_edge_i = second_loop.e;
          r_sorted_corners[0] = poly_vertex_corners[i].first;
          std::swap(connected_polygons[i], connected_polygons[0]);
          std::swap(poly_vertex_corners[i], poly_vertex_corners[0]);
          break;
        }
        if (edge_types[second_loop.e] == EdgeType::Boundary) {
          shared_edge_i = first_loop.e;
          r_sorted_corners[0] = poly_vertex_corners[i].second;
          std::swap(connected_polygons[i], connected_polygons[0]);
          std::swap(poly_vertex_corners[i], poly_vertex_corners[0]);
          break;
        }
      }
    }
  }
  else {
    /* Any polygon can be the first. Just need to check the orientation. */
    const MLoop &first_loop = mesh.mloop[poly_vertex_corners[0].first];
    const MLoop &second_loop = mesh.mloop[poly_vertex_corners[0].second];
    if (first_loop.v == vertex_index) {
      shared_edge_i = second_loop.e;
      r_sorted_corners[0] = poly_vertex_corners[0].first;
    }
    else {
      r_sorted_corners[0] = poly_vertex_corners[0].second;
      shared_edge_i = first_loop.e;
    }
  }
  BLI_assert(shared_edge_i != -1);

  for (const int i : IndexRange(connected_polygons.size() - 1)) {
    r_shared_edges[i] = shared_edge_i;

    /* Look at the other polys to see if it has this shared edge. */
    int j = i + 1;
    for (; j < connected_polygons.size(); ++j) {
      const MLoop &first_loop = mesh.mloop[poly_vertex_corners[j].first];
      const MLoop &second_loop = mesh.mloop[poly_vertex_corners[j].second];
      if (first_loop.e == shared_edge_i) {
        r_sorted_corners[i + 1] = poly_vertex_corners[j].first;
        shared_edge_i = second_loop.e;
        break;
      }
      if (second_loop.e == shared_edge_i) {
        r_sorted_corners[i + 1] = poly_vertex_corners[j].second;
        shared_edge_i = first_loop.e;
        break;
      }
    }
    if (j == connected_polygons.size()) {
      /* The vertex is not manifold because the polygons around the vertex don't form a loop, and
       * hence can't be sorted. */
      return false;
    }

    std::swap(connected_polygons[i + 1], connected_polygons[j]);
    std::swap(poly_vertex_corners[i + 1], poly_vertex_corners[j]);
  }

  if (!boundary_vertex) {
    /* Shared edge between first and last polygon. */
    r_shared_edges.last() = shared_edge_i;
  }
  return true;
}

/**
 * Get the edge on the poly that contains the given vertex and is a boundary edge.
 */
static void boundary_edge_on_poly(const MPoly &poly,
                                  const Mesh &mesh,
                                  const int vertex_index,
                                  const Span<EdgeType> edge_types,
                                  int &r_edge)
{
  for (const MLoop &loop : Span<MLoop>(&mesh.mloop[poly.loopstart], poly.totloop)) {
    if (edge_types[loop.e] == EdgeType::Boundary) {
      const MEdge &edge = mesh.medge[loop.e];
      if (edge.v1 == vertex_index || edge.v2 == vertex_index) {
        r_edge = loop.e;
        return;
      }
    }
  }
}

/**
 * Get the two edges on the poly that contain the given vertex and are boundary edges. The
 * orientation of the poly is taken into account.
 */
static void boundary_edges_on_poly(const MPoly &poly,
                                   const Mesh &mesh,
                                   const int vertex_index,
                                   const Span<EdgeType> edge_types,
                                   int &r_edge1,
                                   int &r_edge2)
{
  bool edge1_done = false;
  /* This is set to true if the order in which we encounter the two edges is inconsistent with the
   * orientation of the polygon. */
  bool needs_swap = false;
  for (const MLoop &loop : Span<MLoop>(&mesh.mloop[poly.loopstart], poly.totloop)) {
    if (edge_types[loop.e] == EdgeType::Boundary) {
      const MEdge &edge = mesh.medge[loop.e];
      if (edge.v1 == vertex_index || edge.v2 == vertex_index) {
        if (edge1_done) {
          if (needs_swap) {
            r_edge2 = r_edge1;
            r_edge1 = loop.e;
          }
          else {
            r_edge2 = loop.e;
          }
          return;
        }
        r_edge1 = loop.e;
        edge1_done = true;
        if (loop.v == vertex_index) {
          needs_swap = true;
        }
      }
    }
  }
}

static void add_edge(const Mesh &mesh,
                     const int old_edge_i,
                     const int v1,
                     const int v2,
                     Vector<int> &new_to_old_edges_map,
                     Vector<MEdge> &new_edges,
                     Vector<int> &loop_edges)
{
  MEdge new_edge = MEdge(mesh.medge[old_edge_i]);
  new_edge.v1 = v1;
  new_edge.v2 = v2;
  const int new_edge_i = new_edges.size();
  new_to_old_edges_map.append(old_edge_i);
  new_edges.append(new_edge);
  loop_edges.append(new_edge_i);
}

/* Returns true if the vertex is connected only to the two polygons and is not on the boundary. */
static bool vertex_needs_dissolving(const int vertex,
                                    const int first_poly_index,
                                    const int second_poly_index,
                                    const Span<VertexType> vertex_types,
                                    const Span<Vector<int>> vertex_poly_indices)
{
  /* Order is guaranteed to be the same because 2poly verts that are not on the boundary are
   * ignored in `sort_vertex_polys`. */
  return (vertex_types[vertex] != VertexType::Boundary &&
          vertex_poly_indices[vertex].size() == 2 &&
          vertex_poly_indices[vertex][0] == first_poly_index &&
          vertex_poly_indices[vertex][1] == second_poly_index);
}

/**
 * Finds 'normal' vertices which are connected to only two polygons and marks them to not be
 * used in the data-structures derived from the mesh. For each pair of polygons which has such a
 * vertex, an edge is created for the dual mesh between the centers of those two polygons. All
 * edges in the input mesh which contain such a vertex are marked as 'done' to prevent duplicate
 * edges being created. (See T94144)
 */
static void dissolve_redundant_verts(const Mesh &mesh,
                                     const Span<Vector<int>> vertex_poly_indices,
                                     MutableSpan<VertexType> vertex_types,
                                     MutableSpan<int> old_to_new_edges_map,
                                     Vector<MEdge> &new_edges,
                                     Vector<int> &new_to_old_edges_map)
{
  for (const int vert_i : IndexRange(mesh.totvert)) {
    if (vertex_poly_indices[vert_i].size() != 2 || vertex_types[vert_i] != VertexType::Normal) {
      continue;
    }
    const int first_poly_index = vertex_poly_indices[vert_i][0];
    const int second_poly_index = vertex_poly_indices[vert_i][1];
    const int new_edge_index = new_edges.size();
    bool edge_created = false;
    const MPoly &poly = mesh.mpoly[first_poly_index];
    for (const MLoop &loop : Span<MLoop>(&mesh.mloop[poly.loopstart], poly.totloop)) {
      const MEdge &edge = mesh.medge[loop.e];
      const int v1 = edge.v1;
      const int v2 = edge.v2;
      bool mark_edge = false;
      if (vertex_needs_dissolving(
              v1, first_poly_index, second_poly_index, vertex_types, vertex_poly_indices)) {
        /* This vertex is now 'removed' and should be ignored elsewhere. */
        vertex_types[v1] = VertexType::Loose;
        mark_edge = true;
      }
      if (vertex_needs_dissolving(
              v2, first_poly_index, second_poly_index, vertex_types, vertex_poly_indices)) {
        /* This vertex is now 'removed' and should be ignored elsewhere. */
        vertex_types[v2] = VertexType::Loose;
        mark_edge = true;
      }
      if (mark_edge) {
        if (!edge_created) {
          MEdge new_edge = MEdge(edge);
          /* The vertex indices in the dual mesh are the polygon indices of the input mesh. */
          new_edge.v1 = first_poly_index;
          new_edge.v2 = second_poly_index;
          new_to_old_edges_map.append(loop.e);
          new_edges.append(new_edge);
          edge_created = true;
        }
        old_to_new_edges_map[loop.e] = new_edge_index;
      }
    }
  }
}

/**
 * Calculate the barycentric dual of a mesh. The dual is only "dual" in terms of connectivity,
 * i.e. applying the function twice will give the same vertices, edges, and faces, but not the
 * same positions. When the option "Keep Boundaries" is selected the connectivity is no
 * longer dual.
 *
 * For the dual mesh of a manifold input mesh:
 * - The vertices are at the centers of the faces of the input mesh.
 * - The edges connect the two vertices created from the two faces next to the edge in the input
 *   mesh.
 * - The faces are at the vertices of the input mesh.
 *
 * Some special cases are needed for boundaries and non-manifold geometry.
 */
static void calc_dual_mesh(GeometrySet &geometry_set,
                           const MeshComponent &in_component,
                           const bool keep_boundaries)
{
  const Mesh &mesh_in = *in_component.get_for_read();

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_MESH}, GEO_COMPONENT_TYPE_MESH, false, attributes);

  Array<VertexType> vertex_types(mesh_in.totvert);
  Array<EdgeType> edge_types(mesh_in.totedge);
  calc_boundaries(mesh_in, vertex_types, edge_types);
  /* Stores the indices of the polygons connected to the vertex. Because the polygons are looped
   * over in order of their indices, the polygon's indices will be sorted in ascending order.
   (This can change once they are sorted using `sort_vertex_polys`). */
  Array<Vector<int>> vertex_poly_indices(mesh_in.totvert);
  Array<Array<int>> vertex_shared_edges(mesh_in.totvert);
  Array<Array<int>> vertex_corners(mesh_in.totvert);
  create_vertex_poly_map(mesh_in, vertex_poly_indices);
  threading::parallel_for(vertex_poly_indices.index_range(), 512, [&](IndexRange range) {
    for (const int i : range) {
      if (vertex_types[i] == VertexType::Loose || vertex_types[i] >= VertexType::NonManifold ||
          (!keep_boundaries && vertex_types[i] == VertexType::Boundary)) {
        /* Bad vertex that we can't work with. */
        continue;
      }
      MutableSpan<int> loop_indices = vertex_poly_indices[i];
      Array<int> sorted_corners(loop_indices.size());
      bool vertex_ok = true;
      if (vertex_types[i] == VertexType::Normal) {
        Array<int> shared_edges(loop_indices.size());
        vertex_ok = sort_vertex_polys(
            mesh_in, i, false, edge_types, loop_indices, shared_edges, sorted_corners);
        vertex_shared_edges[i] = shared_edges;
      }
      else {
        Array<int> shared_edges(loop_indices.size() - 1);
        vertex_ok = sort_vertex_polys(
            mesh_in, i, true, edge_types, loop_indices, shared_edges, sorted_corners);
        vertex_shared_edges[i] = shared_edges;
      }
      if (!vertex_ok) {
        /* The sorting failed which means that the vertex is non-manifold and should be ignored
         * further on. */
        vertex_types[i] = VertexType::NonManifold;
        continue;
      }
      vertex_corners[i] = sorted_corners;
    }
  });

  Vector<float3> vertex_positions(mesh_in.totpoly);
  for (const int i : IndexRange(mesh_in.totpoly)) {
    const MPoly poly = mesh_in.mpoly[i];
    BKE_mesh_calc_poly_center(
        &poly, &mesh_in.mloop[poly.loopstart], mesh_in.mvert, vertex_positions[i]);
  }

  Array<int> boundary_edge_midpoint_index;
  if (keep_boundaries) {
    /* Only initialize when we actually need it. */
    boundary_edge_midpoint_index.reinitialize(mesh_in.totedge);
    /* We need to add vertices at the centers of boundary edges. */
    for (const int i : IndexRange(mesh_in.totedge)) {
      if (edge_types[i] == EdgeType::Boundary) {
        float3 mid;
        const MEdge &edge = mesh_in.medge[i];
        mid_v3_v3v3(mid, mesh_in.mvert[edge.v1].co, mesh_in.mvert[edge.v2].co);
        boundary_edge_midpoint_index[i] = vertex_positions.size();
        vertex_positions.append(mid);
      }
    }
  }

  Vector<int> loop_lengths;
  Vector<int> loops;
  Vector<int> loop_edges;
  Vector<MEdge> new_edges;
  /* These are used to transfer attributes. */
  Vector<int> new_to_old_face_corners_map;
  Vector<int> new_to_old_edges_map;
  /* Stores the index of the vertex in the dual and the face it should get the attribute from. */
  Vector<std::pair<int, int>> boundary_vertex_to_relevant_face_map;
  /* Since each edge in the dual (except the ones created with keep boundaries) comes from
   * exactly one edge in the original, we can use this array to keep track of whether it still
   * needs to be created or not. If it's not -1 it gives the index in `new_edges` of the dual
   * edge. The edges coming from preserving the boundaries only get added once anyway, so we
   * don't need a hash-map for that. */
  Array<int> old_to_new_edges_map(mesh_in.totedge);
  old_to_new_edges_map.fill(-1);

  /* This is necessary to prevent duplicate edges from being created, but will likely not do
   * anything for most meshes. */
  dissolve_redundant_verts(mesh_in,
                           vertex_poly_indices,
                           vertex_types,
                           old_to_new_edges_map,
                           new_edges,
                           new_to_old_edges_map);

  for (const int i : IndexRange(mesh_in.totvert)) {
    if (vertex_types[i] == VertexType::Loose || vertex_types[i] >= VertexType::NonManifold ||
        (!keep_boundaries && vertex_types[i] == VertexType::Boundary)) {
      /* Bad vertex that we can't work with. */
      continue;
    }

    Vector<int> loop_indices = vertex_poly_indices[i];
    Span<int> shared_edges = vertex_shared_edges[i];
    Span<int> sorted_corners = vertex_corners[i];
    if (vertex_types[i] == VertexType::Normal) {
      if (loop_indices.size() <= 2) {
        /* We can't make a polygon from 2 vertices. */
        continue;
      }

      /* Add edges in the loop. */
      for (const int i : shared_edges.index_range()) {
        const int old_edge_i = shared_edges[i];
        if (old_to_new_edges_map[old_edge_i] == -1) {
          /* This edge has not been created yet. */
          MEdge new_edge = MEdge(mesh_in.medge[old_edge_i]);
          new_edge.v1 = loop_indices[i];
          new_edge.v2 = loop_indices[(i + 1) % loop_indices.size()];
          new_to_old_edges_map.append(old_edge_i);
          old_to_new_edges_map[old_edge_i] = new_edges.size();
          new_edges.append(new_edge);
        }
        loop_edges.append(old_to_new_edges_map[old_edge_i]);
      }

      new_to_old_face_corners_map.extend(sorted_corners);
    }
    else {
      /**
       * The code handles boundary vertices like the vertex marked "V" in the diagram below.
       * The first thing that happens is ordering the faces f1,f2 and f3 (stored in
       * loop_indices), together with their shared edges e3 and e4 (which get stored in
       * shared_edges). The ordering could end up being clockwise or counterclockwise, for this
       * we'll assume that the ordering f1->f2->f3 is chosen. After that we add the edges in
       * between the polygons, in this case the edges f1--f2, and f2--f3. Now we need to merge
       * these with the boundary edges e1 and e2. To do this we create an edge from f3 to the
       * midpoint of e2 (computed in a previous step), from this midpoint to V, from V to the
       * midpoint of e1 and from the midpoint of e1 to f1.
       *
       * \code{.unparsed}
       *       |       |             |                    |       |            |
       *       v2 ---- v3 --------- v4---                 v2 ---- v3 -------- v4---
       *       | f3   /          ,-' |                    |      /          ,-'|
       *       |     /   f2   ,-'    |                    |     /        ,-'   |
       *    e2 |    /e3    ,-' e4    |       ====>       M1-f3-/--f2-.,-'      |
       *       |   /    ,-'          |       ====>        |   /    ,-'\        |
       *       |  /  ,-'     f1      |                    |  /  ,-'    f1      |
       *       | /,-'                |                    | /,-'        |      |
       *       V-------------------- v5---                V------------M2----- v5---
       * \endcode
       */

      /* Add the edges in between the polys. */
      for (const int i : shared_edges.index_range()) {
        const int old_edge_i = shared_edges[i];
        if (old_to_new_edges_map[old_edge_i] == -1) {
          /* This edge has not been created yet. */
          MEdge new_edge = MEdge(mesh_in.medge[old_edge_i]);
          new_edge.v1 = loop_indices[i];
          new_edge.v2 = loop_indices[i + 1];
          new_to_old_edges_map.append(old_edge_i);
          old_to_new_edges_map[old_edge_i] = new_edges.size();
          new_edges.append(new_edge);
        }
        loop_edges.append(old_to_new_edges_map[old_edge_i]);
      }

      new_to_old_face_corners_map.extend(sorted_corners);

      /* Add the vertex and the midpoints of the two boundary edges to the loop. */

      /* Get the boundary edges. */
      int edge1;
      int edge2;
      if (loop_indices.size() >= 2) {
        /* The first boundary edge is at the end of the chain of polygons. */
        boundary_edge_on_poly(mesh_in.mpoly[loop_indices.last()], mesh_in, i, edge_types, edge1);
        boundary_edge_on_poly(mesh_in.mpoly[loop_indices.first()], mesh_in, i, edge_types, edge2);
      }
      else {
        /* If there is only one polygon both edges are in that polygon. */
        boundary_edges_on_poly(
            mesh_in.mpoly[loop_indices[0]], mesh_in, i, edge_types, edge1, edge2);
      }

      const int last_face_center = loop_indices.last();
      loop_indices.append(boundary_edge_midpoint_index[edge1]);
      new_to_old_face_corners_map.append(sorted_corners.last());
      const int first_midpoint = loop_indices.last();
      if (old_to_new_edges_map[edge1] == -1) {
        add_edge(mesh_in,
                 edge1,
                 last_face_center,
                 first_midpoint,
                 new_to_old_edges_map,
                 new_edges,
                 loop_edges);
        old_to_new_edges_map[edge1] = new_edges.size() - 1;
        boundary_vertex_to_relevant_face_map.append(std::pair(first_midpoint, last_face_center));
      }
      else {
        loop_edges.append(old_to_new_edges_map[edge1]);
      }
      loop_indices.append(vertex_positions.size());
      /* This is sort of arbitrary, but interpolating would be a lot harder to do. */
      new_to_old_face_corners_map.append(sorted_corners.first());
      boundary_vertex_to_relevant_face_map.append(
          std::pair(loop_indices.last(), last_face_center));
      vertex_positions.append(mesh_in.mvert[i].co);
      const int boundary_vertex = loop_indices.last();
      add_edge(mesh_in,
               edge1,
               first_midpoint,
               boundary_vertex,
               new_to_old_edges_map,
               new_edges,
               loop_edges);

      loop_indices.append(boundary_edge_midpoint_index[edge2]);
      new_to_old_face_corners_map.append(sorted_corners.first());
      const int second_midpoint = loop_indices.last();
      add_edge(mesh_in,
               edge2,
               boundary_vertex,
               second_midpoint,
               new_to_old_edges_map,
               new_edges,
               loop_edges);

      if (old_to_new_edges_map[edge2] == -1) {
        const int first_face_center = loop_indices.first();
        add_edge(mesh_in,
                 edge2,
                 second_midpoint,
                 first_face_center,
                 new_to_old_edges_map,
                 new_edges,
                 loop_edges);
        old_to_new_edges_map[edge2] = new_edges.size() - 1;
        boundary_vertex_to_relevant_face_map.append(std::pair(second_midpoint, first_face_center));
      }
      else {
        loop_edges.append(old_to_new_edges_map[edge2]);
      }
    }

    loop_lengths.append(loop_indices.size());
    for (const int j : loop_indices) {
      loops.append(j);
    }
  }
  Mesh *mesh_out = BKE_mesh_new_nomain(
      vertex_positions.size(), new_edges.size(), 0, loops.size(), loop_lengths.size());
  MeshComponent out_component;
  out_component.replace(mesh_out, GeometryOwnershipType::Editable);
  transfer_attributes(attributes,
                      vertex_types,
                      keep_boundaries,
                      new_to_old_edges_map,
                      new_to_old_face_corners_map,
                      boundary_vertex_to_relevant_face_map,
                      in_component,
                      out_component);

  int loop_start = 0;
  for (const int i : IndexRange(mesh_out->totpoly)) {
    mesh_out->mpoly[i].loopstart = loop_start;
    mesh_out->mpoly[i].totloop = loop_lengths[i];
    loop_start += loop_lengths[i];
  }
  for (const int i : IndexRange(mesh_out->totloop)) {
    mesh_out->mloop[i].v = loops[i];
    mesh_out->mloop[i].e = loop_edges[i];
  }
  for (const int i : IndexRange(mesh_out->totvert)) {
    copy_v3_v3(mesh_out->mvert[i].co, vertex_positions[i]);
  }
  memcpy(mesh_out->medge, new_edges.data(), sizeof(MEdge) * new_edges.size());
  BKE_mesh_normals_tag_dirty(mesh_out);
  geometry_set.replace_mesh(mesh_out);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
  const bool keep_boundaries = params.extract_input<bool>("Keep Boundaries");
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_mesh()) {
      const MeshComponent &component = *geometry_set.get_component_for_read<MeshComponent>();
      calc_dual_mesh(geometry_set, component, keep_boundaries);
    }
  });
  params.set_output("Dual Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_dual_mesh_cc

void register_node_type_geo_dual_mesh()
{
  namespace file_ns = blender::nodes::node_geo_dual_mesh_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_DUAL_MESH, "Dual Mesh", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
