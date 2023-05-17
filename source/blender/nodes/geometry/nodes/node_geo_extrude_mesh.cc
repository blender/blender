/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_disjoint_set.hh"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_extrude_mesh_cc {

NODE_STORAGE_FUNCS(NodeGeometryExtrudeMesh)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>("Selection").default_value(true).field_on_all().hide_value();
  b.add_input<decl::Vector>("Offset")
      .subtype(PROP_TRANSLATION)
      .implicit_field_on_all(implicit_field_inputs::normal)
      .hide_value();
  b.add_input<decl::Float>("Offset Scale").default_value(1.0f).field_on_all();
  b.add_input<decl::Bool>("Individual").default_value(true).make_available([](bNode &node) {
    node_storage(node).mode = GEO_NODE_EXTRUDE_MESH_FACES;
  });
  b.add_output<decl::Geometry>("Mesh").propagate_all();
  b.add_output<decl::Bool>("Top").field_on_all();
  b.add_output<decl::Bool>("Side").field_on_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryExtrudeMesh *data = MEM_cnew<NodeGeometryExtrudeMesh>(__func__);
  data->mode = GEO_NODE_EXTRUDE_MESH_FACES;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryExtrudeMesh &storage = node_storage(*node);
  const GeometryNodeExtrudeMeshMode mode = GeometryNodeExtrudeMeshMode(storage.mode);

  bNodeSocket *individual_socket = static_cast<bNodeSocket *>(node->inputs.last);

  bke::nodeSetSocketAvailability(ntree, individual_socket, mode == GEO_NODE_EXTRUDE_MESH_FACES);
}

struct AttributeOutputs {
  AnonymousAttributeIDPtr top_id;
  AnonymousAttributeIDPtr side_id;
};

static void save_selection_as_attribute(Mesh &mesh,
                                        const AnonymousAttributeID *id,
                                        const eAttrDomain domain,
                                        const IndexMask selection)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  BLI_assert(!attributes.contains(id));

  SpanAttributeWriter<bool> attribute = attributes.lookup_or_add_for_write_span<bool>(id, domain);
  /* Rely on the new attribute being zeroed by default. */
  BLI_assert(!attribute.span.as_span().contains(true));

  if (selection.is_range()) {
    attribute.span.slice(selection.as_range()).fill(true);
  }
  else {
    attribute.span.fill_indices(selection.indices(), true);
  }

  attribute.finish();
}

static void remove_non_propagated_attributes(
    MutableAttributeAccessor attributes, const AnonymousAttributePropagationInfo &propagation_info)
{
  if (propagation_info.propagate_all) {
    return;
  }
  Set<AttributeIDRef> ids_to_remove = attributes.all_ids();
  ids_to_remove.remove_if([&](const AttributeIDRef &id) {
    if (!id.is_anonymous()) {
      return true;
    }
    if (propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    return false;
  });
  for (const AttributeIDRef &id : ids_to_remove) {
    attributes.remove(id);
  }
}

static void expand_mesh(Mesh &mesh,
                        const int vert_expand,
                        const int edge_expand,
                        const int poly_expand,
                        const int loop_expand)
{
  /* Remove types that aren't supported for interpolation in this node. */
  if (vert_expand != 0) {
    CustomData_free_layers(&mesh.vdata, CD_ORCO, mesh.totvert);
    CustomData_free_layers(&mesh.vdata, CD_BWEIGHT, mesh.totvert);
    CustomData_free_layers(&mesh.vdata, CD_SHAPEKEY, mesh.totvert);
    CustomData_free_layers(&mesh.vdata, CD_CLOTH_ORCO, mesh.totvert);
    CustomData_free_layers(&mesh.vdata, CD_MVERT_SKIN, mesh.totvert);
    const int old_verts_num = mesh.totvert;
    mesh.totvert += vert_expand;
    CustomData_realloc(&mesh.vdata, old_verts_num, mesh.totvert);
  }
  if (edge_expand != 0) {
    CustomData_free_layers(&mesh.edata, CD_BWEIGHT, mesh.totedge);
    CustomData_free_layers(&mesh.edata, CD_FREESTYLE_EDGE, mesh.totedge);
    const int old_edges_num = mesh.totedge;
    mesh.totedge += edge_expand;
    CustomData_realloc(&mesh.edata, old_edges_num, mesh.totedge);
  }
  if (poly_expand != 0) {
    CustomData_free_layers(&mesh.pdata, CD_FACEMAP, mesh.totpoly);
    CustomData_free_layers(&mesh.pdata, CD_FREESTYLE_FACE, mesh.totpoly);
    const int old_polys_num = mesh.totpoly;
    mesh.totpoly += poly_expand;
    CustomData_realloc(&mesh.pdata, old_polys_num, mesh.totpoly);
    implicit_sharing::resize_trivial_array(&mesh.poly_offset_indices,
                                           &mesh.runtime->poly_offsets_sharing_info,
                                           old_polys_num == 0 ? 0 : (old_polys_num + 1),
                                           mesh.totpoly + 1);
    /* Set common values for convenience. */
    mesh.poly_offset_indices[0] = 0;
    mesh.poly_offset_indices[mesh.totpoly] = mesh.totloop + loop_expand;
  }
  if (loop_expand != 0) {
    CustomData_free_layers(&mesh.ldata, CD_NORMAL, mesh.totloop);
    CustomData_free_layers(&mesh.ldata, CD_MDISPS, mesh.totloop);
    CustomData_free_layers(&mesh.ldata, CD_TANGENT, mesh.totloop);
    CustomData_free_layers(&mesh.ldata, CD_PAINT_MASK, mesh.totloop);
    CustomData_free_layers(&mesh.ldata, CD_MLOOPTANGENT, mesh.totloop);
    CustomData_free_layers(&mesh.ldata, CD_GRID_PAINT_MASK, mesh.totloop);
    CustomData_free_layers(&mesh.ldata, CD_CUSTOMLOOPNORMAL, mesh.totloop);
    const int old_loops_num = mesh.totloop;
    mesh.totloop += loop_expand;
    CustomData_realloc(&mesh.ldata, old_loops_num, mesh.totloop);
  }
}

static CustomData &mesh_custom_data_for_domain(Mesh &mesh, const eAttrDomain domain)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT:
      return mesh.vdata;
    case ATTR_DOMAIN_EDGE:
      return mesh.edata;
    case ATTR_DOMAIN_FACE:
      return mesh.pdata;
    case ATTR_DOMAIN_CORNER:
      return mesh.ldata;
    default:
      BLI_assert_unreachable();
      return mesh.vdata;
  }
}

/**
 * \note The result may be an empty span.
 */
static MutableSpan<int> get_orig_index_layer(Mesh &mesh, const eAttrDomain domain)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  CustomData &custom_data = mesh_custom_data_for_domain(mesh, domain);
  if (int *orig_indices = static_cast<int *>(CustomData_get_layer_for_write(
          &custom_data, CD_ORIGINDEX, attributes.domain_size(domain))))
  {
    return {orig_indices, attributes.domain_size(domain)};
  }
  return {};
}

/**
 * \param get_mix_indices_fn: Returns a Span of indices of the source points to mix for every
 * result point.
 */
template<typename T>
void copy_with_mixing(const Span<T> src,
                      const FunctionRef<Span<int>(int)> get_mix_indices_fn,
                      MutableSpan<T> dst)
{
  threading::parallel_for(dst.index_range(), 512, [&](const IndexRange range) {
    bke::attribute_math::DefaultPropagationMixer<T> mixer{dst.slice(range)};
    for (const int i_dst : IndexRange(range.size())) {
      const Span<int> indices = get_mix_indices_fn(range[i_dst]);
      for (const int i_src : indices) {
        mixer.mix_in(i_dst, src[i_src]);
      }
    }
    mixer.finalize();
  });
}

static void copy_with_mixing(const GSpan src,
                             const FunctionRef<Span<int>(int)> get_mix_indices_fn,
                             GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    copy_with_mixing(src.typed<T>(), get_mix_indices_fn, dst.typed<T>());
  });
}

static Array<Vector<int>> create_vert_to_edge_map(const int vert_size,
                                                  const Span<int2> edges,
                                                  const int vert_offset = 0)
{
  Array<Vector<int>> vert_to_edge_map(vert_size);
  for (const int i : edges.index_range()) {
    vert_to_edge_map[edges[i][0] - vert_offset].append(i);
    vert_to_edge_map[edges[i][1] - vert_offset].append(i);
  }
  return vert_to_edge_map;
}

static void extrude_mesh_vertices(Mesh &mesh,
                                  const Field<bool> &selection_field,
                                  const Field<float3> &offset_field,
                                  const AttributeOutputs &attribute_outputs,
                                  const AnonymousAttributePropagationInfo &propagation_info)
{
  const int orig_vert_size = mesh.totvert;
  const int orig_edge_size = mesh.totedge;

  /* Use an array for the result of the evaluation because the mesh is reallocated before
   * the vertices are moved, and the evaluated result might reference an attribute. */
  Array<float3> offsets(orig_vert_size);
  const bke::MeshFieldContext context{mesh, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{context, mesh.totvert};
  evaluator.add_with_destination(offset_field, offsets.as_mutable_span());
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  remove_non_propagated_attributes(mesh.attributes_for_write(), propagation_info);

  Set<AttributeIDRef> point_ids;
  Set<AttributeIDRef> edge_ids;
  mesh.attributes().for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (meta_data.domain == ATTR_DOMAIN_POINT) {
      if (id.name() != "position") {
        point_ids.add(id);
      }
    }
    else if (meta_data.domain == ATTR_DOMAIN_EDGE) {
      if (id.name() != ".edge_verts") {
        edge_ids.add(id);
      }
    }
    return true;
  });

  /* This allows parallelizing attribute mixing for new edges. */
  Array<Vector<int>> vert_to_edge_map;
  if (!edge_ids.is_empty()) {
    vert_to_edge_map = create_vert_to_edge_map(orig_vert_size, mesh.edges());
  }

  expand_mesh(mesh, selection.size(), selection.size(), 0, 0);

  const IndexRange new_vert_range{orig_vert_size, selection.size()};
  const IndexRange new_edge_range{orig_edge_size, selection.size()};

  MutableSpan<int2> new_edges = mesh.edges_for_write().slice(new_edge_range);
  for (const int i_selection : selection.index_range()) {
    new_edges[i_selection] = int2(selection[i_selection], new_vert_range[i_selection]);
  }

  MutableAttributeAccessor attributes = mesh.attributes_for_write();

  /* New vertices copy the attribute values from their source vertex. */
  for (const AttributeIDRef &id : point_ids) {
    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
    array_utils::gather(attribute.span, selection, attribute.span.slice(new_vert_range));
    attribute.finish();
  }

  /* New edge values are mixed from of all the edges connected to the source vertex. */
  for (const AttributeIDRef &id : edge_ids) {
    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
    copy_with_mixing(
        attribute.span,
        [&](const int i) { return vert_to_edge_map[selection[i]].as_span(); },
        attribute.span.slice(new_edge_range));
    attribute.finish();
  }

  MutableSpan<float3> positions = mesh.vert_positions_for_write();
  MutableSpan<float3> new_positions = positions.slice(new_vert_range);
  threading::parallel_for(selection.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      new_positions[i] = positions[selection[i]] + offsets[selection[i]];
    }
  });

  MutableSpan<int> vert_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_POINT);
  if (!vert_orig_indices.is_empty()) {
    array_utils::gather(
        vert_orig_indices.as_span(), selection, vert_orig_indices.slice(new_vert_range));
  }

  MutableSpan<int> new_edge_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_EDGE);
  new_edge_orig_indices.slice_safe(new_edge_range).fill(ORIGINDEX_NONE);

  if (attribute_outputs.top_id) {
    save_selection_as_attribute(
        mesh, attribute_outputs.top_id.get(), ATTR_DOMAIN_POINT, new_vert_range);
  }
  if (attribute_outputs.side_id) {
    save_selection_as_attribute(
        mesh, attribute_outputs.side_id.get(), ATTR_DOMAIN_EDGE, new_edge_range);
  }

  BKE_mesh_runtime_clear_cache(&mesh);
}

static void fill_quad_consistent_direction(const Span<int> other_poly_verts,
                                           const Span<int> other_poly_edges,
                                           MutableSpan<int> new_corner_verts,
                                           MutableSpan<int> new_corner_edges,
                                           const int vert_connected_to_poly_1,
                                           const int vert_connected_to_poly_2,
                                           const int vert_across_from_poly_1,
                                           const int vert_across_from_poly_2,
                                           const int edge_connected_to_poly,
                                           const int connecting_edge_1,
                                           const int edge_across_from_poly,
                                           const int connecting_edge_2)
{
  /* Find the loop on the polygon connected to the new quad that uses the duplicate edge. */
  bool start_with_connecting_edge = true;
  for (const int i : other_poly_edges.index_range()) {
    if (other_poly_edges[i] == edge_connected_to_poly) {
      start_with_connecting_edge = other_poly_verts[i] == vert_connected_to_poly_1;
      break;
    }
  }
  if (start_with_connecting_edge) {
    new_corner_verts[0] = vert_connected_to_poly_1;
    new_corner_edges[0] = connecting_edge_1;
    new_corner_verts[1] = vert_across_from_poly_1;
    new_corner_edges[1] = edge_across_from_poly;
    new_corner_verts[2] = vert_across_from_poly_2;
    new_corner_edges[2] = connecting_edge_2;
    new_corner_verts[3] = vert_connected_to_poly_2;
    new_corner_edges[3] = edge_connected_to_poly;
  }
  else {
    new_corner_verts[0] = vert_connected_to_poly_1;
    new_corner_edges[0] = edge_connected_to_poly;
    new_corner_verts[1] = vert_connected_to_poly_2;
    new_corner_edges[1] = connecting_edge_2;
    new_corner_verts[2] = vert_across_from_poly_2;
    new_corner_edges[2] = edge_across_from_poly;
    new_corner_verts[3] = vert_across_from_poly_1;
    new_corner_edges[3] = connecting_edge_1;
  }
}

template<typename T>
static VectorSet<int> vert_indices_from_edges(const Mesh &mesh, const Span<T> edge_indices)
{
  static_assert(is_same_any_v<T, int, int64_t>);
  const Span<int2> edges = mesh.edges();

  VectorSet<int> vert_indices;
  vert_indices.reserve(edge_indices.size());
  for (const T i_edge : edge_indices) {
    const int2 &edge = edges[i_edge];
    vert_indices.add(edge[0]);
    vert_indices.add(edge[1]);
  }
  return vert_indices;
}

static void extrude_mesh_edges(Mesh &mesh,
                               const Field<bool> &selection_field,
                               const Field<float3> &offset_field,
                               const AttributeOutputs &attribute_outputs,
                               const AnonymousAttributePropagationInfo &propagation_info)
{
  const int orig_vert_size = mesh.totvert;
  const Span<int2> orig_edges = mesh.edges();
  const OffsetIndices orig_polys = mesh.polys();
  const int orig_loop_size = mesh.totloop;

  const bke::MeshFieldContext edge_context{mesh, ATTR_DOMAIN_EDGE};
  FieldEvaluator edge_evaluator{edge_context, mesh.totedge};
  edge_evaluator.set_selection(selection_field);
  edge_evaluator.add(offset_field);
  edge_evaluator.evaluate();
  const IndexMask edge_selection = edge_evaluator.get_evaluated_selection_as_mask();
  const VArray<float3> edge_offsets = edge_evaluator.get_evaluated<float3>(0);
  if (edge_selection.is_empty()) {
    return;
  }

  const Array<Vector<int, 2>> edge_to_poly_map = bke::mesh_topology::build_edge_to_poly_map(
      orig_polys, mesh.corner_edges(), mesh.totedge);

  /* Find the offsets on the vertex domain for translation. This must be done before the mesh's
   * custom data layers are reallocated, in case the virtual array references one of them. */
  Array<float3> vert_offsets;
  if (!edge_offsets.is_single()) {
    vert_offsets.reinitialize(orig_vert_size);
    bke::attribute_math::DefaultPropagationMixer<float3> mixer(vert_offsets);
    for (const int i_edge : edge_selection) {
      const int2 &edge = orig_edges[i_edge];
      const float3 offset = edge_offsets[i_edge];
      mixer.mix_in(edge[0], offset);
      mixer.mix_in(edge[1], offset);
    }
    mixer.finalize();
  }

  const VectorSet<int> new_vert_indices = vert_indices_from_edges(mesh, edge_selection.indices());

  const IndexRange new_vert_range{orig_vert_size, new_vert_indices.size()};
  /* The extruded edges connect the original and duplicate edges. */
  const IndexRange connect_edge_range{orig_edges.size(), new_vert_range.size()};
  /* The duplicate edges are extruded copies of the selected edges. */
  const IndexRange duplicate_edge_range = connect_edge_range.after(edge_selection.size());
  /* There is a new polygon for every selected edge. */
  const IndexRange new_poly_range{orig_polys.size(), edge_selection.size()};
  /* Every new polygon is a quad with four corners. */
  const IndexRange new_loop_range{orig_loop_size, new_poly_range.size() * 4};

  remove_non_propagated_attributes(mesh.attributes_for_write(), propagation_info);
  expand_mesh(mesh,
              new_vert_range.size(),
              connect_edge_range.size() + duplicate_edge_range.size(),
              new_poly_range.size(),
              new_loop_range.size());

  MutableSpan<int2> edges = mesh.edges_for_write();
  MutableSpan<int2> connect_edges = edges.slice(connect_edge_range);
  MutableSpan<int2> duplicate_edges = edges.slice(duplicate_edge_range);
  MutableSpan<int> poly_offsets = mesh.poly_offsets_for_write();
  MutableSpan<int> new_poly_offsets = poly_offsets.slice(new_poly_range);
  MutableSpan<int> corner_verts = mesh.corner_verts_for_write();
  MutableSpan<int> new_corner_verts = corner_verts.slice(new_loop_range);
  MutableSpan<int> corner_edges = mesh.corner_edges_for_write();
  MutableSpan<int> new_corner_edges = corner_edges.slice(new_loop_range);

  new_poly_offsets.fill(4);
  offset_indices::accumulate_counts_to_offsets(new_poly_offsets, orig_loop_size);
  const OffsetIndices polys = mesh.polys();

  for (const int i : connect_edges.index_range()) {
    connect_edges[i] = int2(new_vert_indices[i], new_vert_range[i]);
  }

  for (const int i : duplicate_edges.index_range()) {
    const int2 &orig_edge = edges[edge_selection[i]];
    const int i_new_vert_1 = new_vert_indices.index_of(orig_edge[0]);
    const int i_new_vert_2 = new_vert_indices.index_of(orig_edge[1]);
    duplicate_edges[i] = int2(new_vert_range[i_new_vert_1], new_vert_range[i_new_vert_2]);
  }

  for (const int i : edge_selection.index_range()) {
    const int orig_edge_index = edge_selection[i];

    const int2 &duplicate_edge = duplicate_edges[i];
    const int new_vert_1 = duplicate_edge[0];
    const int new_vert_2 = duplicate_edge[1];
    const int extrude_index_1 = new_vert_1 - orig_vert_size;
    const int extrude_index_2 = new_vert_2 - orig_vert_size;

    const Span<int> connected_polys = edge_to_poly_map[orig_edge_index];

    /* When there was a single polygon connected to the new polygon, we can use the old one to keep
     * the face direction consistent. When there is more than one connected edge, the new face
     * direction is totally arbitrary and the only goal for the behavior is to be deterministic. */
    Span<int> connected_poly_verts = {};
    Span<int> connected_poly_edges = {};
    if (connected_polys.size() == 1) {
      const IndexRange connected_poly = polys[connected_polys.first()];
      connected_poly_verts = corner_verts.slice(connected_poly);
      connected_poly_edges = corner_edges.slice(connected_poly);
    }
    fill_quad_consistent_direction(connected_poly_verts,
                                   connected_poly_edges,
                                   new_corner_verts.slice(4 * i, 4),
                                   new_corner_edges.slice(4 * i, 4),
                                   new_vert_indices[extrude_index_1],
                                   new_vert_indices[extrude_index_2],
                                   new_vert_1,
                                   new_vert_2,
                                   orig_edge_index,
                                   connect_edge_range[extrude_index_1],
                                   duplicate_edge_range[i],
                                   connect_edge_range[extrude_index_2]);
  }

  /* Create a map of indices in the extruded vertices array to all of the indices of edges
   * in the duplicate edges array that connect to that vertex. This can be used to simplify the
   * mixing of attribute data for the connecting edges. */
  const Array<Vector<int>> new_vert_to_duplicate_edge_map = create_vert_to_edge_map(
      new_vert_range.size(), duplicate_edges, orig_vert_size);

  MutableAttributeAccessor attributes = mesh.attributes_for_write();

  attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (ELEM(id.name(), ".corner_vert", ".corner_edge", ".edge_verts")) {
      return true;
    }
    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);

    switch (attribute.domain) {
      case ATTR_DOMAIN_POINT: {
        /* New vertices copy the attribute values from their source vertex. */
        bke::attribute_math::gather(
            attribute.span, new_vert_indices, attribute.span.slice(new_vert_range));
        break;
      }
      case ATTR_DOMAIN_EDGE: {
        /* Edges parallel to original edges copy the edge attributes from the original edges. */
        GMutableSpan duplicate_data = attribute.span.slice(duplicate_edge_range);
        array_utils::gather(attribute.span, edge_selection, duplicate_data);

        /* Edges connected to original vertices mix values of selected connected edges. */
        copy_with_mixing(
            duplicate_data,
            [&](const int i) { return new_vert_to_duplicate_edge_map[i].as_span(); },
            attribute.span.slice(connect_edge_range));
        break;
      }
      case ATTR_DOMAIN_FACE: {
        /* Attribute values for new faces are a mix of the values of faces connected to the its
         * original edge. */
        copy_with_mixing(
            attribute.span,
            [&](const int i) { return edge_to_poly_map[edge_selection[i]].as_span(); },
            attribute.span.slice(new_poly_range));
        break;
      }
      case ATTR_DOMAIN_CORNER: {
        /* New corners get the average value of all adjacent corners on original faces connected
         * to the original edge of their face. */
        bke::attribute_math::convert_to_static_type(meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          MutableSpan<T> data = attribute.span.typed<T>();
          MutableSpan<T> new_data = data.slice(new_loop_range);
          threading::parallel_for(edge_selection.index_range(), 256, [&](const IndexRange range) {
            for (const int i_edge_selection : range) {
              const int orig_edge_index = edge_selection[i_edge_selection];

              const Span<int> connected_polys = edge_to_poly_map[orig_edge_index];
              if (connected_polys.is_empty()) {
                /* If there are no connected polygons, there is no corner data to
                 * interpolate. */
                new_data.slice(4 * i_edge_selection, 4).fill(T());
                continue;
              }

              /* Both corners on each vertical edge of the side polygon get the same value,
               * so there are only two unique values to mix. */
              Array<T> side_poly_corner_data(2);
              bke::attribute_math::DefaultPropagationMixer<T> mixer{side_poly_corner_data};

              const int2 &duplicate_edge = duplicate_edges[i_edge_selection];
              const int new_vert_1 = duplicate_edge[0];
              const int new_vert_2 = duplicate_edge[1];
              const int orig_vert_1 = new_vert_indices[new_vert_1 - orig_vert_size];
              const int orig_vert_2 = new_vert_indices[new_vert_2 - orig_vert_size];

              /* Average the corner data from the corners that share a vertex from the
               * polygons that share an edge with the extruded edge. */
              for (const int i_connected_poly : connected_polys.index_range()) {
                const IndexRange connected_poly = polys[connected_polys[i_connected_poly]];
                for (const int i_loop : IndexRange(connected_poly)) {
                  if (corner_verts[i_loop] == orig_vert_1) {
                    mixer.mix_in(0, data[i_loop]);
                  }
                  if (corner_verts[i_loop] == orig_vert_2) {
                    mixer.mix_in(1, data[i_loop]);
                  }
                }
              }

              mixer.finalize();

              /* Instead of replicating the order in #fill_quad_consistent_direction here, it's
               * simpler (though probably slower) to just match the corner data based on the vertex
               * indices. */
              for (const int i : IndexRange(4 * i_edge_selection, 4)) {
                if (ELEM(new_corner_verts[i], new_vert_1, orig_vert_1)) {
                  new_data[i] = side_poly_corner_data.first();
                }
                else if (ELEM(new_corner_verts[i], new_vert_2, orig_vert_2)) {
                  new_data[i] = side_poly_corner_data.last();
                }
              }
            }
          });
        });
        break;
      }
      default:
        BLI_assert_unreachable();
    }

    attribute.finish();
    return true;
  });

  MutableSpan<float3> new_positions = mesh.vert_positions_for_write().slice(new_vert_range);
  if (edge_offsets.is_single()) {
    const float3 offset = edge_offsets.get_internal_single();
    threading::parallel_for(new_positions.index_range(), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        new_positions[i] += offset;
      }
    });
  }
  else {
    threading::parallel_for(new_positions.index_range(), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        new_positions[i] += vert_offsets[new_vert_indices[i]];
      }
    });
  }

  MutableSpan<int> vert_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_POINT);
  if (!vert_orig_indices.is_empty()) {
    array_utils::gather(vert_orig_indices.as_span(),
                        new_vert_indices.as_span(),
                        vert_orig_indices.slice(new_vert_range));
  }

  MutableSpan<int> edge_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_EDGE);
  if (!edge_orig_indices.is_empty()) {
    edge_orig_indices.slice(connect_edge_range).fill(ORIGINDEX_NONE);
    array_utils::gather(edge_orig_indices.as_span(),
                        edge_selection,
                        edge_orig_indices.slice(duplicate_edge_range));
  }

  MutableSpan<int> poly_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_FACE);
  poly_orig_indices.slice_safe(new_poly_range).fill(ORIGINDEX_NONE);

  if (attribute_outputs.top_id) {
    save_selection_as_attribute(
        mesh, attribute_outputs.top_id.get(), ATTR_DOMAIN_EDGE, duplicate_edge_range);
  }
  if (attribute_outputs.side_id) {
    save_selection_as_attribute(
        mesh, attribute_outputs.side_id.get(), ATTR_DOMAIN_FACE, new_poly_range);
  }

  BKE_mesh_runtime_clear_cache(&mesh);
}

/**
 * Edges connected to one selected face are on the boundary of a region and will be duplicated into
 * a "side face". Edges inside a region will be duplicated to leave any original faces unchanged.
 */
static void extrude_mesh_face_regions(Mesh &mesh,
                                      const Field<bool> &selection_field,
                                      const Field<float3> &offset_field,
                                      const AttributeOutputs &attribute_outputs,
                                      const AnonymousAttributePropagationInfo &propagation_info)
{
  const int orig_vert_size = mesh.totvert;
  const Span<int2> orig_edges = mesh.edges();
  const OffsetIndices orig_polys = mesh.polys();
  const Span<int> orig_corner_verts = mesh.corner_verts();
  const int orig_loop_size = orig_corner_verts.size();

  const bke::MeshFieldContext poly_context{mesh, ATTR_DOMAIN_FACE};
  FieldEvaluator poly_evaluator{poly_context, mesh.totpoly};
  poly_evaluator.set_selection(selection_field);
  poly_evaluator.add(offset_field);
  poly_evaluator.evaluate();
  const IndexMask poly_selection = poly_evaluator.get_evaluated_selection_as_mask();
  const VArray<float3> poly_position_offsets = poly_evaluator.get_evaluated<float3>(0);
  if (poly_selection.is_empty()) {
    return;
  }

  Array<bool> poly_selection_array(orig_polys.size(), false);
  for (const int i_poly : poly_selection) {
    poly_selection_array[i_poly] = true;
  }

  /* Mix the offsets from the face domain to the vertex domain. Evaluate on the face domain above
   * in order to be consistent with the selection, and to use the face normals rather than vertex
   * normals as an offset, for example. */
  Array<float3> vert_offsets;
  if (!poly_position_offsets.is_single()) {
    vert_offsets.reinitialize(orig_vert_size);
    bke::attribute_math::DefaultPropagationMixer<float3> mixer(vert_offsets);
    for (const int i_poly : poly_selection) {
      const float3 offset = poly_position_offsets[i_poly];
      for (const int vert : orig_corner_verts.slice(orig_polys[i_poly])) {
        mixer.mix_in(vert, offset);
      }
    }
    mixer.finalize();
  }

  /* All of the faces (selected and deselected) connected to each edge. */
  const Array<Vector<int, 2>> edge_to_poly_map = bke::mesh_topology::build_edge_to_poly_map(
      orig_polys, mesh.corner_edges(), orig_edges.size());

  /* All vertices that are connected to the selected polygons.
   * Start the size at one vert per poly to reduce unnecessary reallocation. */
  VectorSet<int> all_selected_verts;
  all_selected_verts.reserve(orig_polys.size());
  for (const int i_poly : poly_selection) {
    for (const int vert : orig_corner_verts.slice(orig_polys[i_poly])) {
      all_selected_verts.add(vert);
    }
  }

  /* Edges inside of an extruded region that are also attached to deselected edges. They must be
   * duplicated in order to leave the old edge attached to the unchanged deselected faces. */
  VectorSet<int> new_inner_edge_indices;
  /* Edges inside of an extruded region. Their vertices should be translated
   * with the offset, but the edges themselves should not be duplicated. */
  Vector<int> inner_edge_indices;
  /* The extruded face corresponding to each boundary edge (and each boundary face). */
  Vector<int> edge_extruded_face_indices;
  /* Edges on the outside of selected regions, either because there are no
   * other connected faces, or because all of the other faces aren't selected. */
  VectorSet<int> boundary_edge_indices;
  for (const int i_edge : orig_edges.index_range()) {
    const Span<int> polys = edge_to_poly_map[i_edge];

    int i_selected_poly = -1;
    int deselected_poly_count = 0;
    int selected_poly_count = 0;
    for (const int i_other_poly : polys) {
      if (poly_selection_array[i_other_poly]) {
        selected_poly_count++;
        i_selected_poly = i_other_poly;
      }
      else {
        deselected_poly_count++;
      }
    }

    if (selected_poly_count == 1) {
      /* If there is only one selected polygon connected to the edge,
       * the edge should be extruded to form a "side face". */
      boundary_edge_indices.add_new(i_edge);
      edge_extruded_face_indices.append(i_selected_poly);
    }
    else if (selected_poly_count > 1) {
      /* The edge is inside an extruded region of faces. */
      if (deselected_poly_count > 0) {
        /* Add edges that are also connected to deselected edges to a separate list. */
        new_inner_edge_indices.add_new(i_edge);
      }
      else {
        /* Otherwise, just keep track of edges inside the region so that
         * we can reattach them to duplicated vertices if necessary. */
        inner_edge_indices.append(i_edge);
      }
    }
  }

  VectorSet<int> new_vert_indices = vert_indices_from_edges(mesh, boundary_edge_indices.as_span());
  /* Before adding the rest of the new vertices from the new inner edges, store the number
   * of new vertices from the boundary edges, since this is the number of connecting edges. */
  const int extruded_vert_size = new_vert_indices.size();

  /* The vertices attached to duplicate inner edges also have to be duplicated. */
  for (const int i_edge : new_inner_edge_indices) {
    const int2 &edge = orig_edges[i_edge];
    new_vert_indices.add(edge[0]);
    new_vert_indices.add(edge[1]);
  }

  /* New vertices forming the duplicated boundary edges and the ends of the new inner edges. */
  const IndexRange new_vert_range{orig_vert_size, new_vert_indices.size()};
  /* One edge connects each selected vertex to a new vertex on the extruded polygons. */
  const IndexRange connect_edge_range{orig_edges.size(), extruded_vert_size};
  /* Each selected edge is duplicated to form a single edge on the extrusion. */
  const IndexRange boundary_edge_range = connect_edge_range.after(boundary_edge_indices.size());
  /* Duplicated edges inside regions that were connected to deselected faces. */
  const IndexRange new_inner_edge_range = boundary_edge_range.after(new_inner_edge_indices.size());
  /* Each edge selected for extrusion is extruded into a single face. */
  const IndexRange side_poly_range{orig_polys.size(), boundary_edge_indices.size()};
  /* The loops that form the new side faces. */
  const IndexRange side_loop_range{orig_corner_verts.size(), side_poly_range.size() * 4};

  remove_non_propagated_attributes(mesh.attributes_for_write(), propagation_info);
  expand_mesh(mesh,
              new_vert_range.size(),
              connect_edge_range.size() + boundary_edge_range.size() + new_inner_edge_range.size(),
              side_poly_range.size(),
              side_loop_range.size());

  MutableSpan<int2> edges = mesh.edges_for_write();
  MutableSpan<int2> connect_edges = edges.slice(connect_edge_range);
  MutableSpan<int2> boundary_edges = edges.slice(boundary_edge_range);
  MutableSpan<int2> new_inner_edges = edges.slice(new_inner_edge_range);
  MutableSpan<int> poly_offsets = mesh.poly_offsets_for_write();
  MutableSpan<int> new_poly_offsets = poly_offsets.slice(side_poly_range);
  MutableSpan<int> corner_verts = mesh.corner_verts_for_write();
  MutableSpan<int> new_corner_verts = corner_verts.slice(side_loop_range);
  MutableSpan<int> corner_edges = mesh.corner_edges_for_write();
  MutableSpan<int> new_corner_edges = corner_edges.slice(side_loop_range);

  /* Initialize the new side polygons. */
  if (!new_poly_offsets.is_empty()) {
    new_poly_offsets.fill(4);
    offset_indices::accumulate_counts_to_offsets(new_poly_offsets, orig_loop_size);
  }
  const OffsetIndices polys = mesh.polys();

  /* Initialize the edges that form the sides of the extrusion. */
  for (const int i : connect_edges.index_range()) {
    connect_edges[i] = int2(new_vert_indices[i], new_vert_range[i]);
  }

  /* Initialize the edges that form the top of the extrusion. */
  for (const int i : boundary_edges.index_range()) {
    const int2 &orig_edge = edges[boundary_edge_indices[i]];
    const int i_new_vert_1 = new_vert_indices.index_of(orig_edge[0]);
    const int i_new_vert_2 = new_vert_indices.index_of(orig_edge[1]);
    boundary_edges[i] = int2(new_vert_range[i_new_vert_1], new_vert_range[i_new_vert_2]);
  }

  /* Initialize the new edges inside of extrude regions. */
  for (const int i : new_inner_edge_indices.index_range()) {
    const int2 &orig_edge = edges[new_inner_edge_indices[i]];
    const int i_new_vert_1 = new_vert_indices.index_of(orig_edge[0]);
    const int i_new_vert_2 = new_vert_indices.index_of(orig_edge[1]);
    new_inner_edges[i] = int2(new_vert_range[i_new_vert_1], new_vert_range[i_new_vert_2]);
  }

  /* Connect original edges inside face regions to any new vertices, if necessary. */
  for (const int i : inner_edge_indices) {
    int2 &edge = edges[i];
    const int i_new_vert_1 = new_vert_indices.index_of_try(edge[0]);
    const int i_new_vert_2 = new_vert_indices.index_of_try(edge[1]);
    if (i_new_vert_1 != -1) {
      edge[0] = new_vert_range[i_new_vert_1];
    }
    if (i_new_vert_2 != -1) {
      edge[1] = new_vert_range[i_new_vert_2];
    }
  }

  /* Connect the selected faces to the extruded or duplicated edges and the new vertices. */
  for (const int i_poly : poly_selection) {
    for (const int corner : polys[i_poly]) {
      const int i_new_vert = new_vert_indices.index_of_try(corner_verts[corner]);
      if (i_new_vert != -1) {
        corner_verts[corner] = new_vert_range[i_new_vert];
      }
      const int i_boundary_edge = boundary_edge_indices.index_of_try(corner_edges[corner]);
      if (i_boundary_edge != -1) {
        corner_edges[corner] = boundary_edge_range[i_boundary_edge];
        /* Skip the next check, an edge cannot be both a boundary edge and an inner edge. */
        continue;
      }
      const int i_new_inner_edge = new_inner_edge_indices.index_of_try(corner_edges[corner]);
      if (i_new_inner_edge != -1) {
        corner_edges[corner] = new_inner_edge_range[i_new_inner_edge];
      }
    }
  }

  /* Create the faces on the sides of extruded regions. */
  for (const int i : boundary_edge_indices.index_range()) {
    const int2 &boundary_edge = boundary_edges[i];
    const int new_vert_1 = boundary_edge[0];
    const int new_vert_2 = boundary_edge[1];
    const int extrude_index_1 = new_vert_1 - orig_vert_size;
    const int extrude_index_2 = new_vert_2 - orig_vert_size;

    const IndexRange extrude_poly = polys[edge_extruded_face_indices[i]];

    fill_quad_consistent_direction(corner_verts.slice(extrude_poly),
                                   corner_edges.slice(extrude_poly),
                                   new_corner_verts.slice(4 * i, 4),
                                   new_corner_edges.slice(4 * i, 4),
                                   new_vert_1,
                                   new_vert_2,
                                   new_vert_indices[extrude_index_1],
                                   new_vert_indices[extrude_index_2],
                                   boundary_edge_range[i],
                                   connect_edge_range[extrude_index_1],
                                   boundary_edge_indices[i],
                                   connect_edge_range[extrude_index_2]);
  }

  /* Create a map of indices in the extruded vertices array to all of the indices of edges
   * in the duplicate edges array that connect to that vertex. This can be used to simplify the
   * mixing of attribute data for the connecting edges. */
  const Array<Vector<int>> new_vert_to_duplicate_edge_map = create_vert_to_edge_map(
      new_vert_range.size(), boundary_edges, orig_vert_size);

  MutableAttributeAccessor attributes = mesh.attributes_for_write();

  attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (ELEM(id.name(), ".corner_vert", ".corner_edge", ".edge_verts")) {
      return true;
    }
    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);

    switch (attribute.domain) {
      case ATTR_DOMAIN_POINT: {
        /* New vertices copy the attributes from their original vertices. */
        bke::attribute_math::gather(
            attribute.span, new_vert_indices, attribute.span.slice(new_vert_range));
        break;
      }
      case ATTR_DOMAIN_EDGE: {
        /* Edges parallel to original edges copy the edge attributes from the original edges. */
        GMutableSpan boundary_data = attribute.span.slice(boundary_edge_range);
        bke::attribute_math::gather(attribute.span, boundary_edge_indices, boundary_data);

        /* Edges inside of face regions also just duplicate their source data. */
        GMutableSpan new_inner_data = attribute.span.slice(new_inner_edge_range);
        bke::attribute_math::gather(attribute.span, new_inner_edge_indices, new_inner_data);

        /* Edges connected to original vertices mix values of selected connected edges. */
        copy_with_mixing(
            boundary_data,
            [&](const int i) { return new_vert_to_duplicate_edge_map[i].as_span(); },
            attribute.span.slice(connect_edge_range));
        break;
      }
      case ATTR_DOMAIN_FACE: {
        /* New faces on the side of extrusions get the values from the corresponding selected
         * face. */
        GMutableSpan side_data = attribute.span.slice(side_poly_range);
        bke::attribute_math::gather(attribute.span, edge_extruded_face_indices, side_data);
        break;
      }
      case ATTR_DOMAIN_CORNER: {
        /* New corners get the values from the corresponding corner on the extruded face. */
        bke::attribute_math::convert_to_static_type(meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          MutableSpan<T> data = attribute.span.typed<T>();
          MutableSpan<T> new_data = data.slice(side_loop_range);
          threading::parallel_for(
              boundary_edge_indices.index_range(), 256, [&](const IndexRange range) {
                for (const int i_boundary_edge : range) {
                  const int2 &boundary_edge = boundary_edges[i_boundary_edge];
                  const int new_vert_1 = boundary_edge[0];
                  const int new_vert_2 = boundary_edge[1];
                  const int orig_vert_1 = new_vert_indices[new_vert_1 - orig_vert_size];
                  const int orig_vert_2 = new_vert_indices[new_vert_2 - orig_vert_size];

                  /* Retrieve the data for the first two sides of the quad from the extruded
                   * polygon, which we generally expect to have just a small amount of sides. This
                   * loop could be eliminated by adding a cache of connected loops (which would
                   * also simplify some of the other code to find the correct loops on the extruded
                   * face). */
                  T data_1;
                  T data_2;
                  for (const int i_loop : polys[edge_extruded_face_indices[i_boundary_edge]]) {
                    if (corner_verts[i_loop] == new_vert_1) {
                      data_1 = data[i_loop];
                    }
                    if (corner_verts[i_loop] == new_vert_2) {
                      data_2 = data[i_loop];
                    }
                  }

                  /* Instead of replicating the order in #fill_quad_consistent_direction here, it's
                   * simpler (though probably slower) to just match the corner data based on the
                   * vertex indices. */
                  for (const int i : IndexRange(4 * i_boundary_edge, 4)) {
                    if (ELEM(new_corner_verts[i], new_vert_1, orig_vert_1)) {
                      new_data[i] = data_1;
                    }
                    else if (ELEM(new_corner_verts[i], new_vert_2, orig_vert_2)) {
                      new_data[i] = data_2;
                    }
                  }
                }
              });
        });
        break;
      }
      default:
        BLI_assert_unreachable();
    }

    attribute.finish();
    return true;
  });

  /* Translate vertices based on the offset. If the vertex is used by a selected edge, it will
   * have been duplicated and only the new vertex should use the offset. Otherwise the vertex might
   * still need an offset, but it was reused on the inside of a region of extruded faces. */
  MutableSpan<float3> positions = mesh.vert_positions_for_write();
  if (poly_position_offsets.is_single()) {
    const float3 offset = poly_position_offsets.get_internal_single();
    threading::parallel_for(
        IndexRange(all_selected_verts.size()), 1024, [&](const IndexRange range) {
          for (const int i_orig : all_selected_verts.as_span().slice(range)) {
            const int i_new = new_vert_indices.index_of_try(i_orig);
            if (i_new == -1) {
              positions[i_orig] += offset;
            }
            else {
              positions[new_vert_range[i_new]] += offset;
            }
          }
        });
  }
  else {
    threading::parallel_for(
        IndexRange(all_selected_verts.size()), 1024, [&](const IndexRange range) {
          for (const int i_orig : all_selected_verts.as_span().slice(range)) {
            const int i_new = new_vert_indices.index_of_try(i_orig);
            const float3 offset = vert_offsets[i_orig];
            if (i_new == -1) {
              positions[i_orig] += offset;
            }
            else {
              positions[new_vert_range[i_new]] += offset;
            }
          }
        });
  }

  MutableSpan<int> vert_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_POINT);
  if (!vert_orig_indices.is_empty()) {
    array_utils::gather(vert_orig_indices.as_span(),
                        new_vert_indices.as_span(),
                        vert_orig_indices.slice(new_vert_range));
  }

  MutableSpan<int> edge_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_EDGE);
  if (!edge_orig_indices.is_empty()) {
    edge_orig_indices.slice(connect_edge_range).fill(ORIGINDEX_NONE);
    array_utils::gather(edge_orig_indices.as_span(),
                        new_inner_edge_indices.as_span(),
                        edge_orig_indices.slice(new_inner_edge_range));
    array_utils::gather(edge_orig_indices.as_span(),
                        boundary_edge_indices.as_span(),
                        edge_orig_indices.slice(boundary_edge_range));
  }

  MutableSpan<int> poly_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_FACE);
  if (!poly_orig_indices.is_empty()) {
    array_utils::gather(poly_orig_indices.as_span(),
                        edge_extruded_face_indices.as_span(),
                        poly_orig_indices.slice(side_poly_range));
  }

  if (attribute_outputs.top_id) {
    save_selection_as_attribute(
        mesh, attribute_outputs.top_id.get(), ATTR_DOMAIN_FACE, poly_selection);
  }
  if (attribute_outputs.side_id) {
    save_selection_as_attribute(
        mesh, attribute_outputs.side_id.get(), ATTR_DOMAIN_FACE, side_poly_range);
  }

  BKE_mesh_runtime_clear_cache(&mesh);
}

static void extrude_individual_mesh_faces(
    Mesh &mesh,
    const Field<bool> &selection_field,
    const Field<float3> &offset_field,
    const AttributeOutputs &attribute_outputs,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  const int orig_vert_size = mesh.totvert;
  const int orig_edge_size = mesh.totedge;
  const OffsetIndices orig_polys = mesh.polys();
  const Span<int> orig_corner_verts = mesh.corner_verts();
  const int orig_loop_size = orig_corner_verts.size();

  /* Use an array for the result of the evaluation because the mesh is reallocated before
   * the vertices are moved, and the evaluated result might reference an attribute. */
  Array<float3> poly_offset(orig_polys.size());
  const bke::MeshFieldContext poly_context{mesh, ATTR_DOMAIN_FACE};
  FieldEvaluator poly_evaluator{poly_context, mesh.totpoly};
  poly_evaluator.set_selection(selection_field);
  poly_evaluator.add_with_destination(offset_field, poly_offset.as_mutable_span());
  poly_evaluator.evaluate();
  const IndexMask poly_selection = poly_evaluator.get_evaluated_selection_as_mask();
  if (poly_selection.is_empty()) {
    return;
  }

  /* Build an array of offsets into the new data for each polygon. This is used to facilitate
   * parallelism later on by avoiding the need to keep track of an offset when iterating through
   * all polygons. */
  int extrude_corner_size = 0;
  Array<int> group_per_face_data(poly_selection.size() + 1);
  for (const int i_selection : poly_selection.index_range()) {
    group_per_face_data[i_selection] = extrude_corner_size;
    extrude_corner_size += orig_polys[poly_selection[i_selection]].size();
  }
  group_per_face_data.last() = extrude_corner_size;
  const OffsetIndices<int> group_per_face(group_per_face_data);

  const IndexRange new_vert_range{orig_vert_size, extrude_corner_size};
  /* One edge connects each selected vertex to a new vertex on the extruded polygons. */
  const IndexRange connect_edge_range{orig_edge_size, extrude_corner_size};
  /* Each selected edge is duplicated to form a single edge on the extrusion. */
  const IndexRange duplicate_edge_range = connect_edge_range.after(extrude_corner_size);
  /* Each edge selected for extrusion is extruded into a single face. */
  const IndexRange side_poly_range{orig_polys.size(), duplicate_edge_range.size()};
  const IndexRange side_loop_range{orig_loop_size, side_poly_range.size() * 4};

  remove_non_propagated_attributes(mesh.attributes_for_write(), propagation_info);
  expand_mesh(mesh,
              new_vert_range.size(),
              connect_edge_range.size() + duplicate_edge_range.size(),
              side_poly_range.size(),
              side_loop_range.size());

  MutableSpan<float3> new_positions = mesh.vert_positions_for_write().slice(new_vert_range);
  MutableSpan<int2> edges = mesh.edges_for_write();
  MutableSpan<int2> connect_edges = edges.slice(connect_edge_range);
  MutableSpan<int2> duplicate_edges = edges.slice(duplicate_edge_range);
  MutableSpan<int> poly_offsets = mesh.poly_offsets_for_write();
  MutableSpan<int> new_poly_offsets = poly_offsets.slice(side_poly_range);
  MutableSpan<int> corner_verts = mesh.corner_verts_for_write();
  MutableSpan<int> corner_edges = mesh.corner_edges_for_write();

  new_poly_offsets.fill(4);
  offset_indices::accumulate_counts_to_offsets(new_poly_offsets, orig_loop_size);
  const OffsetIndices polys = mesh.polys();

  /* For every selected polygon, change it to use the new extruded vertices and the duplicate
   * edges, and build the faces that form the sides of the extrusion. Build "original index"
   * arrays for the new vertices and edges so they can be accessed later.
   *
   * Filling some of this data like the new edges or polygons could be easily split into
   * separate loops, which may or may not be faster, but would involve more duplication. */
  Array<int> new_vert_indices(extrude_corner_size);
  Array<int> duplicate_edge_indices(extrude_corner_size);
  threading::parallel_for(poly_selection.index_range(), 256, [&](const IndexRange range) {
    for (const int i_selection : range) {
      const IndexRange extrude_range = group_per_face[i_selection];

      const IndexRange poly = polys[poly_selection[i_selection]];
      MutableSpan<int> poly_verts = corner_verts.slice(poly);
      MutableSpan<int> poly_edges = corner_edges.slice(poly);

      for (const int i : IndexRange(poly.size())) {
        const int i_extrude = extrude_range[i];
        new_vert_indices[i_extrude] = poly_verts[i];
        duplicate_edge_indices[i_extrude] = poly_edges[i];

        poly_verts[i] = new_vert_range[i_extrude];
        poly_edges[i] = duplicate_edge_range[i_extrude];
      }

      for (const int i : IndexRange(poly.size())) {
        const int i_next = (i == poly.size() - 1) ? 0 : i + 1;
        const int i_extrude = extrude_range[i];
        const int i_extrude_next = extrude_range[i_next];

        const int i_duplicate_edge = duplicate_edge_range[i_extrude];
        const int new_vert = new_vert_range[i_extrude];
        const int new_vert_next = new_vert_range[i_extrude_next];

        const int orig_edge = duplicate_edge_indices[i_extrude];

        const int orig_vert = new_vert_indices[i_extrude];
        const int orig_vert_next = new_vert_indices[i_extrude_next];

        duplicate_edges[i_extrude] = int2(new_vert, new_vert_next);

        MutableSpan<int> side_poly_verts = corner_verts.slice(side_loop_range[i_extrude * 4], 4);
        MutableSpan<int> side_poly_edges = corner_edges.slice(side_loop_range[i_extrude * 4], 4);
        side_poly_verts[0] = new_vert_next;
        side_poly_edges[0] = i_duplicate_edge;
        side_poly_verts[1] = new_vert;
        side_poly_edges[1] = connect_edge_range[i_extrude];
        side_poly_verts[2] = orig_vert;
        side_poly_edges[2] = orig_edge;
        side_poly_verts[3] = orig_vert_next;
        side_poly_edges[3] = connect_edge_range[i_extrude_next];

        connect_edges[i_extrude] = int2(orig_vert, new_vert);
      }
    }
  });

  MutableAttributeAccessor attributes = mesh.attributes_for_write();

  attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (ELEM(id.name(), ".corner_vert", ".corner_edge", ".edge_verts")) {
      return true;
    }
    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);

    switch (attribute.domain) {
      case ATTR_DOMAIN_POINT: {
        /* New vertices copy the attributes from their original vertices. */
        GMutableSpan new_data = attribute.span.slice(new_vert_range);
        bke::attribute_math::gather(attribute.span, new_vert_indices, new_data);
        break;
      }
      case ATTR_DOMAIN_EDGE: {
        /* The data for the duplicate edge is simply a copy of the original edge's data. */
        GMutableSpan duplicate_data = attribute.span.slice(duplicate_edge_range);
        bke::attribute_math::gather(attribute.span, duplicate_edge_indices, duplicate_data);

        bke::attribute_math::convert_to_static_type(meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          MutableSpan<T> data = attribute.span.typed<T>();
          MutableSpan<T> connect_data = data.slice(connect_edge_range);
          threading::parallel_for(poly_selection.index_range(), 512, [&](const IndexRange range) {
            for (const int i_selection : range) {
              const IndexRange poly = polys[poly_selection[i_selection]];
              const IndexRange extrude_range = group_per_face[i_selection];

              /* For the extruded edges, mix the data from the two neighboring original edges of
               * the extruded polygon. */
              for (const int i : IndexRange(poly.size())) {
                const int i_prev = (i == 0) ? poly.size() - 1 : i - 1;
                const int i_extrude = extrude_range[i];
                const int i_extrude_prev = extrude_range[i_prev];

                const int orig_edge = duplicate_edge_indices[i_extrude];
                const int orig_edge_prev = duplicate_edge_indices[i_extrude_prev];
                if constexpr (std::is_same_v<T, bool>) {
                  /* Propagate selections with "or" instead of "at least half". */
                  connect_data[i_extrude] = data[orig_edge] || data[orig_edge_prev];
                }
                else {
                  connect_data[i_extrude] = bke::attribute_math::mix2(
                      0.5f, data[orig_edge], data[orig_edge_prev]);
                }
              }
            }
          });
        });
        break;
      }
      case ATTR_DOMAIN_FACE: {
        /* Each side face gets the values from the corresponding new face. */
        bke::attribute_math::convert_to_static_type(meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          MutableSpan<T> data = attribute.span.typed<T>();
          MutableSpan<T> new_data = data.slice(side_poly_range);
          threading::parallel_for(poly_selection.index_range(), 1024, [&](const IndexRange range) {
            for (const int i_selection : range) {
              const int poly_index = poly_selection[i_selection];
              const IndexRange extrude_range = group_per_face[i_selection];
              new_data.slice(extrude_range).fill(data[poly_index]);
            }
          });
        });
        break;
      }
      case ATTR_DOMAIN_CORNER: {
        /* Each corner on a side face gets its value from the matching corner on an extruded
         * face. */
        bke::attribute_math::convert_to_static_type(meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          MutableSpan<T> data = attribute.span.typed<T>();
          MutableSpan<T> new_data = data.slice(side_loop_range);
          threading::parallel_for(poly_selection.index_range(), 256, [&](const IndexRange range) {
            for (const int i_selection : range) {
              const IndexRange poly = polys[poly_selection[i_selection]];
              const Span<T> poly_loop_data = data.slice(poly);
              const IndexRange extrude_range = group_per_face[i_selection];

              for (const int i : IndexRange(poly.size())) {
                const int i_next = (i == poly.size() - 1) ? 0 : i + 1;
                const int i_extrude = extrude_range[i];

                MutableSpan<T> side_loop_data = new_data.slice(i_extrude * 4, 4);

                /* The two corners on each side of the side polygon get the data from the matching
                 * corners of the extruded polygon. This order depends on the loop filling the loop
                 * indices. */
                side_loop_data[0] = poly_loop_data[i_next];
                side_loop_data[1] = poly_loop_data[i];
                side_loop_data[2] = poly_loop_data[i];
                side_loop_data[3] = poly_loop_data[i_next];
              }
            }
          });
        });
        break;
      }
      default:
        BLI_assert_unreachable();
    }

    attribute.finish();
    return true;
  });

  /* Offset the new vertices. */
  threading::parallel_for(poly_selection.index_range(), 1024, [&](const IndexRange range) {
    for (const int i_selection : range) {
      const IndexRange extrude_range = group_per_face[i_selection];
      for (float3 &position : new_positions.slice(extrude_range)) {
        position += poly_offset[poly_selection[i_selection]];
      }
    }
  });

  MutableSpan<int> vert_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_POINT);
  if (!vert_orig_indices.is_empty()) {
    array_utils::gather(vert_orig_indices.as_span(),
                        new_vert_indices.as_span(),
                        vert_orig_indices.slice(new_vert_range));
  }

  MutableSpan<int> edge_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_EDGE);
  if (!edge_orig_indices.is_empty()) {
    edge_orig_indices.slice(connect_edge_range).fill(ORIGINDEX_NONE);
    array_utils::gather(edge_orig_indices.as_span(),
                        duplicate_edge_indices.as_span(),
                        edge_orig_indices.slice(duplicate_edge_range));
  }

  MutableSpan<int> poly_orig_indices = get_orig_index_layer(mesh, ATTR_DOMAIN_FACE);
  if (!poly_orig_indices.is_empty()) {
    MutableSpan<int> new_poly_orig_indices = poly_orig_indices.slice(side_poly_range);
    threading::parallel_for(poly_selection.index_range(), 1024, [&](const IndexRange range) {
      for (const int selection_i : range) {
        const int poly_i = poly_selection[selection_i];
        const IndexRange extrude_range = group_per_face[selection_i];
        new_poly_orig_indices.slice(extrude_range).fill(poly_orig_indices[poly_i]);
      }
    });
  }

  if (attribute_outputs.top_id) {
    save_selection_as_attribute(
        mesh, attribute_outputs.top_id.get(), ATTR_DOMAIN_FACE, poly_selection);
  }
  if (attribute_outputs.side_id) {
    save_selection_as_attribute(
        mesh, attribute_outputs.side_id.get(), ATTR_DOMAIN_FACE, side_poly_range);
  }

  BKE_mesh_runtime_clear_cache(&mesh);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
  Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  Field<float3> offset_field = params.extract_input<Field<float3>>("Offset");
  Field<float> scale_field = params.extract_input<Field<float>>("Offset Scale");
  const NodeGeometryExtrudeMesh &storage = node_storage(params.node());
  GeometryNodeExtrudeMeshMode mode = GeometryNodeExtrudeMeshMode(storage.mode);

  /* Create a combined field from the offset and the scale so the field evaluator
   * can take care of the multiplication and to simplify each extrude function. */
  static auto multiply_fn = mf::build::SI2_SO<float3, float, float3>(
      "Scale",
      [](const float3 &offset, const float scale) { return offset * scale; },
      mf::build::exec_presets::AllSpanOrSingle());
  const Field<float3> final_offset{
      FieldOperation::Create(multiply_fn, {std::move(offset_field), std::move(scale_field)})};

  AttributeOutputs attribute_outputs;
  attribute_outputs.top_id = params.get_output_anonymous_attribute_id_if_needed("Top");
  attribute_outputs.side_id = params.get_output_anonymous_attribute_id_if_needed("Side");

  const bool extrude_individual = mode == GEO_NODE_EXTRUDE_MESH_FACES &&
                                  params.extract_input<bool>("Individual");

  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Mesh");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {

      switch (mode) {
        case GEO_NODE_EXTRUDE_MESH_VERTICES:
          extrude_mesh_vertices(
              *mesh, selection, final_offset, attribute_outputs, propagation_info);
          break;
        case GEO_NODE_EXTRUDE_MESH_EDGES:
          extrude_mesh_edges(*mesh, selection, final_offset, attribute_outputs, propagation_info);
          break;
        case GEO_NODE_EXTRUDE_MESH_FACES: {
          if (extrude_individual) {
            extrude_individual_mesh_faces(
                *mesh, selection, final_offset, attribute_outputs, propagation_info);
          }
          else {
            extrude_mesh_face_regions(
                *mesh, selection, final_offset, attribute_outputs, propagation_info);
          }
          break;
        }
      }
    }
  });

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_extrude_mesh_cc

void register_node_type_geo_extrude_mesh()
{
  namespace file_ns = blender::nodes::node_geo_extrude_mesh_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_EXTRUDE_MESH, "Extrude Mesh", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  node_type_storage(
      &ntype, "NodeGeometryExtrudeMesh", node_free_standard_storage, node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
