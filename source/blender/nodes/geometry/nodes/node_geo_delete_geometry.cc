/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.h"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_delete_geometry_cc {

template<typename T>
static void copy_data_based_on_map(const Span<T> src,
                                   const Span<int> index_map,
                                   MutableSpan<T> dst)
{
  threading::parallel_for(index_map.index_range(), 1024, [&](const IndexRange range) {
    for (const int i_src : range) {
      const int i_dst = index_map[i_src];
      if (i_dst != -1) {
        dst[i_dst] = src[i_src];
      }
    }
  });
}

/**
 * Copies the attributes with a domain in `domains` to `result_component`.
 */
static void copy_attributes(const Map<AttributeIDRef, AttributeKind> &attributes,
                            const bke::AttributeAccessor src_attributes,
                            bke::MutableAttributeAccessor dst_attributes,
                            const Span<eAttrDomain> domains)
{
  for (MapItem<AttributeIDRef, AttributeKind> entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    GAttributeReader attribute = src_attributes.lookup(attribute_id);
    if (!attribute) {
      continue;
    }
    /* Only copy if it is on a domain we want. */
    if (!domains.contains(attribute.domain)) {
      continue;
    }
    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute.varray.type());
    GSpanAttributeWriter result_attribute = dst_attributes.lookup_or_add_for_write_only_span(
        attribute_id, attribute.domain, data_type);
    if (!result_attribute) {
      continue;
    }
    attribute.varray.materialize(result_attribute.span.data());
    result_attribute.finish();
  }
}

/**
 * For each attribute with a domain in `domains` it copies the parts of that attribute which lie in
 * the mask to `result_component`.
 */
static void copy_attributes_based_on_mask(const Map<AttributeIDRef, AttributeKind> &attributes,
                                          const bke::AttributeAccessor src_attributes,
                                          bke::MutableAttributeAccessor dst_attributes,
                                          const eAttrDomain domain,
                                          const IndexMask mask)
{
  for (MapItem<AttributeIDRef, AttributeKind> entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    GAttributeReader attribute = src_attributes.lookup(attribute_id);
    if (!attribute) {
      continue;
    }
    /* Only copy if it is on a domain we want. */
    if (domain != attribute.domain) {
      continue;
    }
    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute.varray.type());
    GSpanAttributeWriter result_attribute = dst_attributes.lookup_or_add_for_write_only_span(
        attribute_id, attribute.domain, data_type);
    if (!result_attribute) {
      continue;
    }

    array_utils::gather(attribute.varray, mask, result_attribute.span);

    result_attribute.finish();
  }
}

static void copy_attributes_based_on_map(const Map<AttributeIDRef, AttributeKind> &attributes,
                                         const bke::AttributeAccessor src_attributes,
                                         bke::MutableAttributeAccessor dst_attributes,
                                         const eAttrDomain domain,
                                         const Span<int> index_map)
{
  for (MapItem<AttributeIDRef, AttributeKind> entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    GAttributeReader attribute = src_attributes.lookup(attribute_id);
    if (!attribute) {
      continue;
    }
    /* Only copy if it is on a domain we want. */
    if (domain != attribute.domain) {
      continue;
    }
    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute.varray.type());
    GSpanAttributeWriter result_attribute = dst_attributes.lookup_or_add_for_write_only_span(
        attribute_id, attribute.domain, data_type);
    if (!result_attribute) {
      continue;
    }

    bke::attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArraySpan<T> span{attribute.varray.typed<T>()};
      MutableSpan<T> out_span = result_attribute.span.typed<T>();
      copy_data_based_on_map(span, index_map, out_span);
    });
    result_attribute.finish();
  }
}

static void copy_face_corner_attributes(const Map<AttributeIDRef, AttributeKind> &attributes,
                                        const bke::AttributeAccessor src_attributes,
                                        bke::MutableAttributeAccessor dst_attributes,
                                        const int selected_loops_num,
                                        const Span<int> selected_poly_indices,
                                        const Mesh &mesh_in)
{
  const OffsetIndices polys = mesh_in.polys();
  Vector<int64_t> indices;
  indices.reserve(selected_loops_num);
  for (const int src_poly_index : selected_poly_indices) {
    for (const int corner : polys[src_poly_index]) {
      indices.append_unchecked(corner);
    }
  }
  copy_attributes_based_on_mask(
      attributes, src_attributes, dst_attributes, ATTR_DOMAIN_CORNER, IndexMask(indices));
}

static void copy_masked_edges_to_new_mesh(const Mesh &src_mesh, Mesh &dst_mesh, Span<int> edge_map)
{
  BLI_assert(src_mesh.totedge == edge_map.size());
  const Span<int2> src_edges = src_mesh.edges();
  MutableSpan<int2> dst_edges = dst_mesh.edges_for_write();

  threading::parallel_for(src_edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int i_src : range) {
      const int i_dst = edge_map[i_src];
      if (ELEM(i_dst, -1, -2)) {
        continue;
      }
      dst_edges[i_dst] = src_edges[i_src];
    }
  });
}

static void copy_masked_edges_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map,
                                          Span<int> edge_map)
{
  BLI_assert(src_mesh.totvert == vertex_map.size());
  BLI_assert(src_mesh.totedge == edge_map.size());
  const Span<int2> src_edges = src_mesh.edges();
  MutableSpan<int2> dst_edges = dst_mesh.edges_for_write();

  threading::parallel_for(src_edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int i_src : range) {
      const int i_dst = edge_map[i_src];
      if (i_dst == -1) {
        continue;
      }

      dst_edges[i_dst][0] = vertex_map[src_edges[i_src][0]];
      dst_edges[i_dst][1] = vertex_map[src_edges[i_src][1]];
    }
  });
}

/* Faces and edges changed but vertices are the same. */
static void copy_masked_polys_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> edge_map,
                                          Span<int> masked_poly_indices,
                                          Span<int> new_loop_starts)
{
  const OffsetIndices src_polys = src_mesh.polys();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const Span<int> src_corner_edges = src_mesh.corner_edges();
  MutableSpan<int> dst_poly_offsets = dst_mesh.poly_offsets_for_write();
  MutableSpan<int> dst_corner_verts = dst_mesh.corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = dst_mesh.corner_edges_for_write();

  threading::parallel_for(masked_poly_indices.index_range(), 512, [&](const IndexRange range) {
    for (const int i_dst : range) {
      const int i_src = masked_poly_indices[i_dst];
      const IndexRange poly_src = src_polys[i_src];
      const Span<int> src_poly_verts = src_corner_verts.slice(poly_src);
      const Span<int> src_poly_edges = src_corner_edges.slice(poly_src);

      dst_poly_offsets[i_dst] = new_loop_starts[i_dst];
      MutableSpan<int> dst_poly_verts = dst_corner_verts.slice(dst_poly_offsets[i_dst],
                                                               poly_src.size());
      MutableSpan<int> dst_poly_edges = dst_corner_edges.slice(dst_poly_offsets[i_dst],
                                                               poly_src.size());

      dst_poly_verts.copy_from(src_poly_verts);

      for (const int i : IndexRange(poly_src.size())) {
        dst_poly_edges[i] = edge_map[src_poly_edges[i]];
      }
    }
  });
}

/* Only faces changed. */
static void copy_masked_polys_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> masked_poly_indices,
                                          Span<int> new_loop_starts)
{
  const OffsetIndices src_polys = src_mesh.polys();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const Span<int> src_corner_edges = src_mesh.corner_edges();
  MutableSpan<int> dst_poly_offsets = dst_mesh.poly_offsets_for_write();
  MutableSpan<int> dst_corner_verts = dst_mesh.corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = dst_mesh.corner_edges_for_write();

  threading::parallel_for(masked_poly_indices.index_range(), 512, [&](const IndexRange range) {
    for (const int i_dst : range) {
      const int i_src = masked_poly_indices[i_dst];
      const IndexRange poly_src = src_polys[i_src];
      const Span<int> src_poly_verts = src_corner_verts.slice(poly_src);
      const Span<int> src_poly_edges = src_corner_edges.slice(poly_src);

      dst_poly_offsets[i_dst] = new_loop_starts[i_dst];
      MutableSpan<int> dst_poly_verts = dst_corner_verts.slice(dst_poly_offsets[i_dst],
                                                               poly_src.size());
      MutableSpan<int> dst_poly_edges = dst_corner_edges.slice(dst_poly_offsets[i_dst],
                                                               poly_src.size());

      dst_poly_verts.copy_from(src_poly_verts);
      dst_poly_edges.copy_from(src_poly_edges);
    }
  });
}

static void copy_masked_polys_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map,
                                          Span<int> edge_map,
                                          Span<int> masked_poly_indices,
                                          Span<int> new_loop_starts)
{
  const OffsetIndices src_polys = src_mesh.polys();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const Span<int> src_corner_edges = src_mesh.corner_edges();
  MutableSpan<int> dst_poly_offsets = dst_mesh.poly_offsets_for_write();
  MutableSpan<int> dst_corner_verts = dst_mesh.corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = dst_mesh.corner_edges_for_write();

  threading::parallel_for(masked_poly_indices.index_range(), 512, [&](const IndexRange range) {
    for (const int i_dst : range) {
      const int i_src = masked_poly_indices[i_dst];
      const IndexRange poly_src = src_polys[i_src];
      const Span<int> src_poly_verts = src_corner_verts.slice(poly_src);
      const Span<int> src_poly_edges = src_corner_edges.slice(poly_src);

      dst_poly_offsets[i_dst] = new_loop_starts[i_dst];
      MutableSpan<int> dst_poly_verts = dst_corner_verts.slice(dst_poly_offsets[i_dst],
                                                               poly_src.size());
      MutableSpan<int> dst_poly_edges = dst_corner_edges.slice(dst_poly_offsets[i_dst],
                                                               poly_src.size());

      for (const int i : IndexRange(poly_src.size())) {
        dst_poly_verts[i] = vertex_map[src_poly_verts[i]];
      }
      for (const int i : IndexRange(poly_src.size())) {
        dst_poly_edges[i] = edge_map[src_poly_edges[i]];
      }
    }
  });
}

static void delete_curves_selection(GeometrySet &geometry_set,
                                    const Field<bool> &selection_field,
                                    const eAttrDomain selection_domain,
                                    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const Curves &src_curves_id = *geometry_set.get_curves_for_read();
  const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();

  const int domain_size = src_curves.attributes().domain_size(selection_domain);
  const bke::CurvesFieldContext field_context{src_curves, selection_domain};
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    return;
  }
  if (selection.size() == domain_size) {
    geometry_set.remove<CurveComponent>();
    return;
  }

  CurveComponent &component = geometry_set.get_component_for_write<CurveComponent>();
  Curves &curves_id = *component.get_for_write();
  bke::CurvesGeometry &curves = curves_id.geometry.wrap();

  if (selection_domain == ATTR_DOMAIN_POINT) {
    curves.remove_points(selection, propagation_info);
  }
  else if (selection_domain == ATTR_DOMAIN_CURVE) {
    curves.remove_curves(selection, propagation_info);
  }
}

static void separate_point_cloud_selection(
    GeometrySet &geometry_set,
    const Field<bool> &selection_field,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  const PointCloud &src_pointcloud = *geometry_set.get_pointcloud_for_read();

  const bke::PointCloudFieldContext field_context{src_pointcloud};
  fn::FieldEvaluator evaluator{field_context, src_pointcloud.totpoint};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    geometry_set.replace_pointcloud(nullptr);
    return;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(selection.size());

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation({GEO_COMPONENT_TYPE_POINT_CLOUD},
                                                 GEO_COMPONENT_TYPE_POINT_CLOUD,
                                                 false,
                                                 propagation_info,
                                                 attributes);

  copy_attributes_based_on_mask(attributes,
                                src_pointcloud.attributes(),
                                pointcloud->attributes_for_write(),
                                ATTR_DOMAIN_POINT,
                                selection);
  geometry_set.replace_pointcloud(pointcloud);
}

static void delete_selected_instances(GeometrySet &geometry_set,
                                      const Field<bool> &selection_field,
                                      const AnonymousAttributePropagationInfo &propagation_info)
{
  bke::Instances &instances = *geometry_set.get_instances_for_write();
  bke::InstancesFieldContext field_context{instances};

  fn::FieldEvaluator evaluator{field_context, instances.instances_num()};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    geometry_set.remove<InstancesComponent>();
    return;
  }

  instances.remove(selection, propagation_info);
}

static void compute_selected_verts_from_vertex_selection(const Span<bool> vertex_selection,
                                                         MutableSpan<int> r_vertex_map,
                                                         int *r_selected_verts_num)
{
  BLI_assert(vertex_selection.size() == r_vertex_map.size());

  int selected_verts_num = 0;
  for (const int i : r_vertex_map.index_range()) {
    if (vertex_selection[i]) {
      r_vertex_map[i] = selected_verts_num;
      selected_verts_num++;
    }
    else {
      r_vertex_map[i] = -1;
    }
  }

  *r_selected_verts_num = selected_verts_num;
}

static void compute_selected_edges_from_vertex_selection(const Mesh &mesh,
                                                         const Span<bool> vertex_selection,
                                                         MutableSpan<int> r_edge_map,
                                                         int *r_selected_edges_num)
{
  BLI_assert(mesh.totedge == r_edge_map.size());
  const Span<int2> edges = mesh.edges();

  int selected_edges_num = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    const int2 &edge = edges[i];

    /* Only add the edge if both vertices will be in the new mesh. */
    if (vertex_selection[edge[0]] && vertex_selection[edge[1]]) {
      r_edge_map[i] = selected_edges_num;
      selected_edges_num++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_selected_edges_num = selected_edges_num;
}

static void compute_selected_polys_from_vertex_selection(const Mesh &mesh,
                                                         const Span<bool> vertex_selection,
                                                         Vector<int> &r_selected_poly_indices,
                                                         Vector<int> &r_loop_starts,
                                                         int *r_selected_polys_num,
                                                         int *r_selected_loops_num)
{
  BLI_assert(mesh.totvert == vertex_selection.size());
  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_verts = mesh.corner_verts();

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  for (const int i : polys.index_range()) {
    const IndexRange poly_src = polys[i];

    bool all_verts_in_selection = true;
    for (const int vert : corner_verts.slice(poly_src)) {
      if (!vertex_selection[vert]) {
        all_verts_in_selection = false;
        break;
      }
    }

    if (all_verts_in_selection) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.size();
    }
  }

  *r_selected_polys_num = r_selected_poly_indices.size();
  *r_selected_loops_num = selected_loops_num;
}

/**
 * Checks for every edge if it is in `edge_selection`. If it is, then the two vertices of the
 * edge are kept along with the edge.
 */
static void compute_selected_verts_and_edges_from_edge_selection(const Mesh &mesh,
                                                                 const Span<bool> edge_selection,
                                                                 MutableSpan<int> r_vertex_map,
                                                                 MutableSpan<int> r_edge_map,
                                                                 int *r_selected_verts_num,
                                                                 int *r_selected_edges_num)
{
  BLI_assert(mesh.totedge == edge_selection.size());
  const Span<int2> edges = mesh.edges();

  int selected_edges_num = 0;
  int selected_verts_num = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    const int2 &edge = edges[i];
    if (edge_selection[i]) {
      r_edge_map[i] = selected_edges_num;
      selected_edges_num++;
      if (r_vertex_map[edge[0]] == -1) {
        r_vertex_map[edge[0]] = selected_verts_num;
        selected_verts_num++;
      }
      if (r_vertex_map[edge[1]] == -1) {
        r_vertex_map[edge[1]] = selected_verts_num;
        selected_verts_num++;
      }
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_selected_verts_num = selected_verts_num;
  *r_selected_edges_num = selected_edges_num;
}

/**
 * Checks for every edge if it is in `edge_selection`.
 */
static void compute_selected_edges_from_edge_selection(const Mesh &mesh,
                                                       const Span<bool> edge_selection,
                                                       MutableSpan<int> r_edge_map,
                                                       int *r_selected_edges_num)
{
  BLI_assert(mesh.totedge == edge_selection.size());

  int selected_edges_num = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    if (edge_selection[i]) {
      r_edge_map[i] = selected_edges_num;
      selected_edges_num++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_selected_edges_num = selected_edges_num;
}

/**
 * Checks for every polygon if all the edges are in `edge_selection`. If they are, then that
 * polygon is kept.
 */
static void compute_selected_polys_from_edge_selection(const Mesh &mesh,
                                                       const Span<bool> edge_selection,
                                                       Vector<int> &r_selected_poly_indices,
                                                       Vector<int> &r_loop_starts,
                                                       int *r_selected_polys_num,
                                                       int *r_selected_loops_num)
{
  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_edges = mesh.corner_edges();

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  for (const int i : polys.index_range()) {
    const IndexRange poly_src = polys[i];

    bool all_edges_in_selection = true;
    for (const int edge : corner_edges.slice(poly_src)) {
      if (!edge_selection[edge]) {
        all_edges_in_selection = false;
        break;
      }
    }

    if (all_edges_in_selection) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.size();
    }
  }

  *r_selected_polys_num = r_selected_poly_indices.size();
  *r_selected_loops_num = selected_loops_num;
}

/**
 * Checks for every edge and polygon if all its vertices are in `vertex_selection`.
 */
static void compute_selected_mesh_data_from_vertex_selection_edge_face(
    const Mesh &mesh,
    const Span<bool> vertex_selection,
    MutableSpan<int> r_edge_map,
    Vector<int> &r_selected_poly_indices,
    Vector<int> &r_loop_starts,
    int *r_selected_edges_num,
    int *r_selected_polys_num,
    int *r_selected_loops_num)
{
  threading::parallel_invoke(
      mesh.totedge > 1000,
      [&]() {
        compute_selected_edges_from_vertex_selection(
            mesh, vertex_selection, r_edge_map, r_selected_edges_num);
      },
      [&]() {
        compute_selected_polys_from_vertex_selection(mesh,
                                                     vertex_selection,
                                                     r_selected_poly_indices,
                                                     r_loop_starts,
                                                     r_selected_polys_num,
                                                     r_selected_loops_num);
      });
}

/**
 * Checks for every vertex if it is in `vertex_selection`. The polygons and edges are kept if all
 * vertices of that polygon or edge are in the selection.
 */
static void compute_selected_mesh_data_from_vertex_selection(const Mesh &mesh,
                                                             const Span<bool> vertex_selection,
                                                             MutableSpan<int> r_vertex_map,
                                                             MutableSpan<int> r_edge_map,
                                                             Vector<int> &r_selected_poly_indices,
                                                             Vector<int> &r_loop_starts,
                                                             int *r_selected_verts_num,
                                                             int *r_selected_edges_num,
                                                             int *r_selected_polys_num,
                                                             int *r_selected_loops_num)
{
  threading::parallel_invoke(
      mesh.totedge > 1000,
      [&]() {
        compute_selected_verts_from_vertex_selection(
            vertex_selection, r_vertex_map, r_selected_verts_num);
      },
      [&]() {
        compute_selected_edges_from_vertex_selection(
            mesh, vertex_selection, r_edge_map, r_selected_edges_num);
      },
      [&]() {
        compute_selected_polys_from_vertex_selection(mesh,
                                                     vertex_selection,
                                                     r_selected_poly_indices,
                                                     r_loop_starts,
                                                     r_selected_polys_num,
                                                     r_selected_loops_num);
      });
}

/**
 * Checks for every edge if it is in `edge_selection`. The polygons are kept if all edges are in
 * the selection.
 */
static void compute_selected_mesh_data_from_edge_selection_edge_face(
    const Mesh &mesh,
    const Span<bool> edge_selection,
    MutableSpan<int> r_edge_map,
    Vector<int> &r_selected_poly_indices,
    Vector<int> &r_loop_starts,
    int *r_selected_edges_num,
    int *r_selected_polys_num,
    int *r_selected_loops_num)
{
  threading::parallel_invoke(
      mesh.totedge > 1000,
      [&]() {
        compute_selected_edges_from_edge_selection(
            mesh, edge_selection, r_edge_map, r_selected_edges_num);
      },
      [&]() {
        compute_selected_polys_from_edge_selection(mesh,
                                                   edge_selection,
                                                   r_selected_poly_indices,
                                                   r_loop_starts,
                                                   r_selected_polys_num,
                                                   r_selected_loops_num);
      });
}

/**
 * Checks for every edge if it is in `edge_selection`. If it is, the vertices belonging to
 * that edge are kept as well. The polys are kept if all edges are in the selection.
 */
static void compute_selected_mesh_data_from_edge_selection(const Mesh &mesh,
                                                           const Span<bool> edge_selection,
                                                           MutableSpan<int> r_vertex_map,
                                                           MutableSpan<int> r_edge_map,
                                                           Vector<int> &r_selected_poly_indices,
                                                           Vector<int> &r_loop_starts,
                                                           int *r_selected_verts_num,
                                                           int *r_selected_edges_num,
                                                           int *r_selected_polys_num,
                                                           int *r_selected_loops_num)
{
  threading::parallel_invoke(
      mesh.totedge > 1000,
      [&]() {
        r_vertex_map.fill(-1);
        compute_selected_verts_and_edges_from_edge_selection(mesh,
                                                             edge_selection,
                                                             r_vertex_map,
                                                             r_edge_map,
                                                             r_selected_verts_num,
                                                             r_selected_edges_num);
      },
      [&]() {
        compute_selected_polys_from_edge_selection(mesh,
                                                   edge_selection,
                                                   r_selected_poly_indices,
                                                   r_loop_starts,
                                                   r_selected_polys_num,
                                                   r_selected_loops_num);
      });
}

/**
 * Checks for every polygon if it is in `poly_selection`.
 */
static void compute_selected_polys_from_poly_selection(const Mesh &mesh,
                                                       const Span<bool> poly_selection,
                                                       Vector<int> &r_selected_poly_indices,
                                                       Vector<int> &r_loop_starts,
                                                       int *r_selected_polys_num,
                                                       int *r_selected_loops_num)
{
  BLI_assert(mesh.totpoly == poly_selection.size());
  const OffsetIndices polys = mesh.polys();

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  for (const int i : polys.index_range()) {
    const IndexRange poly_src = polys[i];
    /* We keep this one. */
    if (poly_selection[i]) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.size();
    }
  }
  *r_selected_polys_num = r_selected_poly_indices.size();
  *r_selected_loops_num = selected_loops_num;
}
/**
 * Checks for every polygon if it is in `poly_selection`. If it is, the edges
 * belonging to that polygon are kept as well.
 */
static void compute_selected_mesh_data_from_poly_selection_edge_face(
    const Mesh &mesh,
    const Span<bool> poly_selection,
    MutableSpan<int> r_edge_map,
    Vector<int> &r_selected_poly_indices,
    Vector<int> &r_loop_starts,
    int *r_selected_edges_num,
    int *r_selected_polys_num,
    int *r_selected_loops_num)
{
  BLI_assert(mesh.totpoly == poly_selection.size());
  BLI_assert(mesh.totedge == r_edge_map.size());
  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_edges = mesh.corner_edges();

  r_edge_map.fill(-1);

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  int selected_edges_num = 0;
  for (const int i : polys.index_range()) {
    const IndexRange poly_src = polys[i];
    /* We keep this one. */
    if (poly_selection[i]) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.size();

      /* Add the vertices and the edges. */
      for (const int edge : corner_edges.slice(poly_src)) {
        /* Check first if it has not yet been added. */
        if (r_edge_map[edge] == -1) {
          r_edge_map[edge] = selected_edges_num;
          selected_edges_num++;
        }
      }
    }
  }
  *r_selected_edges_num = selected_edges_num;
  *r_selected_polys_num = r_selected_poly_indices.size();
  *r_selected_loops_num = selected_loops_num;
}

/**
 * Checks for every polygon if it is in `poly_selection`. If it is, the edges and vertices
 * belonging to that polygon are kept as well.
 */
static void compute_selected_mesh_data_from_poly_selection(const Mesh &mesh,
                                                           const Span<bool> poly_selection,
                                                           MutableSpan<int> r_vertex_map,
                                                           MutableSpan<int> r_edge_map,
                                                           Vector<int> &r_selected_poly_indices,
                                                           Vector<int> &r_loop_starts,
                                                           int *r_selected_verts_num,
                                                           int *r_selected_edges_num,
                                                           int *r_selected_polys_num,
                                                           int *r_selected_loops_num)
{
  BLI_assert(mesh.totpoly == poly_selection.size());
  BLI_assert(mesh.totedge == r_edge_map.size());
  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();

  r_vertex_map.fill(-1);
  r_edge_map.fill(-1);

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  int selected_verts_num = 0;
  int selected_edges_num = 0;
  for (const int i : polys.index_range()) {
    const IndexRange poly_src = polys[i];
    /* We keep this one. */
    if (poly_selection[i]) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.size();

      /* Add the vertices and the edges. */
      for (const int corner : poly_src) {
        const int vert = corner_verts[corner];
        const int edge = corner_edges[corner];
        /* Check first if it has not yet been added. */
        if (r_vertex_map[vert] == -1) {
          r_vertex_map[vert] = selected_verts_num;
          selected_verts_num++;
        }
        if (r_edge_map[edge] == -1) {
          r_edge_map[edge] = selected_edges_num;
          selected_edges_num++;
        }
      }
    }
  }
  *r_selected_verts_num = selected_verts_num;
  *r_selected_edges_num = selected_edges_num;
  *r_selected_polys_num = r_selected_poly_indices.size();
  *r_selected_loops_num = selected_loops_num;
}

/**
 * Keep the parts of the mesh that are in the selection.
 */
static void do_mesh_separation(GeometrySet &geometry_set,
                               const Mesh &mesh_in,
                               const Span<bool> selection,
                               const eAttrDomain domain,
                               const GeometryNodeDeleteGeometryMode mode,
                               const AnonymousAttributePropagationInfo &propagation_info)
{
  /* Needed in all cases. */
  Vector<int> selected_poly_indices;
  Vector<int> new_loop_starts;
  int selected_polys_num = 0;
  int selected_loops_num = 0;

  Mesh *mesh_out;

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_MESH}, GEO_COMPONENT_TYPE_MESH, false, propagation_info, attributes);
  attributes.remove(".edge_verts");
  attributes.remove(".corner_vert");
  attributes.remove(".corner_edge");

  switch (mode) {
    case GEO_NODE_DELETE_GEOMETRY_MODE_ALL: {
      Array<int> vertex_map(mesh_in.totvert);
      int selected_verts_num = 0;

      Array<int> edge_map(mesh_in.totedge);
      int selected_edges_num = 0;

      /* Fill all the maps based on the selection. */
      switch (domain) {
        case ATTR_DOMAIN_POINT:
          compute_selected_mesh_data_from_vertex_selection(mesh_in,
                                                           selection,
                                                           vertex_map,
                                                           edge_map,
                                                           selected_poly_indices,
                                                           new_loop_starts,
                                                           &selected_verts_num,
                                                           &selected_edges_num,
                                                           &selected_polys_num,
                                                           &selected_loops_num);
          break;
        case ATTR_DOMAIN_EDGE:
          compute_selected_mesh_data_from_edge_selection(mesh_in,
                                                         selection,
                                                         vertex_map,
                                                         edge_map,
                                                         selected_poly_indices,
                                                         new_loop_starts,
                                                         &selected_verts_num,
                                                         &selected_edges_num,
                                                         &selected_polys_num,
                                                         &selected_loops_num);
          break;
        case ATTR_DOMAIN_FACE:
          compute_selected_mesh_data_from_poly_selection(mesh_in,
                                                         selection,
                                                         vertex_map,
                                                         edge_map,
                                                         selected_poly_indices,
                                                         new_loop_starts,
                                                         &selected_verts_num,
                                                         &selected_edges_num,
                                                         &selected_polys_num,
                                                         &selected_loops_num);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
      mesh_out = BKE_mesh_new_nomain_from_template(&mesh_in,
                                                   selected_verts_num,
                                                   selected_edges_num,
                                                   selected_polys_num,
                                                   selected_loops_num);

      /* Copy the selected parts of the mesh over to the new mesh. */
      copy_masked_edges_to_new_mesh(mesh_in, *mesh_out, vertex_map, edge_map);
      copy_masked_polys_to_new_mesh(
          mesh_in, *mesh_out, vertex_map, edge_map, selected_poly_indices, new_loop_starts);

      /* Copy attributes. */
      copy_attributes_based_on_map(attributes,
                                   mesh_in.attributes(),
                                   mesh_out->attributes_for_write(),
                                   ATTR_DOMAIN_POINT,
                                   vertex_map);
      copy_attributes_based_on_map(attributes,
                                   mesh_in.attributes(),
                                   mesh_out->attributes_for_write(),
                                   ATTR_DOMAIN_EDGE,
                                   edge_map);
      copy_attributes_based_on_mask(attributes,
                                    mesh_in.attributes(),
                                    mesh_out->attributes_for_write(),
                                    ATTR_DOMAIN_FACE,
                                    IndexMask(Vector<int64_t>(selected_poly_indices.as_span())));
      copy_face_corner_attributes(attributes,
                                  mesh_in.attributes(),
                                  mesh_out->attributes_for_write(),
                                  selected_loops_num,
                                  selected_poly_indices,
                                  mesh_in);
      break;
    }
    case GEO_NODE_DELETE_GEOMETRY_MODE_EDGE_FACE: {
      Array<int> edge_map(mesh_in.totedge);
      int selected_edges_num = 0;

      /* Fill all the maps based on the selection. */
      switch (domain) {
        case ATTR_DOMAIN_POINT:
          compute_selected_mesh_data_from_vertex_selection_edge_face(mesh_in,
                                                                     selection,
                                                                     edge_map,
                                                                     selected_poly_indices,
                                                                     new_loop_starts,
                                                                     &selected_edges_num,
                                                                     &selected_polys_num,
                                                                     &selected_loops_num);
          break;
        case ATTR_DOMAIN_EDGE:
          compute_selected_mesh_data_from_edge_selection_edge_face(mesh_in,
                                                                   selection,
                                                                   edge_map,
                                                                   selected_poly_indices,
                                                                   new_loop_starts,
                                                                   &selected_edges_num,
                                                                   &selected_polys_num,
                                                                   &selected_loops_num);
          break;
        case ATTR_DOMAIN_FACE:
          compute_selected_mesh_data_from_poly_selection_edge_face(mesh_in,
                                                                   selection,
                                                                   edge_map,
                                                                   selected_poly_indices,
                                                                   new_loop_starts,
                                                                   &selected_edges_num,
                                                                   &selected_polys_num,
                                                                   &selected_loops_num);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
      mesh_out = BKE_mesh_new_nomain_from_template(
          &mesh_in, mesh_in.totvert, selected_edges_num, selected_polys_num, selected_loops_num);

      /* Copy the selected parts of the mesh over to the new mesh. */
      copy_masked_edges_to_new_mesh(mesh_in, *mesh_out, edge_map);
      copy_masked_polys_to_new_mesh(
          mesh_in, *mesh_out, edge_map, selected_poly_indices, new_loop_starts);

      /* Copy attributes. */
      copy_attributes(
          attributes, mesh_in.attributes(), mesh_out->attributes_for_write(), {ATTR_DOMAIN_POINT});
      copy_attributes_based_on_map(attributes,
                                   mesh_in.attributes(),
                                   mesh_out->attributes_for_write(),
                                   ATTR_DOMAIN_EDGE,
                                   edge_map);
      copy_attributes_based_on_mask(attributes,
                                    mesh_in.attributes(),
                                    mesh_out->attributes_for_write(),
                                    ATTR_DOMAIN_FACE,
                                    IndexMask(Vector<int64_t>(selected_poly_indices.as_span())));
      copy_face_corner_attributes(attributes,
                                  mesh_in.attributes(),
                                  mesh_out->attributes_for_write(),
                                  selected_loops_num,
                                  selected_poly_indices,
                                  mesh_in);

      /* Positions are not changed by the operation, so the bounds are the same. */
      mesh_out->runtime->bounds_cache = mesh_in.runtime->bounds_cache;
      break;
    }
    case GEO_NODE_DELETE_GEOMETRY_MODE_ONLY_FACE: {
      /* Fill all the maps based on the selection. */
      switch (domain) {
        case ATTR_DOMAIN_POINT:
          compute_selected_polys_from_vertex_selection(mesh_in,
                                                       selection,
                                                       selected_poly_indices,
                                                       new_loop_starts,
                                                       &selected_polys_num,
                                                       &selected_loops_num);
          break;
        case ATTR_DOMAIN_EDGE:
          compute_selected_polys_from_edge_selection(mesh_in,
                                                     selection,
                                                     selected_poly_indices,
                                                     new_loop_starts,
                                                     &selected_polys_num,
                                                     &selected_loops_num);
          break;
        case ATTR_DOMAIN_FACE:
          compute_selected_polys_from_poly_selection(mesh_in,
                                                     selection,
                                                     selected_poly_indices,
                                                     new_loop_starts,
                                                     &selected_polys_num,
                                                     &selected_loops_num);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
      mesh_out = BKE_mesh_new_nomain_from_template(
          &mesh_in, mesh_in.totvert, mesh_in.totedge, selected_polys_num, selected_loops_num);

      /* Copy the selected parts of the mesh over to the new mesh. */
      mesh_out->edges_for_write().copy_from(mesh_in.edges());
      copy_masked_polys_to_new_mesh(mesh_in, *mesh_out, selected_poly_indices, new_loop_starts);

      /* Copy attributes. */
      copy_attributes(attributes,
                      mesh_in.attributes(),
                      mesh_out->attributes_for_write(),
                      {ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE});
      copy_attributes_based_on_mask(attributes,
                                    mesh_in.attributes(),
                                    mesh_out->attributes_for_write(),
                                    ATTR_DOMAIN_FACE,
                                    IndexMask(Vector<int64_t>(selected_poly_indices.as_span())));
      copy_face_corner_attributes(attributes,
                                  mesh_in.attributes(),
                                  mesh_out->attributes_for_write(),
                                  selected_loops_num,
                                  selected_poly_indices,
                                  mesh_in);

      /* Positions are not changed by the operation, so the bounds are the same. */
      mesh_out->runtime->bounds_cache = mesh_in.runtime->bounds_cache;
      break;
    }
  }

  geometry_set.replace_mesh(mesh_out);
}

static void separate_mesh_selection(GeometrySet &geometry_set,
                                    const Field<bool> &selection_field,
                                    const eAttrDomain selection_domain,
                                    const GeometryNodeDeleteGeometryMode mode,
                                    const AnonymousAttributePropagationInfo &propagation_info)
{
  const Mesh &src_mesh = *geometry_set.get_mesh_for_read();
  const bke::MeshFieldContext field_context{src_mesh, selection_domain};
  fn::FieldEvaluator evaluator{field_context, src_mesh.attributes().domain_size(selection_domain)};
  evaluator.add(selection_field);
  evaluator.evaluate();
  const VArray<bool> selection = evaluator.get_evaluated<bool>(0);
  /* Check if there is anything to delete. */
  if (selection.is_empty() || (selection.is_single() && selection.get_internal_single())) {
    return;
  }

  const VArraySpan<bool> selection_span{selection};

  do_mesh_separation(
      geometry_set, src_mesh, selection_span, selection_domain, mode, propagation_info);
}

}  // namespace blender::nodes::node_geo_delete_geometry_cc

namespace blender::nodes {

void separate_geometry(GeometrySet &geometry_set,
                       const eAttrDomain domain,
                       const GeometryNodeDeleteGeometryMode mode,
                       const Field<bool> &selection_field,
                       const AnonymousAttributePropagationInfo &propagation_info,
                       bool &r_is_error)
{
  namespace file_ns = blender::nodes::node_geo_delete_geometry_cc;

  bool some_valid_domain = false;
  if (geometry_set.has_pointcloud()) {
    if (domain == ATTR_DOMAIN_POINT) {
      file_ns::separate_point_cloud_selection(geometry_set, selection_field, propagation_info);
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_mesh()) {
    if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_FACE, ATTR_DOMAIN_CORNER)) {
      file_ns::separate_mesh_selection(
          geometry_set, selection_field, domain, mode, propagation_info);
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_curves()) {
    if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE)) {
      file_ns::delete_curves_selection(
          geometry_set, fn::invert_boolean_field(selection_field), domain, propagation_info);
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_instances()) {
    if (domain == ATTR_DOMAIN_INSTANCE) {
      file_ns::delete_selected_instances(geometry_set, selection_field, propagation_info);
      some_valid_domain = true;
    }
  }
  r_is_error = !some_valid_domain && geometry_set.has_realized_data();
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_delete_geometry_cc {

NODE_STORAGE_FUNCS(NodeGeometryDeleteGeometry)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection")
      .default_value(true)
      .hide_value()
      .field_on_all()
      .description("The parts of the geometry to be deleted");
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  const bNode *node = static_cast<bNode *>(ptr->data);
  const NodeGeometryDeleteGeometry &storage = node_storage(*node);
  const eAttrDomain domain = eAttrDomain(storage.domain);

  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
  /* Only show the mode when it is relevant. */
  if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_FACE)) {
    uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryDeleteGeometry *data = MEM_cnew<NodeGeometryDeleteGeometry>(__func__);
  data->domain = ATTR_DOMAIN_POINT;
  data->mode = GEO_NODE_DELETE_GEOMETRY_MODE_ALL;

  node->storage = data;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  /* The node's input is a selection of elements that should be deleted, but the code is
   * implemented as a separation operation that copies the selected elements to a new geometry.
   * Invert the selection to avoid the need to keep track of both cases in the code. */
  const Field<bool> selection = fn::invert_boolean_field(
      params.extract_input<Field<bool>>("Selection"));

  const NodeGeometryDeleteGeometry &storage = node_storage(params.node());
  const eAttrDomain domain = eAttrDomain(storage.domain);
  const GeometryNodeDeleteGeometryMode mode = (GeometryNodeDeleteGeometryMode)storage.mode;

  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Geometry");

  if (domain == ATTR_DOMAIN_INSTANCE) {
    bool is_error;
    separate_geometry(geometry_set, domain, mode, selection, propagation_info, is_error);
  }
  else {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      bool is_error;
      /* Invert here because we want to keep the things not in the selection. */
      separate_geometry(geometry_set, domain, mode, selection, propagation_info, is_error);
    });
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_delete_geometry_cc

void register_node_type_geo_delete_geometry()
{
  namespace file_ns = blender::nodes::node_geo_delete_geometry_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_DELETE_GEOMETRY, "Delete Geometry", NODE_CLASS_GEOMETRY);

  node_type_storage(&ntype,
                    "NodeGeometryDeleteGeometry",
                    node_free_standard_storage,
                    node_copy_standard_storage);

  ntype.initfunc = file_ns::node_init;

  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
