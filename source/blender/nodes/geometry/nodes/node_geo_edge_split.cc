/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_edge_split_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_output<decl::Geometry>(N_("Mesh"));
}

/* Naively checks if the first vertices and the second vertices are the same. */
static inline bool naive_edges_equal(const MEdge &edge1, const MEdge &edge2)
{
  return edge1.v1 == edge2.v1 && edge1.v2 == edge2.v2;
}

static void add_new_vertices(Mesh &mesh, const Span<int> new_to_old_verts_map)
{
  CustomData_realloc(&mesh.vdata, mesh.totvert, mesh.totvert + new_to_old_verts_map.size());
  mesh.totvert += new_to_old_verts_map.size();

  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  for (const AttributeIDRef &id : attributes.all_ids()) {
    if (attributes.lookup_meta_data(id)->domain != ATTR_DOMAIN_POINT) {
      continue;
    }
    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
    if (!attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(attribute.span.type(), [&](auto dummy) {
      using T = decltype(dummy);
      MutableSpan<T> span = attribute.span.typed<T>();
      const Span<T> old_data = span.drop_back(new_to_old_verts_map.size());
      MutableSpan<T> new_data = span.take_back(new_to_old_verts_map.size());
      array_utils::gather(old_data, new_to_old_verts_map, new_data);
    });

    attribute.finish();
  }
}

static void add_new_edges(Mesh &mesh,
                          const Span<MEdge> new_edges,
                          const Span<int> new_to_old_edges_map)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();

  /* Store a copy of the IDs locally since we will remove the existing attributes which
   * can also free the names, since the API does not provide pointer stability. */
  Vector<std::string> named_ids;
  Vector<WeakAnonymousAttributeID> anonymous_ids;
  for (const AttributeIDRef &id : attributes.all_ids()) {
    if (attributes.lookup_meta_data(id)->domain != ATTR_DOMAIN_EDGE) {
      continue;
    }
    if (!id.should_be_kept()) {
      continue;
    }
    if (id.is_named()) {
      named_ids.append(id.name());
    }
    else {
      anonymous_ids.append(WeakAnonymousAttributeID(&id.anonymous_id()));
    }
  }
  Vector<AttributeIDRef> local_edge_ids;
  for (const StringRef name : named_ids) {
    local_edge_ids.append(name);
  }
  for (const WeakAnonymousAttributeID &id : anonymous_ids) {
    local_edge_ids.append(id.get());
  }

  /* Build new arrays for the copied edge attributes. Unlike vertices, new edges aren't all at the
   * end of the array, so just copying to the new edges would overwrite old values when they were
   * still needed. */
  struct NewAttributeData {
    const AttributeIDRef &local_id;
    const CPPType &type;
    void *array;
  };
  Vector<NewAttributeData> dst_attributes;
  for (const AttributeIDRef &id : local_edge_ids) {
    GAttributeReader attribute = attributes.lookup(id);
    if (!attribute) {
      continue;
    }

    const CPPType &type = attribute.varray.type();
    void *new_data = MEM_malloc_arrayN(new_edges.size(), type.size(), __func__);

    attribute_math::convert_to_static_type(type, [&](auto dummy) {
      using T = decltype(dummy);
      const VArray<T> src = attribute.varray.typed<T>();
      MutableSpan<T> dst(static_cast<T *>(new_data), new_edges.size());
      array_utils::gather(src, new_to_old_edges_map, dst);
    });

    /* Free the original attribute as soon as possible to lower peak memory usage. */
    attributes.remove(id);
    dst_attributes.append({id, type, new_data});
  }

  CustomData_free(&mesh.edata, mesh.totedge);
  mesh.totedge = new_edges.size();
  CustomData_add_layer(&mesh.edata, CD_MEDGE, CD_CONSTRUCT, nullptr, mesh.totedge);
  mesh.edges_for_write().copy_from(new_edges);

  for (NewAttributeData &new_data : dst_attributes) {
    attributes.add(new_data.local_id,
                   ATTR_DOMAIN_EDGE,
                   bke::cpp_type_to_custom_data_type(new_data.type),
                   bke::AttributeInitMoveArray(new_data.array));
  }
}

/**
 * Merge the new_edge into the original edge.
 *
 * NOTE: This function is very specific to the situation and makes a lot of assumptions.
 */
static void merge_edges(const int orig_edge_i,
                        const int new_edge_i,
                        MutableSpan<MLoop> new_loops,
                        Vector<Vector<int>> &edge_to_loop_map,
                        Vector<MEdge> &new_edges,
                        Vector<int> &new_to_old_edges_map)
{
  /* Merge back into the original edge by undoing the topology changes. */
  BLI_assert(edge_to_loop_map[new_edge_i].size() == 1);
  const int loop_i = edge_to_loop_map[new_edge_i][0];
  new_loops[loop_i].e = orig_edge_i;

  /* We are putting the last edge in the location of new_edge in all the maps, to remove
   * new_edge efficiently. We have to update the topology information for this last edge
   * though. Essentially we are replacing every instance of last_edge_i with new_edge_i. */
  const int last_edge_i = new_edges.size() - 1;
  if (last_edge_i != new_edge_i) {
    BLI_assert(edge_to_loop_map[last_edge_i].size() == 1);
    const int last_edge_loop_i = edge_to_loop_map[last_edge_i][0];
    new_loops[last_edge_loop_i].e = new_edge_i;
  }

  /* We can now safely swap-remove. */
  new_edges.remove_and_reorder(new_edge_i);
  edge_to_loop_map.remove_and_reorder(new_edge_i);
  new_to_old_edges_map.remove_and_reorder(new_edge_i);
}

/**
 * Replace the vertex of an edge with a new one, and update the connected loops.
 *
 * NOTE: This only updates the loops containing the edge and the old vertex. It should therefore
 * also be called on the adjacent edge.
 */
static void swap_vertex_of_edge(MEdge &edge,
                                const int old_vert,
                                const int new_vert,
                                MutableSpan<MLoop> loops,
                                const Span<int> connected_loops)
{
  if (edge.v1 == old_vert) {
    edge.v1 = new_vert;
  }
  else if (edge.v2 == old_vert) {
    edge.v2 = new_vert;
  }
  else {
    BLI_assert_unreachable();
  }

  for (const int loop_i : connected_loops) {
    if (loops[loop_i].v == old_vert) {
      loops[loop_i].v = new_vert;
    }
    /* The old vertex is on the loop containing the adjacent edge. Since this function is also
     * called on the adjacent edge, we don't replace it here. */
  }
}

/** Split the vertex into duplicates so that each fan has a different vertex. */
static void split_vertex_per_fan(const int vertex,
                                 const int start_offset,
                                 const int orig_verts_num,
                                 const Span<int> fans,
                                 const Span<int> fan_sizes,
                                 const Span<Vector<int>> edge_to_loop_map,
                                 MutableSpan<MEdge> new_edges,
                                 MutableSpan<MLoop> new_loops,
                                 MutableSpan<int> new_to_old_verts_map)
{
  int fan_start = 0;
  /* We don't need to create a new vertex for the last fan. That fan can just be connected to the
   * original vertex. */
  for (const int i : fan_sizes.index_range().drop_back(1)) {
    const int new_vert_i = start_offset + i;
    new_to_old_verts_map[new_vert_i - orig_verts_num] = vertex;

    for (const int edge_i : fans.slice(fan_start, fan_sizes[i])) {
      swap_vertex_of_edge(
          new_edges[edge_i], vertex, new_vert_i, new_loops, edge_to_loop_map[edge_i]);
    }
    fan_start += fan_sizes[i];
  }
}

/**
 * Get the index of the adjacent edge to a loop connected to a vertex. In other words, for the
 * given polygon return the unique edge connected to the given vertex and not on the given loop.
 */
static int adjacent_edge(Span<MLoop> loops, const int loop_i, const MPoly &poly, const int vertex)
{
  const int adjacent_loop_i = (loops[loop_i].v == vertex) ?
                                  bke::mesh_topology::previous_poly_loop(poly, loop_i) :
                                  bke::mesh_topology::next_poly_loop(poly, loop_i);
  return loops[adjacent_loop_i].e;
}

/**
 * Calculate the disjoint fans connected to the vertex, where a fan is a group of edges connected
 * through polygons. The connected_edges vector is rearranged in such a way that edges in the same
 * fan are grouped together. The r_fans_sizes Vector gives the sizes of the different fans, and can
 * be used to retreive the fans from connected_edges.
 */
static void calc_vertex_fans(const int vertex,
                             const Span<MLoop> new_loops,
                             const Span<MPoly> polys,
                             const Span<Vector<int>> edge_to_loop_map,
                             const Span<int> loop_to_poly_map,
                             MutableSpan<int> connected_edges,
                             Vector<int> &r_fan_sizes)
{
  if (connected_edges.size() <= 1) {
    r_fan_sizes.append(connected_edges.size());
    return;
  }

  Vector<int> search_edges;
  int total_found_edges_num = 0;
  int fan_size = 0;
  const int total_edge_num = connected_edges.size();
  /* Iteratively go through the connected edges. The front contains already handled edges, while
   * the back contains unhandled edges. */
  while (true) {
    /* This edge has not been visited yet. */
    int curr_i = total_found_edges_num;
    int curr_edge_i = connected_edges[curr_i];

    /* Gather all the edges in this fan. */
    while (true) {
      fan_size++;

      /* Add adjacent edges to search stack. */
      for (const int loop_i : edge_to_loop_map[curr_edge_i]) {
        const int adjacent_edge_i = adjacent_edge(
            new_loops, loop_i, polys[loop_to_poly_map[loop_i]], vertex);

        /* Find out if this edge was visited already. */
        int i = curr_i + 1;
        for (; i < total_edge_num; i++) {
          if (connected_edges[i] == adjacent_edge_i) {
            break;
          }
        }
        if (i == total_edge_num) {
          /* Already visited this edge. */
          continue;
        }
        search_edges.append(adjacent_edge_i);
        curr_i++;
        std::swap(connected_edges[curr_i], connected_edges[i]);
      }

      if (search_edges.is_empty()) {
        break;
      }

      curr_edge_i = search_edges.pop_last();
    }
    /* We have now collected all the edges in this fan. */
    total_found_edges_num += fan_size;
    BLI_assert(total_found_edges_num <= total_edge_num);
    r_fan_sizes.append(fan_size);
    if (total_found_edges_num == total_edge_num) {
      /* We have found all the edges, so this final batch must be the last connected fan. */
      break;
    }
    fan_size = 0;
  }
}

/**
 * Splits the edge into duplicates, so that each edge is connected to one poly.
 */
static void split_edge_per_poly(const int edge_i,
                                const int new_edge_start,
                                MutableSpan<Vector<int>> edge_to_loop_map,
                                MutableSpan<MLoop> new_loops,
                                MutableSpan<MEdge> new_edges,
                                MutableSpan<int> new_to_old_edges_map)
{
  if (edge_to_loop_map[edge_i].size() <= 1) {
    return;
  }
  int new_edge_index = new_edge_start;
  for (const int loop_i : edge_to_loop_map[edge_i].as_span().drop_front(1)) {
    const MEdge new_edge(new_edges[edge_i]);
    new_edges[new_edge_index] = new_edge;
    new_to_old_edges_map[new_edge_index] = edge_i;
    edge_to_loop_map[new_edge_index].append({loop_i});
    new_loops[loop_i].e = new_edge_index;
    new_edge_index++;
  }
  /* Only the first loop is now connected to this edge. */
  edge_to_loop_map[edge_i].resize(1);
}

static void mesh_edge_split(Mesh &mesh, const IndexMask mask)
{
  /* Flag vertices that need to be split. */
  Array<bool> should_split_vert(mesh.totvert, false);
  const Span<MEdge> edges = mesh.edges();
  for (const int edge_i : mask) {
    const MEdge edge = edges[edge_i];
    should_split_vert[edge.v1] = true;
    should_split_vert[edge.v2] = true;
  }

  /* Precalculate topology info. */
  Array<Vector<int>> vert_to_edge_map = bke::mesh_topology::build_vert_to_edge_map(edges,
                                                                                   mesh.totvert);
  Vector<Vector<int>> edge_to_loop_map = bke::mesh_topology::build_edge_to_loop_map_resizable(
      mesh.loops(), mesh.totedge);
  Array<int> loop_to_poly_map = bke::mesh_topology::build_loop_to_poly_map(mesh.polys(),
                                                                           mesh.totloop);

  /* Store offsets, so we can split edges in parallel. */
  Array<int> edge_offsets(edges.size());
  Array<int> num_edge_duplicates(edges.size());
  int new_edges_size = edges.size();
  for (const int edge : mask) {
    edge_offsets[edge] = new_edges_size;
    /* We add duplicates of the edge for each poly (except the first). */
    const int num_connected_loops = edge_to_loop_map[edge].size();
    const int num_duplicates = std::max(0, num_connected_loops - 1);
    new_edges_size += num_duplicates;
    num_edge_duplicates[edge] = num_duplicates;
  }

  const Span<MPoly> polys = mesh.polys();

  MutableSpan<MLoop> loops = mesh.loops_for_write();
  Vector<MEdge> new_edges(new_edges_size);
  new_edges.as_mutable_span().take_front(edges.size()).copy_from(edges);

  edge_to_loop_map.resize(new_edges_size);

  /* Used for transferring attributes. */
  Vector<int> new_to_old_edges_map(IndexRange(new_edges.size()).as_span());

  /* Step 1: Split the edges. */
  threading::parallel_for(mask.index_range(), 512, [&](IndexRange range) {
    for (const int mask_i : range) {
      const int edge_i = mask[mask_i];
      split_edge_per_poly(
          edge_i, edge_offsets[edge_i], edge_to_loop_map, loops, new_edges, new_to_old_edges_map);
    }
  });

  /* Step 1.5: Update topology information (can't parallelize). */
  for (const int edge_i : mask) {
    const MEdge &edge = edges[edge_i];
    for (const int duplicate_i : IndexRange(edge_offsets[edge_i], num_edge_duplicates[edge_i])) {
      vert_to_edge_map[edge.v1].append(duplicate_i);
      vert_to_edge_map[edge.v2].append(duplicate_i);
    }
  }

  /* Step 2: Calculate vertex fans. */
  Array<Vector<int>> vertex_fan_sizes(mesh.totvert);
  threading::parallel_for(IndexRange(mesh.totvert), 512, [&](IndexRange range) {
    for (const int vert : range) {
      if (!should_split_vert[vert]) {
        continue;
      }
      calc_vertex_fans(vert,
                       loops,
                       polys,
                       edge_to_loop_map,
                       loop_to_poly_map,
                       vert_to_edge_map[vert],
                       vertex_fan_sizes[vert]);
    }
  });

  /* Step 2.5: Calculate offsets for next step. */
  Array<int> vert_offsets(mesh.totvert);
  int total_verts_num = mesh.totvert;
  for (const int vert : IndexRange(mesh.totvert)) {
    if (!should_split_vert[vert]) {
      continue;
    }
    vert_offsets[vert] = total_verts_num;
    /* We only create a new vertex for each fan different from the first. */
    total_verts_num += vertex_fan_sizes[vert].size() - 1;
  }

  /* Step 3: Split the vertices.
   * Build a map from each new vertex to an old vertex to use for transferring attributes later. */
  const int new_verts_num = total_verts_num - mesh.totvert;
  Array<int> new_to_old_verts_map(new_verts_num);
  threading::parallel_for(IndexRange(mesh.totvert), 512, [&](IndexRange range) {
    for (const int vert : range) {
      if (!should_split_vert[vert]) {
        continue;
      }
      split_vertex_per_fan(vert,
                           vert_offsets[vert],
                           mesh.totvert,
                           vert_to_edge_map[vert],
                           vertex_fan_sizes[vert],
                           edge_to_loop_map,
                           new_edges,
                           loops,
                           new_to_old_verts_map);
    }
  });

  /* Step 4: Deduplicate edges. We loop backwards so we can use remove_and_reorder. Although this
   * does look bad (3 nested loops), in practice the inner loops are very small. For most meshes,
   * there are at most 2 polygons connected to each edge, and hence you'll only get at most 1
   * duplicate per edge. */
  for (int mask_i = mask.size() - 1; mask_i >= 0; mask_i--) {
    const int edge = mask[mask_i];
    int start_of_duplicates = edge_offsets[edge];
    int end_of_duplicates = start_of_duplicates + num_edge_duplicates[edge] - 1;
    for (int duplicate = end_of_duplicates; duplicate >= start_of_duplicates; duplicate--) {
      if (naive_edges_equal(new_edges[edge], new_edges[duplicate])) {
        merge_edges(edge, duplicate, loops, edge_to_loop_map, new_edges, new_to_old_edges_map);
        break;
      }
      for (int other = start_of_duplicates; other < duplicate; other++) {
        if (naive_edges_equal(new_edges[other], new_edges[duplicate])) {
          merge_edges(other, duplicate, loops, edge_to_loop_map, new_edges, new_to_old_edges_map);
          break;
        }
      }
    }
  }

  /* Step 5: Resize the mesh to add the new vertices and rebuild the edges. */
  add_new_vertices(mesh, new_to_old_verts_map);
  add_new_edges(mesh, new_edges, new_to_old_edges_map);

  BKE_mesh_tag_edges_split(&mesh);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (const Mesh *mesh = geometry_set.get_mesh_for_read()) {

      bke::MeshFieldContext field_context{*mesh, ATTR_DOMAIN_EDGE};
      fn::FieldEvaluator selection_evaluator{field_context, mesh->totedge};
      selection_evaluator.set_selection(selection_field);
      selection_evaluator.evaluate();
      const IndexMask mask = selection_evaluator.get_evaluated_selection_as_mask();
      if (mask.is_empty()) {
        return;
      }

      mesh_edge_split(*geometry_set.get_mesh_for_write(), mask);
    }
  });

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_edge_split_cc

void register_node_type_geo_edge_split()
{
  namespace file_ns = blender::nodes::node_geo_edge_split_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SPLIT_EDGES, "Split Edges", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
