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

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLI_array.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"
#include "BKE_spline.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

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

/** Utility function for making an IndexMask from a boolean selection. The indices vector should
 * live at least as long as the returned IndexMask.
 */
static IndexMask index_mask_indices(Span<bool> mask, const bool invert, Vector<int64_t> &indices)
{
  for (const int i : mask.index_range()) {
    if (mask[i] != invert) {
      indices.append(i);
    }
  }
  return IndexMask(indices);
}

/**
 * Copies the attributes with a domain in `domains` to `result_component`.
 */
static void copy_attributes(const Map<AttributeIDRef, AttributeKind> &attributes,
                            const GeometryComponent &in_component,
                            GeometryComponent &result_component,
                            const Span<AttributeDomain> domains)
{
  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    ReadAttributeLookup attribute = in_component.attribute_try_get_for_read(attribute_id);
    if (!attribute) {
      continue;
    }

    /* Only copy if it is on a domain we want. */
    if (!domains.contains(attribute.domain)) {
      continue;
    }
    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute.varray.type());

    OutputAttribute result_attribute = result_component.attribute_try_get_for_output_only(
        attribute_id, attribute.domain, data_type);

    if (!result_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArray_Span<T> span{attribute.varray.typed<T>()};
      MutableSpan<T> out_span = result_attribute.as_span<T>();
      out_span.copy_from(span);
    });
    result_attribute.save();
  }
}

/**
 * For each attribute with a domain in `domains` it copies the parts of that attribute which lie in
 * the mask to `result_component`.
 */
static void copy_attributes_based_on_mask(const Map<AttributeIDRef, AttributeKind> &attributes,
                                          const GeometryComponent &in_component,
                                          GeometryComponent &result_component,
                                          const AttributeDomain domain,
                                          const IndexMask mask)
{
  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    ReadAttributeLookup attribute = in_component.attribute_try_get_for_read(attribute_id);
    if (!attribute) {
      continue;
    }

    /* Only copy if it is on a domain we want. */
    if (domain != attribute.domain) {
      continue;
    }
    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute.varray.type());

    OutputAttribute result_attribute = result_component.attribute_try_get_for_output_only(
        attribute_id, attribute.domain, data_type);

    if (!result_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArray_Span<T> span{attribute.varray.typed<T>()};
      MutableSpan<T> out_span = result_attribute.as_span<T>();
      copy_data_based_on_mask(span, out_span, mask);
    });
    result_attribute.save();
  }
}

static void copy_attributes_based_on_map(const Map<AttributeIDRef, AttributeKind> &attributes,
                                         const GeometryComponent &in_component,
                                         GeometryComponent &result_component,
                                         const AttributeDomain domain,
                                         const Span<int> index_map)
{
  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    ReadAttributeLookup attribute = in_component.attribute_try_get_for_read(attribute_id);
    if (!attribute) {
      continue;
    }

    /* Only copy if it is on a domain we want. */
    if (domain != attribute.domain) {
      continue;
    }
    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(attribute.varray.type());

    OutputAttribute result_attribute = result_component.attribute_try_get_for_output_only(
        attribute_id, attribute.domain, data_type);

    if (!result_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArray_Span<T> span{attribute.varray.typed<T>()};
      MutableSpan<T> out_span = result_attribute.as_span<T>();
      copy_data_based_on_map(span, out_span, index_map);
    });
    result_attribute.save();
  }
}

static void copy_face_corner_attributes(const Map<AttributeIDRef, AttributeKind> &attributes,
                                        const GeometryComponent &in_component,
                                        GeometryComponent &out_component,
                                        const int num_selected_loops,
                                        const Span<int> selected_poly_indices,
                                        const Mesh &mesh_in)
{
  Vector<int64_t> indices;
  indices.reserve(num_selected_loops);
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

static void spline_copy_builtin_attributes(const Spline &spline,
                                           Spline &r_spline,
                                           const IndexMask mask)
{
  copy_data_based_on_mask(spline.positions(), r_spline.positions(), mask);
  copy_data_based_on_mask(spline.radii(), r_spline.radii(), mask);
  copy_data_based_on_mask(spline.tilts(), r_spline.tilts(), mask);
  switch (spline.type()) {
    case Spline::Type::Poly:
      break;
    case Spline::Type::Bezier: {
      const BezierSpline &src = static_cast<const BezierSpline &>(spline);
      BezierSpline &dst = static_cast<BezierSpline &>(r_spline);
      copy_data_based_on_mask(src.handle_positions_left(), dst.handle_positions_left(), mask);
      copy_data_based_on_mask(src.handle_positions_right(), dst.handle_positions_right(), mask);
      copy_data_based_on_mask(src.handle_types_left(), dst.handle_types_left(), mask);
      copy_data_based_on_mask(src.handle_types_right(), dst.handle_types_right(), mask);
      break;
    }
    case Spline::Type::NURBS: {
      const NURBSpline &src = static_cast<const NURBSpline &>(spline);
      NURBSpline &dst = static_cast<NURBSpline &>(r_spline);
      copy_data_based_on_mask(src.weights(), dst.weights(), mask);
      break;
    }
  }
}

static void copy_dynamic_attributes(const CustomDataAttributes &src,
                                    CustomDataAttributes &dst,
                                    const IndexMask mask)
{
  src.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        std::optional<GSpan> src_attribute = src.get_for_read(attribute_id);
        BLI_assert(src_attribute);

        if (!dst.create(attribute_id, meta_data.data_type)) {
          /* Since the source spline of the same type had the attribute, adding it should work.
           */
          BLI_assert_unreachable();
        }

        std::optional<GMutableSpan> new_attribute = dst.get_for_write(attribute_id);
        BLI_assert(new_attribute);

        attribute_math::convert_to_static_type(new_attribute->type(), [&](auto dummy) {
          using T = decltype(dummy);
          copy_data_based_on_mask(src_attribute->typed<T>(), new_attribute->typed<T>(), mask);
        });
        return true;
      },
      ATTR_DOMAIN_POINT);
}

/**
 * Deletes points in the spline. Those not in the mask are deleted. The spline is not split into
 * multiple newer splines.
 */
static SplinePtr spline_delete(const Spline &spline, const IndexMask mask)
{
  SplinePtr new_spline = spline.copy_only_settings();
  new_spline->resize(mask.size());

  spline_copy_builtin_attributes(spline, *new_spline, mask);
  copy_dynamic_attributes(spline.attributes, new_spline->attributes, mask);

  return new_spline;
}

static std::unique_ptr<CurveEval> curve_separate(const CurveEval &input_curve,
                                                 const Span<bool> selection,
                                                 const AttributeDomain selection_domain,
                                                 const bool invert)
{
  Span<SplinePtr> input_splines = input_curve.splines();
  std::unique_ptr<CurveEval> output_curve = std::make_unique<CurveEval>();

  /* Keep track of which splines were copied to the result to copy spline domain attributes. */
  Vector<int64_t> copied_splines;

  if (selection_domain == ATTR_DOMAIN_CURVE) {
    /* Operates on each of the splines as a whole, i.e. not on the points in the splines
     * themselves. */
    for (const int i : selection.index_range()) {
      if (selection[i] != invert) {
        output_curve->add_spline(input_splines[i]->copy());
        copied_splines.append(i);
      }
    }
  }
  else {
    /* Operates on the points in the splines themselves. */

    /* Reuse index vector for each spline. */
    Vector<int64_t> indices_to_copy;

    int selection_index = 0;
    for (const int i : input_splines.index_range()) {
      const Spline &spline = *input_splines[i];

      indices_to_copy.clear();
      for (const int i_point : IndexRange(spline.size())) {
        if (selection[selection_index] != invert) {
          /* Append i_point instead of selection_index because we need indices local to the spline
           * for copying. */
          indices_to_copy.append(i_point);
        }
        selection_index++;
      }

      /* Avoid creating an empty spline. */
      if (indices_to_copy.is_empty()) {
        continue;
      }

      SplinePtr new_spline = spline_delete(spline, IndexMask(indices_to_copy));
      output_curve->add_spline(std::move(new_spline));
      copied_splines.append(i);
    }
  }

  if (copied_splines.is_empty()) {
    return {};
  }

  output_curve->attributes.reallocate(output_curve->splines().size());
  copy_dynamic_attributes(
      input_curve.attributes, output_curve->attributes, IndexMask(copied_splines));

  return output_curve;
}

static void separate_curve_selection(GeometrySet &geometry_set,
                                     const Field<bool> &selection_field,
                                     const AttributeDomain selection_domain,
                                     const bool invert)
{
  const CurveComponent &src_component = *geometry_set.get_component_for_read<CurveComponent>();
  GeometryComponentFieldContext field_context{src_component, selection_domain};

  fn::FieldEvaluator selection_evaluator{field_context,
                                         src_component.attribute_domain_size(selection_domain)};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const VArray_Span<bool> &selection = selection_evaluator.get_evaluated<bool>(0);
  std::unique_ptr<CurveEval> r_curve = curve_separate(
      *src_component.get_for_read(), selection, selection_domain, invert);
  if (r_curve) {
    geometry_set.replace_curve(r_curve.release());
  }
  else {
    geometry_set.replace_curve(nullptr);
  }
}

static void separate_point_cloud_selection(GeometrySet &geometry_set,
                                           const Field<bool> &selection_field,
                                           const bool invert)
{
  const PointCloudComponent &src_points =
      *geometry_set.get_component_for_read<PointCloudComponent>();
  GeometryComponentFieldContext field_context{src_points, ATTR_DOMAIN_POINT};

  fn::FieldEvaluator selection_evaluator{field_context,
                                         src_points.attribute_domain_size(ATTR_DOMAIN_POINT)};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const VArray_Span<bool> &selection = selection_evaluator.get_evaluated<bool>(0);

  Vector<int64_t> indices;
  const IndexMask mask = index_mask_indices(selection, invert, indices);
  const int total = mask.size();
  PointCloud *pointcloud = BKE_pointcloud_new_nomain(total);

  if (total == 0) {
    geometry_set.replace_pointcloud(pointcloud);
    return;
  }

  PointCloudComponent dst_points;
  dst_points.replace(pointcloud, GeometryOwnershipType::Editable);

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_POINT_CLOUD}, GEO_COMPONENT_TYPE_POINT_CLOUD, false, attributes);

  copy_attributes_based_on_mask(attributes, src_points, dst_points, ATTR_DOMAIN_POINT, mask);
  geometry_set.replace_pointcloud(pointcloud);
}

static void separate_instance_selection(GeometrySet &geometry_set,
                                        const Field<bool> &selection_field,
                                        const bool invert)
{
  InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();
  GeometryComponentFieldContext field_context{instances, ATTR_DOMAIN_INSTANCE};

  const int domain_size = instances.attribute_domain_size(ATTR_DOMAIN_INSTANCE);
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.add(selection_field);
  evaluator.evaluate();
  const VArray_Span<bool> &selection = evaluator.get_evaluated<bool>(0);

  Vector<int64_t> indices;
  const IndexMask mask = index_mask_indices(selection, invert, indices);

  if (mask.size() == 0) {
    geometry_set.remove<InstancesComponent>();
    return;
  }

  instances.remove_instances(mask);
}

static void compute_selected_vertices_from_vertex_selection(const Span<bool> vertex_selection,
                                                            const bool invert,
                                                            MutableSpan<int> r_vertex_map,
                                                            int *r_num_selected_vertices)
{
  BLI_assert(vertex_selection.size() == r_vertex_map.size());

  int num_selected_vertices = 0;
  for (const int i : r_vertex_map.index_range()) {
    if (vertex_selection[i] != invert) {
      r_vertex_map[i] = num_selected_vertices;
      num_selected_vertices++;
    }
    else {
      r_vertex_map[i] = -1;
    }
  }

  *r_num_selected_vertices = num_selected_vertices;
}

static void compute_selected_edges_from_vertex_selection(const Mesh &mesh,
                                                         const Span<bool> vertex_selection,
                                                         const bool invert,
                                                         MutableSpan<int> r_edge_map,
                                                         int *r_num_selected_edges)
{
  BLI_assert(mesh.totedge == r_edge_map.size());

  int num_selected_edges = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    const MEdge &edge = mesh.medge[i];

    /* Only add the edge if both vertices will be in the new mesh. */
    if (vertex_selection[edge.v1] != invert && vertex_selection[edge.v2] != invert) {
      r_edge_map[i] = num_selected_edges;
      num_selected_edges++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_num_selected_edges = num_selected_edges;
}

static void compute_selected_polygons_from_vertex_selection(const Mesh &mesh,
                                                            const Span<bool> vertex_selection,
                                                            const bool invert,
                                                            Vector<int> &r_selected_poly_indices,
                                                            Vector<int> &r_loop_starts,
                                                            int *r_num_selected_polys,
                                                            int *r_num_selected_loops)
{
  BLI_assert(mesh.totvert == vertex_selection.size());

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int num_selected_loops = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];

    bool all_verts_in_selection = true;
    Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
    for (const MLoop &loop : loops_src) {
      if (vertex_selection[loop.v] == invert) {
        all_verts_in_selection = false;
        break;
      }
    }

    if (all_verts_in_selection) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(num_selected_loops);
      num_selected_loops += poly_src.totloop;
    }
  }

  *r_num_selected_polys = r_selected_poly_indices.size();
  *r_num_selected_loops = num_selected_loops;
}

/**
 * Checks for every edge if it is in `edge_selection`. If it is, then the two vertices of the
 * edge are kept along with the edge.
 */
static void compute_selected_vertices_and_edges_from_edge_selection(
    const Mesh &mesh,
    const Span<bool> edge_selection,
    const bool invert,
    MutableSpan<int> r_vertex_map,
    MutableSpan<int> r_edge_map,
    int *r_num_selected_vertices,
    int *r_num_selected_edges)
{
  BLI_assert(mesh.totedge == edge_selection.size());

  int num_selected_edges = 0;
  int num_selected_vertices = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    const MEdge &edge = mesh.medge[i];
    if (edge_selection[i] != invert) {
      r_edge_map[i] = num_selected_edges;
      num_selected_edges++;
      if (r_vertex_map[edge.v1] == -1) {
        r_vertex_map[edge.v1] = num_selected_vertices;
        num_selected_vertices++;
      }
      if (r_vertex_map[edge.v2] == -1) {
        r_vertex_map[edge.v2] = num_selected_vertices;
        num_selected_vertices++;
      }
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_num_selected_vertices = num_selected_vertices;
  *r_num_selected_edges = num_selected_edges;
}

/**
 * Checks for every edge if it is in `edge_selection`.
 */
static void compute_selected_edges_from_edge_selection(const Mesh &mesh,
                                                       const Span<bool> edge_selection,
                                                       const bool invert,
                                                       MutableSpan<int> r_edge_map,
                                                       int *r_num_selected_edges)
{
  BLI_assert(mesh.totedge == edge_selection.size());

  int num_selected_edges = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    if (edge_selection[i] != invert) {
      r_edge_map[i] = num_selected_edges;
      num_selected_edges++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_num_selected_edges = num_selected_edges;
}

/**
 * Checks for every polygon if all the edges are in `edge_selection`. If they are, then that
 * polygon is kept.
 */
static void compute_selected_polygons_from_edge_selection(const Mesh &mesh,
                                                          const Span<bool> edge_selection,
                                                          const bool invert,
                                                          Vector<int> &r_selected_poly_indices,
                                                          Vector<int> &r_loop_starts,
                                                          int *r_num_selected_polys,
                                                          int *r_num_selected_loops)
{
  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int num_selected_loops = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];

    bool all_edges_in_selection = true;
    Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
    for (const MLoop &loop : loops_src) {
      if (edge_selection[loop.e] == invert) {
        all_edges_in_selection = false;
        break;
      }
    }

    if (all_edges_in_selection) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(num_selected_loops);
      num_selected_loops += poly_src.totloop;
    }
  }

  *r_num_selected_polys = r_selected_poly_indices.size();
  *r_num_selected_loops = num_selected_loops;
}

/**
 * Checks for every edge and polygon if all its vertices are in `vertex_selection`.
 */
static void compute_selected_mesh_data_from_vertex_selection_edge_face(
    const Mesh &mesh,
    const Span<bool> vertex_selection,
    const bool invert,
    MutableSpan<int> r_edge_map,
    Vector<int> &r_selected_poly_indices,
    Vector<int> &r_loop_starts,
    int *r_num_selected_edges,
    int *r_num_selected_polys,
    int *r_num_selected_loops)
{

  compute_selected_edges_from_vertex_selection(
      mesh, vertex_selection, invert, r_edge_map, r_num_selected_edges);

  compute_selected_polygons_from_vertex_selection(mesh,
                                                  vertex_selection,
                                                  invert,
                                                  r_selected_poly_indices,
                                                  r_loop_starts,
                                                  r_num_selected_polys,
                                                  r_num_selected_loops);
}

/**
 * Checks for every vertex if it is in `vertex_selection`. The polygons and edges are kept if all
 * vertices of that polygon or edge are in the selection.
 */
static void compute_selected_mesh_data_from_vertex_selection(const Mesh &mesh,
                                                             const Span<bool> vertex_selection,
                                                             const bool invert,
                                                             MutableSpan<int> r_vertex_map,
                                                             MutableSpan<int> r_edge_map,
                                                             Vector<int> &r_selected_poly_indices,
                                                             Vector<int> &r_loop_starts,
                                                             int *r_num_selected_vertices,
                                                             int *r_num_selected_edges,
                                                             int *r_num_selected_polys,
                                                             int *r_num_selected_loops)
{
  compute_selected_vertices_from_vertex_selection(
      vertex_selection, invert, r_vertex_map, r_num_selected_vertices);

  compute_selected_edges_from_vertex_selection(
      mesh, vertex_selection, invert, r_edge_map, r_num_selected_edges);

  compute_selected_polygons_from_vertex_selection(mesh,
                                                  vertex_selection,
                                                  invert,
                                                  r_selected_poly_indices,
                                                  r_loop_starts,
                                                  r_num_selected_polys,
                                                  r_num_selected_loops);
}

/**
 * Checks for every edge if it is in `edge_selection`. The polygons are kept if all edges are in
 * the selection.
 */
static void compute_selected_mesh_data_from_edge_selection_edge_face(
    const Mesh &mesh,
    const Span<bool> edge_selection,
    const bool invert,
    MutableSpan<int> r_edge_map,
    Vector<int> &r_selected_poly_indices,
    Vector<int> &r_loop_starts,
    int *r_num_selected_edges,
    int *r_num_selected_polys,
    int *r_num_selected_loops)
{
  compute_selected_edges_from_edge_selection(
      mesh, edge_selection, invert, r_edge_map, r_num_selected_edges);
  compute_selected_polygons_from_edge_selection(mesh,
                                                edge_selection,
                                                invert,
                                                r_selected_poly_indices,
                                                r_loop_starts,
                                                r_num_selected_polys,
                                                r_num_selected_loops);
}

/**
 * Checks for every edge if it is in `edge_selection`. If it is, the vertices belonging to
 * that edge are kept as well. The polygons are kept if all edges are in the selection.
 */
static void compute_selected_mesh_data_from_edge_selection(const Mesh &mesh,
                                                           const Span<bool> edge_selection,
                                                           const bool invert,
                                                           MutableSpan<int> r_vertex_map,
                                                           MutableSpan<int> r_edge_map,
                                                           Vector<int> &r_selected_poly_indices,
                                                           Vector<int> &r_loop_starts,
                                                           int *r_num_selected_vertices,
                                                           int *r_num_selected_edges,
                                                           int *r_num_selected_polys,
                                                           int *r_num_selected_loops)
{
  r_vertex_map.fill(-1);
  compute_selected_vertices_and_edges_from_edge_selection(mesh,
                                                          edge_selection,
                                                          invert,
                                                          r_vertex_map,
                                                          r_edge_map,
                                                          r_num_selected_vertices,
                                                          r_num_selected_edges);
  compute_selected_polygons_from_edge_selection(mesh,
                                                edge_selection,
                                                invert,
                                                r_selected_poly_indices,
                                                r_loop_starts,
                                                r_num_selected_polys,
                                                r_num_selected_loops);
}

/**
 * Checks for every polygon if it is in `poly_selection`.
 */
static void compute_selected_polygons_from_poly_selection(const Mesh &mesh,
                                                          const Span<bool> poly_selection,
                                                          const bool invert,
                                                          Vector<int> &r_selected_poly_indices,
                                                          Vector<int> &r_loop_starts,
                                                          int *r_num_selected_polys,
                                                          int *r_num_selected_loops)
{
  BLI_assert(mesh.totpoly == poly_selection.size());

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int num_selected_loops = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];
    /* We keep this one. */
    if (poly_selection[i] != invert) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(num_selected_loops);
      num_selected_loops += poly_src.totloop;
    }
  }
  *r_num_selected_polys = r_selected_poly_indices.size();
  *r_num_selected_loops = num_selected_loops;
}
/**
 * Checks for every polygon if it is in `poly_selection`. If it is, the edges
 * belonging to that polygon are kept as well.
 */
static void compute_selected_mesh_data_from_poly_selection_edge_face(
    const Mesh &mesh,
    const Span<bool> poly_selection,
    const bool invert,
    MutableSpan<int> r_edge_map,
    Vector<int> &r_selected_poly_indices,
    Vector<int> &r_loop_starts,
    int *r_num_selected_edges,
    int *r_num_selected_polys,
    int *r_num_selected_loops)
{
  BLI_assert(mesh.totpoly == poly_selection.size());
  BLI_assert(mesh.totedge == r_edge_map.size());
  r_edge_map.fill(-1);

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int num_selected_loops = 0;
  int num_selected_edges = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];
    /* We keep this one. */
    if (poly_selection[i] != invert) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(num_selected_loops);
      num_selected_loops += poly_src.totloop;

      /* Add the vertices and the edges. */
      Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
      for (const MLoop &loop : loops_src) {
        /* Check first if it has not yet been added. */
        if (r_edge_map[loop.e] == -1) {
          r_edge_map[loop.e] = num_selected_edges;
          num_selected_edges++;
        }
      }
    }
  }
  *r_num_selected_edges = num_selected_edges;
  *r_num_selected_polys = r_selected_poly_indices.size();
  *r_num_selected_loops = num_selected_loops;
}

/**
 * Checks for every polygon if it is in `poly_selection`. If it is, the edges and vertices
 * belonging to that polygon are kept as well.
 */
static void compute_selected_mesh_data_from_poly_selection(const Mesh &mesh,
                                                           const Span<bool> poly_selection,
                                                           const bool invert,
                                                           MutableSpan<int> r_vertex_map,
                                                           MutableSpan<int> r_edge_map,
                                                           Vector<int> &r_selected_poly_indices,
                                                           Vector<int> &r_loop_starts,
                                                           int *r_num_selected_vertices,
                                                           int *r_num_selected_edges,
                                                           int *r_num_selected_polys,
                                                           int *r_num_selected_loops)
{
  BLI_assert(mesh.totpoly == poly_selection.size());
  BLI_assert(mesh.totedge == r_edge_map.size());
  r_vertex_map.fill(-1);
  r_edge_map.fill(-1);

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  int num_selected_loops = 0;
  int num_selected_vertices = 0;
  int num_selected_edges = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];
    /* We keep this one. */
    if (poly_selection[i] != invert) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(num_selected_loops);
      num_selected_loops += poly_src.totloop;

      /* Add the vertices and the edges. */
      Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
      for (const MLoop &loop : loops_src) {
        /* Check first if it has not yet been added. */
        if (r_vertex_map[loop.v] == -1) {
          r_vertex_map[loop.v] = num_selected_vertices;
          num_selected_vertices++;
        }
        if (r_edge_map[loop.e] == -1) {
          r_edge_map[loop.e] = num_selected_edges;
          num_selected_edges++;
        }
      }
    }
  }
  *r_num_selected_vertices = num_selected_vertices;
  *r_num_selected_edges = num_selected_edges;
  *r_num_selected_polys = r_selected_poly_indices.size();
  *r_num_selected_loops = num_selected_loops;
}

/**
 * Keep the parts of the mesh that are in the selection.
 */
static void do_mesh_separation(GeometrySet &geometry_set,
                               const MeshComponent &in_component,
                               const VArray_Span<bool> &selection,
                               const bool invert,
                               const AttributeDomain domain,
                               const GeometryNodeDeleteGeometryMode mode)
{
  /* Needed in all cases. */
  Vector<int> selected_poly_indices;
  Vector<int> new_loop_starts;
  int num_selected_polys = 0;
  int num_selected_loops = 0;

  const Mesh &mesh_in = *in_component.get_for_read();
  Mesh *mesh_out;
  MeshComponent out_component;

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_MESH}, GEO_COMPONENT_TYPE_MESH, false, attributes);

  switch (mode) {
    case GEO_NODE_DELETE_GEOMETRY_MODE_ALL: {
      Array<int> vertex_map(mesh_in.totvert);
      int num_selected_vertices = 0;

      Array<int> edge_map(mesh_in.totedge);
      int num_selected_edges = 0;

      /* Fill all the maps based on the selection. */
      switch (domain) {
        case ATTR_DOMAIN_POINT:
          compute_selected_mesh_data_from_vertex_selection(mesh_in,
                                                           selection,
                                                           invert,
                                                           vertex_map,
                                                           edge_map,
                                                           selected_poly_indices,
                                                           new_loop_starts,
                                                           &num_selected_vertices,
                                                           &num_selected_edges,
                                                           &num_selected_polys,
                                                           &num_selected_loops);
          break;
        case ATTR_DOMAIN_EDGE:
          compute_selected_mesh_data_from_edge_selection(mesh_in,
                                                         selection,
                                                         invert,
                                                         vertex_map,
                                                         edge_map,
                                                         selected_poly_indices,
                                                         new_loop_starts,
                                                         &num_selected_vertices,
                                                         &num_selected_edges,
                                                         &num_selected_polys,
                                                         &num_selected_loops);
          break;
        case ATTR_DOMAIN_FACE:
          compute_selected_mesh_data_from_poly_selection(mesh_in,
                                                         selection,
                                                         invert,
                                                         vertex_map,
                                                         edge_map,
                                                         selected_poly_indices,
                                                         new_loop_starts,
                                                         &num_selected_vertices,
                                                         &num_selected_edges,
                                                         &num_selected_polys,
                                                         &num_selected_loops);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
      mesh_out = BKE_mesh_new_nomain_from_template(&mesh_in,
                                                   num_selected_vertices,
                                                   num_selected_edges,
                                                   0,
                                                   num_selected_loops,
                                                   num_selected_polys);
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
                                  num_selected_loops,
                                  selected_poly_indices,
                                  mesh_in);
      break;
    }
    case GEO_NODE_DELETE_GEOMETRY_MODE_EDGE_FACE: {
      Array<int> edge_map(mesh_in.totedge);
      int num_selected_edges = 0;

      /* Fill all the maps based on the selection. */
      switch (domain) {
        case ATTR_DOMAIN_POINT:
          compute_selected_mesh_data_from_vertex_selection_edge_face(mesh_in,
                                                                     selection,
                                                                     invert,
                                                                     edge_map,
                                                                     selected_poly_indices,
                                                                     new_loop_starts,
                                                                     &num_selected_edges,
                                                                     &num_selected_polys,
                                                                     &num_selected_loops);
          break;
        case ATTR_DOMAIN_EDGE:
          compute_selected_mesh_data_from_edge_selection_edge_face(mesh_in,
                                                                   selection,
                                                                   invert,
                                                                   edge_map,
                                                                   selected_poly_indices,
                                                                   new_loop_starts,
                                                                   &num_selected_edges,
                                                                   &num_selected_polys,
                                                                   &num_selected_loops);
          break;
        case ATTR_DOMAIN_FACE:
          compute_selected_mesh_data_from_poly_selection_edge_face(mesh_in,
                                                                   selection,
                                                                   invert,
                                                                   edge_map,
                                                                   selected_poly_indices,
                                                                   new_loop_starts,
                                                                   &num_selected_edges,
                                                                   &num_selected_polys,
                                                                   &num_selected_loops);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
      mesh_out = BKE_mesh_new_nomain_from_template(&mesh_in,
                                                   mesh_in.totvert,
                                                   num_selected_edges,
                                                   0,
                                                   num_selected_loops,
                                                   num_selected_polys);
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
                                  num_selected_loops,
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
                                                          invert,
                                                          selected_poly_indices,
                                                          new_loop_starts,
                                                          &num_selected_polys,
                                                          &num_selected_loops);
          break;
        case ATTR_DOMAIN_EDGE:
          compute_selected_polygons_from_edge_selection(mesh_in,
                                                        selection,
                                                        invert,
                                                        selected_poly_indices,
                                                        new_loop_starts,
                                                        &num_selected_polys,
                                                        &num_selected_loops);
          break;
        case ATTR_DOMAIN_FACE:
          compute_selected_polygons_from_poly_selection(mesh_in,
                                                        selection,
                                                        invert,
                                                        selected_poly_indices,
                                                        new_loop_starts,
                                                        &num_selected_polys,
                                                        &num_selected_loops);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
      mesh_out = BKE_mesh_new_nomain_from_template(
          &mesh_in, mesh_in.totvert, mesh_in.totedge, 0, num_selected_loops, num_selected_polys);
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
                                  num_selected_loops,
                                  selected_poly_indices,
                                  mesh_in);
      break;
    }
  }

  BKE_mesh_calc_edges_loose(mesh_out);
  /* Tag to recalculate normals later. */
  BKE_mesh_normals_tag_dirty(mesh_out);
  geometry_set.replace_mesh(mesh_out);
}

static void separate_mesh_selection(GeometrySet &geometry_set,
                                    const Field<bool> &selection_field,
                                    const AttributeDomain selection_domain,
                                    const GeometryNodeDeleteGeometryMode mode,
                                    const bool invert)
{
  const MeshComponent &src_component = *geometry_set.get_component_for_read<MeshComponent>();
  GeometryComponentFieldContext field_context{src_component, selection_domain};

  fn::FieldEvaluator selection_evaluator{field_context,
                                         src_component.attribute_domain_size(selection_domain)};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const VArray_Span<bool> &selection = selection_evaluator.get_evaluated<bool>(0);

  /* Check if there is anything to delete. */
  bool delete_nothing = true;
  for (const int i : selection.index_range()) {
    if (selection[i] == invert) {
      delete_nothing = false;
      break;
    }
  }
  if (delete_nothing) {
    return;
  }

  do_mesh_separation(geometry_set, src_component, selection, invert, selection_domain, mode);
}

void separate_geometry(GeometrySet &geometry_set,
                       const AttributeDomain domain,
                       const GeometryNodeDeleteGeometryMode mode,
                       const Field<bool> &selection_field,
                       const bool invert,
                       bool &r_is_error)
{
  bool some_valid_domain = false;
  if (geometry_set.has_pointcloud()) {
    if (domain == ATTR_DOMAIN_POINT) {
      separate_point_cloud_selection(geometry_set, selection_field, invert);
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_mesh()) {
    if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_FACE, ATTR_DOMAIN_CORNER)) {
      separate_mesh_selection(geometry_set, selection_field, domain, mode, invert);
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_curve()) {
    if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE)) {
      separate_curve_selection(geometry_set, selection_field, domain, invert);
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_instances()) {
    if (domain == ATTR_DOMAIN_INSTANCE) {
      separate_instance_selection(geometry_set, selection_field, invert);
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
  const AttributeDomain domain = static_cast<AttributeDomain>(storage.domain);

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

  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  const NodeGeometryDeleteGeometry &storage = node_storage(params.node());
  const AttributeDomain domain = static_cast<AttributeDomain>(storage.domain);
  const GeometryNodeDeleteGeometryMode mode = (GeometryNodeDeleteGeometryMode)storage.mode;

  bool all_is_error = false;
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    bool this_is_error = false;
    /* Invert here because we want to keep the things not in the selection. */
    separate_geometry(geometry_set, domain, mode, selection_field, true, this_is_error);
    all_is_error &= this_is_error;
  });
  if (all_is_error) {
    /* Only show this if none of the instances/components actually changed. */
    params.error_message_add(NodeWarningType::Info, TIP_("No geometry with given domain"));
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_delete_geometry_cc

void register_node_type_geo_delete_geometry()
{
  namespace file_ns = blender::nodes::node_geo_delete_geometry_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_DELETE_GEOMETRY, "Delete Geometry", NODE_CLASS_GEOMETRY, 0);

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
