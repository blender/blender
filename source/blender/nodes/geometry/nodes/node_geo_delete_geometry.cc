/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLI_array.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_delete_geometry_cc {

using blender::bke::CustomDataAttributes;

template<typename T>
static void copy_data_based_on_mask(Span<T> data, MutableSpan<T> r_data, IndexMask mask)
{
  for (const int i_out : mask.index_range()) {
    r_data[i_out] = data[mask[i_out]];
  }
}

template<typename T>
static void copy_data_based_on_map(Span<T> src, MutableSpan<T> dst, Span<int> index_map)
{
  for (const int i_src : index_map.index_range()) {
    const int i_dst = index_map[i_src];
    if (i_dst != -1) {
      dst[i_dst] = src[i_src];
    }
  }
}

/**
 * Copies the attributes with a domain in `domains` to `result_component`.
 */
static void copy_attributes(const Map<AttributeIDRef, AttributeKind> &attributes,
                            const GeometryComponent &in_component,
                            GeometryComponent &result_component,
                            const Span<eAttrDomain> domains)
{
  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    GAttributeReader attribute = in_component.attributes()->lookup(attribute_id);
    if (!attribute) {
      continue;
    }

    /* Only copy if it is on a domain we want. */
    if (!domains.contains(attribute.domain)) {
      continue;
    }
    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute.varray.type());

    GSpanAttributeWriter result_attribute =
        result_component.attributes_for_write()->lookup_or_add_for_write_only_span(
            attribute_id, attribute.domain, data_type);

    if (!result_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArraySpan<T> span{attribute.varray.typed<T>()};
      MutableSpan<T> out_span = result_attribute.span.typed<T>();
      out_span.copy_from(span);
    });
    result_attribute.finish();
  }
}

/**
 * For each attribute with a domain in `domains` it copies the parts of that attribute which lie in
 * the mask to `result_component`.
 */
static void copy_attributes_based_on_mask(const Map<AttributeIDRef, AttributeKind> &attributes,
                                          const GeometryComponent &in_component,
                                          GeometryComponent &result_component,
                                          const eAttrDomain domain,
                                          const IndexMask mask)
{
  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    GAttributeReader attribute = in_component.attributes()->lookup(attribute_id);
    if (!attribute) {
      continue;
    }

    /* Only copy if it is on a domain we want. */
    if (domain != attribute.domain) {
      continue;
    }
    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute.varray.type());

    GSpanAttributeWriter result_attribute =
        result_component.attributes_for_write()->lookup_or_add_for_write_only_span(
            attribute_id, attribute.domain, data_type);

    if (!result_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArraySpan<T> span{attribute.varray.typed<T>()};
      MutableSpan<T> out_span = result_attribute.span.typed<T>();
      copy_data_based_on_mask(span, out_span, mask);
    });
    result_attribute.finish();
  }
}

static void copy_attributes_based_on_map(const Map<AttributeIDRef, AttributeKind> &attributes,
                                         const GeometryComponent &in_component,
                                         GeometryComponent &result_component,
                                         const eAttrDomain domain,
                                         const Span<int> index_map)
{
  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    GAttributeReader attribute = in_component.attributes()->lookup(attribute_id);
    if (!attribute) {
      continue;
    }

    /* Only copy if it is on a domain we want. */
    if (domain != attribute.domain) {
      continue;
    }
    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute.varray.type());

    GSpanAttributeWriter result_attribute =
        result_component.attributes_for_write()->lookup_or_add_for_write_only_span(
            attribute_id, attribute.domain, data_type);

    if (!result_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArraySpan<T> span{attribute.varray.typed<T>()};
      MutableSpan<T> out_span = result_attribute.span.typed<T>();
      copy_data_based_on_map(span, out_span, index_map);
    });
    result_attribute.finish();
  }
}

static void copy_face_corner_attributes(const Map<AttributeIDRef, AttributeKind> &attributes,
                                        const GeometryComponent &in_component,
                                        GeometryComponent &out_component,
                                        const int selected_loops_num,
                                        const Span<int> selected_poly_indices,
                                        const Mesh &mesh_in)
{
  Vector<int64_t> indices;
  indices.reserve(selected_loops_num);
  for (const int src_poly_index : selected_poly_indices) {
    const MPoly &src_poly = mesh_in.mpoly[src_poly_index];
    const int src_loop_start = src_poly.loopstart;
    const int tot_loop = src_poly.totloop;
    for (const int i : IndexRange(tot_loop)) {
      indices.append_unchecked(src_loop_start + i);
    }
  }
  copy_attributes_based_on_mask(
      attributes, in_component, out_component, ATTR_DOMAIN_CORNER, IndexMask(indices));
}

static void copy_masked_vertices_to_new_mesh(const Mesh &src_mesh,
                                             Mesh &dst_mesh,
                                             Span<int> vertex_map)
{
  BLI_assert(src_mesh.totvert == vertex_map.size());
  for (const int i_src : vertex_map.index_range()) {
    const int i_dst = vertex_map[i_src];
    if (i_dst == -1) {
      continue;
    }

    const MVert &v_src = src_mesh.mvert[i_src];
    MVert &v_dst = dst_mesh.mvert[i_dst];

    v_dst = v_src;
  }
}

static void copy_masked_edges_to_new_mesh(const Mesh &src_mesh, Mesh &dst_mesh, Span<int> edge_map)
{
  BLI_assert(src_mesh.totedge == edge_map.size());
  for (const int i_src : IndexRange(src_mesh.totedge)) {
    const int i_dst = edge_map[i_src];
    if (ELEM(i_dst, -1, -2)) {
      continue;
    }

    const MEdge &e_src = src_mesh.medge[i_src];
    MEdge &e_dst = dst_mesh.medge[i_dst];

    e_dst = e_src;
    e_dst.v1 = e_src.v1;
    e_dst.v2 = e_src.v2;
  }
}

static void copy_masked_edges_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map,
                                          Span<int> edge_map)
{
  BLI_assert(src_mesh.totvert == vertex_map.size());
  BLI_assert(src_mesh.totedge == edge_map.size());
  for (const int i_src : IndexRange(src_mesh.totedge)) {
    const int i_dst = edge_map[i_src];
    if (i_dst == -1) {
      continue;
    }

    const MEdge &e_src = src_mesh.medge[i_src];
    MEdge &e_dst = dst_mesh.medge[i_dst];

    e_dst = e_src;
    e_dst.v1 = vertex_map[e_src.v1];
    e_dst.v2 = vertex_map[e_src.v2];
  }
}

/* Faces and edges changed but vertices are the same. */
static void copy_masked_polys_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> edge_map,
                                          Span<int> masked_poly_indices,
                                          Span<int> new_loop_starts)
{
  for (const int i_dst : masked_poly_indices.index_range()) {
    const int i_src = masked_poly_indices[i_dst];

    const MPoly &mp_src = src_mesh.mpoly[i_src];
    MPoly &mp_dst = dst_mesh.mpoly[i_dst];
    const int i_ml_src = mp_src.loopstart;
    const int i_ml_dst = new_loop_starts[i_dst];

    const MLoop *ml_src = src_mesh.mloop + i_ml_src;
    MLoop *ml_dst = dst_mesh.mloop + i_ml_dst;

    mp_dst = mp_src;
    mp_dst.loopstart = i_ml_dst;
    for (int i : IndexRange(mp_src.totloop)) {
      ml_dst[i].v = ml_src[i].v;
      ml_dst[i].e = edge_map[ml_src[i].e];
    }
  }
}

/* Only faces changed. */
static void copy_masked_polys_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> masked_poly_indices,
                                          Span<int> new_loop_starts)
{
  for (const int i_dst : masked_poly_indices.index_range()) {
    const int i_src = masked_poly_indices[i_dst];

    const MPoly &mp_src = src_mesh.mpoly[i_src];
    MPoly &mp_dst = dst_mesh.mpoly[i_dst];
    const int i_ml_src = mp_src.loopstart;
    const int i_ml_dst = new_loop_starts[i_dst];

    const MLoop *ml_src = src_mesh.mloop + i_ml_src;
    MLoop *ml_dst = dst_mesh.mloop + i_ml_dst;

    mp_dst = mp_src;
    mp_dst.loopstart = i_ml_dst;
    for (int i : IndexRange(mp_src.totloop)) {
      ml_dst[i].v = ml_src[i].v;
      ml_dst[i].e = ml_src[i].e;
    }
  }
}

static void copy_masked_polys_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map,
                                          Span<int> edge_map,
                                          Span<int> masked_poly_indices,
                                          Span<int> new_loop_starts)
{
  for (const int i_dst : masked_poly_indices.index_range()) {
    const int i_src = masked_poly_indices[i_dst];

    const MPoly &mp_src = src_mesh.mpoly[i_src];
    MPoly &mp_dst = dst_mesh.mpoly[i_dst];
    const int i_ml_src = mp_src.loopstart;
    const int i_ml_dst = new_loop_starts[i_dst];

    const MLoop *ml_src = src_mesh.mloop + i_ml_src;
    MLoop *ml_dst = dst_mesh.mloop + i_ml_dst;

    mp_dst = mp_src;
    mp_dst.loopstart = i_ml_dst;
    for (int i : IndexRange(mp_src.totloop)) {
      ml_dst[i].v = vertex_map[ml_src[i].v];
      ml_dst[i].e = edge_map[ml_src[i].e];
    }
  }
}

static void delete_curves_selection(GeometrySet &geometry_set,
                                    const Field<bool> &selection_field,
                                    const eAttrDomain selection_domain)
{
  const CurveComponent &src_component = *geometry_set.get_component_for_read<CurveComponent>();
  GeometryComponentFieldContext field_context{src_component, selection_domain};

  const int domain_num = src_component.attribute_domain_size(selection_domain);
  fn::FieldEvaluator evaluator{field_context, domain_num};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    return;
  }
  if (selection.size() == domain_num) {
    geometry_set.remove<CurveComponent>();
    return;
  }

  CurveComponent &component = geometry_set.get_component_for_write<CurveComponent>();
  Curves &curves_id = *component.get_for_write();
  bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);

  if (selection_domain == ATTR_DOMAIN_POINT) {
    curves.remove_points(selection);
  }
  else if (selection_domain == ATTR_DOMAIN_CURVE) {
    curves.remove_curves(selection);
  }
}

static void separate_point_cloud_selection(GeometrySet &geometry_set,
                                           const Field<bool> &selection_field)
{
  const PointCloudComponent &src_points =
      *geometry_set.get_component_for_read<PointCloudComponent>();
  GeometryComponentFieldContext field_context{src_points, ATTR_DOMAIN_POINT};

  fn::FieldEvaluator evaluator{field_context, src_points.attribute_domain_size(ATTR_DOMAIN_POINT)};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    geometry_set.replace_pointcloud(nullptr);
    return;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(selection.size());

  PointCloudComponent dst_points;
  dst_points.replace(pointcloud, GeometryOwnershipType::Editable);

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_POINT_CLOUD}, GEO_COMPONENT_TYPE_POINT_CLOUD, false, attributes);

  copy_attributes_based_on_mask(attributes, src_points, dst_points, ATTR_DOMAIN_POINT, selection);
  geometry_set.replace_pointcloud(pointcloud);
}

static void delete_selected_instances(GeometrySet &geometry_set,
                                      const Field<bool> &selection_field)
{
  InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();
  GeometryComponentFieldContext field_context{instances, ATTR_DOMAIN_INSTANCE};

  fn::FieldEvaluator evaluator{field_context, instances.instances_num()};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    geometry_set.remove<InstancesComponent>();
    return;
  }

  instances.remove_instances(selection);
}

static void compute_selected_vertices_from_vertex_selection(const Span<bool> vertex_selection,
                                                            MutableSpan<int> r_vertex_map,
                                                            int *r_selected_vertices_num)
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

  *r_selected_vertices_num = selected_verts_num;
}

static void compute_selected_edges_from_vertex_selection(const Mesh &mesh,
                                                         const Span<bool> vertex_selection,
                                                         MutableSpan<int> r_edge_map,
                                                         int *r_selected_edges_num)
{
  BLI_assert(mesh.totedge == r_edge_map.size());

  int selected_edges_num = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    const MEdge &edge = mesh.medge[i];

    /* Only add the edge if both vertices will be in the new mesh. */
    if (vertex_selection[edge.v1] && vertex_selection[edge.v2]) {
      r_edge_map[i] = selected_edges_num;
      selected_edges_num++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_selected_edges_num = selected_edges_num;
}

static void compute_selected_polygons_from_vertex_selection(const Mesh &mesh,
                                                            const Span<bool> vertex_selection,
                                                            Vector<int> &r_selected_poly_indices,
                                                            Vector<int> &r_loop_starts,
                                                            int *r_selected_polys_num,
                                                            int *r_selected_loops_num)
{
  BLI_assert(mesh.totvert == vertex_selection.size());

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];

    bool all_verts_in_selection = true;
    Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
    for (const MLoop &loop : loops_src) {
      if (!vertex_selection[loop.v]) {
        all_verts_in_selection = false;
        break;
      }
    }

    if (all_verts_in_selection) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.totloop;
    }
  }

  *r_selected_polys_num = r_selected_poly_indices.size();
  *r_selected_loops_num = selected_loops_num;
}

/**
 * Checks for every edge if it is in `edge_selection`. If it is, then the two vertices of the
 * edge are kept along with the edge.
 */
static void compute_selected_vertices_and_edges_from_edge_selection(
    const Mesh &mesh,
    const Span<bool> edge_selection,
    MutableSpan<int> r_vertex_map,
    MutableSpan<int> r_edge_map,
    int *r_selected_vertices_num,
    int *r_selected_edges_num)
{
  BLI_assert(mesh.totedge == edge_selection.size());

  int selected_edges_num = 0;
  int selected_verts_num = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    const MEdge &edge = mesh.medge[i];
    if (edge_selection[i]) {
      r_edge_map[i] = selected_edges_num;
      selected_edges_num++;
      if (r_vertex_map[edge.v1] == -1) {
        r_vertex_map[edge.v1] = selected_verts_num;
        selected_verts_num++;
      }
      if (r_vertex_map[edge.v2] == -1) {
        r_vertex_map[edge.v2] = selected_verts_num;
        selected_verts_num++;
      }
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_selected_vertices_num = selected_verts_num;
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
static void compute_selected_polygons_from_edge_selection(const Mesh &mesh,
                                                          const Span<bool> edge_selection,
                                                          Vector<int> &r_selected_poly_indices,
                                                          Vector<int> &r_loop_starts,
                                                          int *r_selected_polys_num,
                                                          int *r_selected_loops_num)
{
  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];

    bool all_edges_in_selection = true;
    Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
    for (const MLoop &loop : loops_src) {
      if (!edge_selection[loop.e]) {
        all_edges_in_selection = false;
        break;
      }
    }

    if (all_edges_in_selection) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.totloop;
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

  compute_selected_edges_from_vertex_selection(
      mesh, vertex_selection, r_edge_map, r_selected_edges_num);

  compute_selected_polygons_from_vertex_selection(mesh,
                                                  vertex_selection,
                                                  r_selected_poly_indices,
                                                  r_loop_starts,
                                                  r_selected_polys_num,
                                                  r_selected_loops_num);
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
                                                             int *r_selected_vertices_num,
                                                             int *r_selected_edges_num,
                                                             int *r_selected_polys_num,
                                                             int *r_selected_loops_num)
{
  compute_selected_vertices_from_vertex_selection(
      vertex_selection, r_vertex_map, r_selected_vertices_num);

  compute_selected_edges_from_vertex_selection(
      mesh, vertex_selection, r_edge_map, r_selected_edges_num);

  compute_selected_polygons_from_vertex_selection(mesh,
                                                  vertex_selection,
                                                  r_selected_poly_indices,
                                                  r_loop_starts,
                                                  r_selected_polys_num,
                                                  r_selected_loops_num);
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
  compute_selected_edges_from_edge_selection(
      mesh, edge_selection, r_edge_map, r_selected_edges_num);
  compute_selected_polygons_from_edge_selection(mesh,
                                                edge_selection,
                                                r_selected_poly_indices,
                                                r_loop_starts,
                                                r_selected_polys_num,
                                                r_selected_loops_num);
}

/**
 * Checks for every edge if it is in `edge_selection`. If it is, the vertices belonging to
 * that edge are kept as well. The polygons are kept if all edges are in the selection.
 */
static void compute_selected_mesh_data_from_edge_selection(const Mesh &mesh,
                                                           const Span<bool> edge_selection,
                                                           MutableSpan<int> r_vertex_map,
                                                           MutableSpan<int> r_edge_map,
                                                           Vector<int> &r_selected_poly_indices,
                                                           Vector<int> &r_loop_starts,
                                                           int *r_selected_vertices_num,
                                                           int *r_selected_edges_num,
                                                           int *r_selected_polys_num,
                                                           int *r_selected_loops_num)
{
  r_vertex_map.fill(-1);
  compute_selected_vertices_and_edges_from_edge_selection(mesh,
                                                          edge_selection,
                                                          r_vertex_map,
                                                          r_edge_map,
                                                          r_selected_vertices_num,
                                                          r_selected_edges_num);
  compute_selected_polygons_from_edge_selection(mesh,
                                                edge_selection,
                                                r_selected_poly_indices,
                                                r_loop_starts,
                                                r_selected_polys_num,
                                                r_selected_loops_num);
}

/**
 * Checks for every polygon if it is in `poly_selection`.
 */
static void compute_selected_polygons_from_poly_selection(const Mesh &mesh,
                                                          const Span<bool> poly_selection,
                                                          Vector<int> &r_selected_poly_indices,
                                                          Vector<int> &r_loop_starts,
                                                          int *r_selected_polys_num,
                                                          int *r_selected_loops_num)
{
  BLI_assert(mesh.totpoly == poly_selection.size());

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];
    /* We keep this one. */
    if (poly_selection[i]) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.totloop;
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
  r_edge_map.fill(-1);

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  int selected_edges_num = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];
    /* We keep this one. */
    if (poly_selection[i]) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.totloop;

      /* Add the vertices and the edges. */
      Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
      for (const MLoop &loop : loops_src) {
        /* Check first if it has not yet been added. */
        if (r_edge_map[loop.e] == -1) {
          r_edge_map[loop.e] = selected_edges_num;
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
                                                           int *r_selected_vertices_num,
                                                           int *r_selected_edges_num,
                                                           int *r_selected_polys_num,
                                                           int *r_selected_loops_num)
{
  BLI_assert(mesh.totpoly == poly_selection.size());
  BLI_assert(mesh.totedge == r_edge_map.size());
  r_vertex_map.fill(-1);
  r_edge_map.fill(-1);

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int selected_loops_num = 0;
  int selected_verts_num = 0;
  int selected_edges_num = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];
    /* We keep this one. */
    if (poly_selection[i]) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(selected_loops_num);
      selected_loops_num += poly_src.totloop;

      /* Add the vertices and the edges. */
      Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
      for (const MLoop &loop : loops_src) {
        /* Check first if it has not yet been added. */
        if (r_vertex_map[loop.v] == -1) {
          r_vertex_map[loop.v] = selected_verts_num;
          selected_verts_num++;
        }
        if (r_edge_map[loop.e] == -1) {
          r_edge_map[loop.e] = selected_edges_num;
          selected_edges_num++;
        }
      }
    }
  }
  *r_selected_vertices_num = selected_verts_num;
  *r_selected_edges_num = selected_edges_num;
  *r_selected_polys_num = r_selected_poly_indices.size();
  *r_selected_loops_num = selected_loops_num;
}

/**
 * Keep the parts of the mesh that are in the selection.
 */
static void do_mesh_separation(GeometrySet &geometry_set,
                               const MeshComponent &in_component,
                               const Span<bool> selection,
                               const eAttrDomain domain,
                               const GeometryNodeDeleteGeometryMode mode)
{
  /* Needed in all cases. */
  Vector<int> selected_poly_indices;
  Vector<int> new_loop_starts;
  int selected_polys_num = 0;
  int selected_loops_num = 0;

  const Mesh &mesh_in = *in_component.get_for_read();
  Mesh *mesh_out;
  MeshComponent out_component;

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_MESH}, GEO_COMPONENT_TYPE_MESH, false, attributes);

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
                                                   0,
                                                   selected_loops_num,
                                                   selected_polys_num);
      out_component.replace(mesh_out, GeometryOwnershipType::Editable);

      /* Copy the selected parts of the mesh over to the new mesh. */
      copy_masked_vertices_to_new_mesh(mesh_in, *mesh_out, vertex_map);
      copy_masked_edges_to_new_mesh(mesh_in, *mesh_out, vertex_map, edge_map);
      copy_masked_polys_to_new_mesh(
          mesh_in, *mesh_out, vertex_map, edge_map, selected_poly_indices, new_loop_starts);

      /* Copy attributes. */
      copy_attributes_based_on_map(
          attributes, in_component, out_component, ATTR_DOMAIN_POINT, vertex_map);
      copy_attributes_based_on_map(
          attributes, in_component, out_component, ATTR_DOMAIN_EDGE, edge_map);
      copy_attributes_based_on_mask(attributes,
                                    in_component,
                                    out_component,
                                    ATTR_DOMAIN_FACE,
                                    IndexMask(Vector<int64_t>(selected_poly_indices.as_span())));
      copy_face_corner_attributes(attributes,
                                  in_component,
                                  out_component,
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
      mesh_out = BKE_mesh_new_nomain_from_template(&mesh_in,
                                                   mesh_in.totvert,
                                                   selected_edges_num,
                                                   0,
                                                   selected_loops_num,
                                                   selected_polys_num);
      out_component.replace(mesh_out, GeometryOwnershipType::Editable);

      /* Copy the selected parts of the mesh over to the new mesh. */
      memcpy(mesh_out->mvert, mesh_in.mvert, mesh_in.totvert * sizeof(MVert));
      copy_masked_edges_to_new_mesh(mesh_in, *mesh_out, edge_map);
      copy_masked_polys_to_new_mesh(
          mesh_in, *mesh_out, edge_map, selected_poly_indices, new_loop_starts);

      /* Copy attributes. */
      copy_attributes(attributes, in_component, out_component, {ATTR_DOMAIN_POINT});
      copy_attributes_based_on_map(
          attributes, in_component, out_component, ATTR_DOMAIN_EDGE, edge_map);
      copy_attributes_based_on_mask(attributes,
                                    in_component,
                                    out_component,
                                    ATTR_DOMAIN_FACE,
                                    IndexMask(Vector<int64_t>(selected_poly_indices.as_span())));
      copy_face_corner_attributes(attributes,
                                  in_component,
                                  out_component,
                                  selected_loops_num,
                                  selected_poly_indices,
                                  mesh_in);
      break;
    }
    case GEO_NODE_DELETE_GEOMETRY_MODE_ONLY_FACE: {
      /* Fill all the maps based on the selection. */
      switch (domain) {
        case ATTR_DOMAIN_POINT:
          compute_selected_polygons_from_vertex_selection(mesh_in,
                                                          selection,
                                                          selected_poly_indices,
                                                          new_loop_starts,
                                                          &selected_polys_num,
                                                          &selected_loops_num);
          break;
        case ATTR_DOMAIN_EDGE:
          compute_selected_polygons_from_edge_selection(mesh_in,
                                                        selection,
                                                        selected_poly_indices,
                                                        new_loop_starts,
                                                        &selected_polys_num,
                                                        &selected_loops_num);
          break;
        case ATTR_DOMAIN_FACE:
          compute_selected_polygons_from_poly_selection(mesh_in,
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
          &mesh_in, mesh_in.totvert, mesh_in.totedge, 0, selected_loops_num, selected_polys_num);
      out_component.replace(mesh_out, GeometryOwnershipType::Editable);

      /* Copy the selected parts of the mesh over to the new mesh. */
      memcpy(mesh_out->mvert, mesh_in.mvert, mesh_in.totvert * sizeof(MVert));
      memcpy(mesh_out->medge, mesh_in.medge, mesh_in.totedge * sizeof(MEdge));
      copy_masked_polys_to_new_mesh(mesh_in, *mesh_out, selected_poly_indices, new_loop_starts);

      /* Copy attributes. */
      copy_attributes(
          attributes, in_component, out_component, {ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE});
      copy_attributes_based_on_mask(attributes,
                                    in_component,
                                    out_component,
                                    ATTR_DOMAIN_FACE,
                                    IndexMask(Vector<int64_t>(selected_poly_indices.as_span())));
      copy_face_corner_attributes(attributes,
                                  in_component,
                                  out_component,
                                  selected_loops_num,
                                  selected_poly_indices,
                                  mesh_in);
      break;
    }
  }

  BKE_mesh_calc_edges_loose(mesh_out);
  geometry_set.replace_mesh(mesh_out);
}

static void separate_mesh_selection(GeometrySet &geometry_set,
                                    const Field<bool> &selection_field,
                                    const eAttrDomain selection_domain,
                                    const GeometryNodeDeleteGeometryMode mode)
{
  const MeshComponent &src_component = *geometry_set.get_component_for_read<MeshComponent>();
  GeometryComponentFieldContext field_context{src_component, selection_domain};

  fn::FieldEvaluator evaluator{field_context,
                               src_component.attribute_domain_size(selection_domain)};
  evaluator.add(selection_field);
  evaluator.evaluate();
  const VArray<bool> selection = evaluator.get_evaluated<bool>(0);
  /* Check if there is anything to delete. */
  if (selection.is_single() && selection.get_internal_single()) {
    return;
  }

  const VArraySpan<bool> selection_span{selection};

  do_mesh_separation(geometry_set, src_component, selection_span, selection_domain, mode);
}

}  // namespace blender::nodes::node_geo_delete_geometry_cc

namespace blender::nodes {

void separate_geometry(GeometrySet &geometry_set,
                       const eAttrDomain domain,
                       const GeometryNodeDeleteGeometryMode mode,
                       const Field<bool> &selection_field,
                       bool &r_is_error)
{
  namespace file_ns = blender::nodes::node_geo_delete_geometry_cc;

  bool some_valid_domain = false;
  if (geometry_set.has_pointcloud()) {
    if (domain == ATTR_DOMAIN_POINT) {
      file_ns::separate_point_cloud_selection(geometry_set, selection_field);
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_mesh()) {
    if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_FACE, ATTR_DOMAIN_CORNER)) {
      file_ns::separate_mesh_selection(geometry_set, selection_field, domain, mode);
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_curves()) {
    if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE)) {
      file_ns::delete_curves_selection(
          geometry_set, fn::invert_boolean_field(selection_field), domain);
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_instances()) {
    if (domain == ATTR_DOMAIN_INSTANCE) {
      file_ns::delete_selected_instances(geometry_set, selection_field);
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
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Bool>(N_("Selection"))
      .default_value(true)
      .hide_value()
      .supports_field()
      .description(N_("The parts of the geometry to be deleted"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  const bNode *node = static_cast<bNode *>(ptr->data);
  const NodeGeometryDeleteGeometry &storage = node_storage(*node);
  const eAttrDomain domain = static_cast<eAttrDomain>(storage.domain);

  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
  /* Only show the mode when it is relevant. */
  if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_FACE)) {
    uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
  }
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
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
  const eAttrDomain domain = static_cast<eAttrDomain>(storage.domain);
  const GeometryNodeDeleteGeometryMode mode = (GeometryNodeDeleteGeometryMode)storage.mode;

  if (domain == ATTR_DOMAIN_INSTANCE) {
    bool is_error;
    separate_geometry(geometry_set, domain, mode, selection, is_error);
  }
  else {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      bool is_error;
      /* Invert here because we want to keep the things not in the selection. */
      separate_geometry(geometry_set, domain, mode, selection, is_error);
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

  node_type_init(&ntype, file_ns::node_init);

  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
