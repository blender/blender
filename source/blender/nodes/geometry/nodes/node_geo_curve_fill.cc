/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_delaunay_2d.hh"
#include "BLI_math_vector_types.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"

#include "BLI_task.hh"

#include "GEO_foreach_geometry.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_fill_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveFill)

static const EnumPropertyItem mode_items[] = {
    {GEO_NODE_CURVE_FILL_MODE_TRIANGULATED, "TRIANGLES", 0, N_("Triangles"), ""},
    {GEO_NODE_CURVE_FILL_MODE_NGONS, "NGONS", 0, N_("N-gons"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve")
      .supported_type({GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil})
      .description(
          "Curves to fill. All curves are treated as cyclic and projected to the XY plane");
  b.add_input<decl::Int>("Group ID")
      .field_on_all()
      .hide_value()
      .description(
          "An index used to group curves together. Filling is done separately for each group");
  b.add_input<decl::Menu>("Mode")
      .static_items(mode_items)
      .default_value(GEO_NODE_CURVE_FILL_MODE_TRIANGULATED)
      .optional_label();
  b.add_output<decl::Geometry>("Mesh").propagate_all_instance_attributes();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  /* Still used for forward compatibility. */
  node->storage = MEM_callocN<NodeGeometryCurveFill>(__func__);
}

static void fill_curve_vert_indices(const OffsetIndices<int> offsets,
                                    MutableSpan<Vector<int>> faces)
{
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      faces[i].resize(offsets[i].size());
      array_utils::fill_index_range<int>(faces[i], offsets[i].start());
    }
  });
}

static meshintersect::CDT_result<double> do_cdt(const bke::CurvesGeometry &curves,
                                                const CDT_output_type output_type)
{
  const OffsetIndices points_by_curve = curves.evaluated_points_by_curve();
  const Span<float3> positions = curves.evaluated_positions();

  Array<double2> positions_2d(positions.size());
  threading::parallel_for(positions.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      positions_2d[i] = double2(positions[i].x, positions[i].y);
    }
  });

  Array<Vector<int>> faces(curves.curves_num());
  fill_curve_vert_indices(points_by_curve, faces);

  meshintersect::CDT_input<double> input;
  input.need_ids = false;
  input.vert = std::move(positions_2d);
  input.face = std::move(faces);

  return delaunay_2d_calc(input, output_type);
}

static meshintersect::CDT_result<double> do_cdt_with_mask(const bke::CurvesGeometry &curves,
                                                          const CDT_output_type output_type,
                                                          const IndexMask &mask)
{
  const OffsetIndices points_by_curve = curves.evaluated_points_by_curve();
  const Span<float3> positions = curves.evaluated_positions();

  Array<int> offsets_data(mask.size() + 1);
  const OffsetIndices points_by_curve_masked = offset_indices::gather_selected_offsets(
      points_by_curve, mask, offsets_data);

  Array<double2> positions_2d(points_by_curve_masked.total_size());
  mask.foreach_index(GrainSize(1024), [&](const int src_curve, const int dst_curve) {
    const IndexRange src_points = points_by_curve[src_curve];
    const IndexRange dst_points = points_by_curve_masked[dst_curve];
    for (const int i : src_points.index_range()) {
      const int src = src_points[i];
      const int dst = dst_points[i];
      positions_2d[dst] = double2(positions[src].x, positions[src].y);
    }
  });

  Array<Vector<int>> faces(points_by_curve_masked.size());
  fill_curve_vert_indices(points_by_curve_masked, faces);

  meshintersect::CDT_input<double> input;
  input.need_ids = false;
  input.vert = std::move(positions_2d);
  input.face = std::move(faces);

  return delaunay_2d_calc(input, output_type);
}

static Array<meshintersect::CDT_result<double>> do_group_aware_cdt(
    const bke::CurvesGeometry &curves,
    const CDT_output_type output_type,
    const Field<int> &group_index_field)
{
  const bke::GeometryFieldContext field_context{curves, AttrDomain::Curve};
  fn::FieldEvaluator data_evaluator{field_context, curves.curves_num()};
  data_evaluator.add(group_index_field);
  data_evaluator.evaluate();
  const VArray<int> curve_group_ids = data_evaluator.get_evaluated<int>(0);

  if (curve_group_ids.is_single()) {
    return {do_cdt(curves, output_type)};
  }

  VectorSet<int> group_indexing;
  IndexMaskMemory mask_memory;
  const Vector<IndexMask> group_masks = IndexMask::from_group_ids(
      curve_group_ids, mask_memory, group_indexing);
  const int groups_num = group_masks.size();

  Array<meshintersect::CDT_result<double>> cdt_results(groups_num);

  /* The grain size should be larger as each group gets smaller. */
  const int domain_size = curve_group_ids.size();
  const int avg_group_size = domain_size / groups_num;
  const int grain_size = std::max(8192 / avg_group_size, 1);
  threading::parallel_for(IndexRange(groups_num), grain_size, [&](const IndexRange range) {
    for (const int group_index : range) {
      const IndexMask &mask = group_masks[group_index];
      cdt_results[group_index] = do_cdt_with_mask(curves, output_type, mask);
    }
  });

  return cdt_results;
}

/* Converts multiple CDT results into a single Mesh. */
static Mesh *cdts_to_mesh(const Span<meshintersect::CDT_result<double>> results)
{
  /* Converting a single CDT result to a Mesh would be simple because the indices could be re-used.
   * However, in the general case here we need to combine several CDT results into a single Mesh,
   * which requires us to map the original indices to a new set of indices.
   * In order to allow for parallelization when appropriate, this implementation starts by
   * determining (for each domain) what range of indices in the final mesh data will be used for
   * each CDT result. The index ranges are represented as offsets, which are referred to as "group
   * offsets" to distinguish them from the other types of offsets we need to work with here.
   * Since it's likely that most invocations will only have a single CDT result, it's important
   * that case is made as optimal as feasible. */

  Array<int> vert_groups_data(results.size() + 1);
  Array<int> edge_groups_data(results.size() + 1);
  Array<int> face_groups_data(results.size() + 1);
  Array<int> loop_groups_data(results.size() + 1);
  threading::parallel_for(results.index_range(), 1024, [&](const IndexRange results_range) {
    for (const int i_result : results_range) {
      const meshintersect::CDT_result<double> &result = results[i_result];
      vert_groups_data[i_result] = result.vert.size();
      edge_groups_data[i_result] = result.edge.size();
      face_groups_data[i_result] = result.face.size();
      int loop_len = 0;
      for (const Vector<int> &face : result.face) {
        loop_len += face.size();
      }
      loop_groups_data[i_result] = loop_len;
    }
  });

  const OffsetIndices vert_groups = offset_indices::accumulate_counts_to_offsets(vert_groups_data);
  const OffsetIndices edge_groups = offset_indices::accumulate_counts_to_offsets(edge_groups_data);
  const OffsetIndices face_groups = offset_indices::accumulate_counts_to_offsets(face_groups_data);
  const OffsetIndices loop_groups = offset_indices::accumulate_counts_to_offsets(loop_groups_data);

  Mesh *mesh = BKE_mesh_new_nomain(vert_groups.total_size(),
                                   edge_groups.total_size(),
                                   face_groups.total_size(),
                                   loop_groups.total_size());

  MutableSpan<float3> all_positions = mesh->vert_positions_for_write();
  MutableSpan<int2> all_edges = mesh->edges_for_write();
  MutableSpan<int> all_face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> all_corner_verts = mesh->corner_verts_for_write();

  threading::parallel_for(results.index_range(), 1024, [&](const IndexRange results_range) {
    for (const int i_result : results_range) {
      const meshintersect::CDT_result<double> &result = results[i_result];
      const IndexRange verts_range = vert_groups[i_result];
      const IndexRange edges_range = edge_groups[i_result];
      const IndexRange faces_range = face_groups[i_result];
      const IndexRange loops_range = loop_groups[i_result];

      MutableSpan<float3> positions = all_positions.slice(verts_range);
      for (const int i : result.vert.index_range()) {
        positions[i] = float3(float(result.vert[i].x), float(result.vert[i].y), 0.0f);
      }

      MutableSpan<int2> edges = all_edges.slice(edges_range);
      for (const int i : result.edge.index_range()) {
        edges[i] = int2(result.edge[i].first + verts_range.start(),
                        result.edge[i].second + verts_range.start());
      }

      MutableSpan<int> face_offsets = all_face_offsets.slice(faces_range);
      MutableSpan<int> corner_verts = all_corner_verts.slice(loops_range);
      int i_face_corner = 0;
      for (const int i_face : result.face.index_range()) {
        face_offsets[i_face] = i_face_corner + loops_range.start();
        for (const int i_corner : result.face[i_face].index_range()) {
          corner_verts[i_face_corner] = result.face[i_face][i_corner] + verts_range.start();
          i_face_corner++;
        }
      }
    }
  });

  /* The delaunay triangulation doesn't seem to return all of the necessary all_edges, even in
   * triangulation mode. */
  bke::mesh_calc_edges(*mesh, true, false);
  bke::mesh_smooth_set(*mesh, false);

  mesh->tag_overlapping_none();

  return mesh;
}

static void curve_fill_calculate(GeometrySet &geometry_set,
                                 const GeometryNodeCurveFillMode mode,
                                 const Field<int> &group_index)
{
  const CDT_output_type output_type = (mode == GEO_NODE_CURVE_FILL_MODE_NGONS) ?
                                          CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES :
                                          CDT_INSIDE_WITH_HOLES;
  if (geometry_set.has_curves()) {
    const Curves &curves_id = *geometry_set.get_curves();
    const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
    if (curves.curves_num() > 0) {
      const Array<meshintersect::CDT_result<double>> results = do_group_aware_cdt(
          curves, output_type, group_index);
      Mesh *mesh = cdts_to_mesh(results);
      geometry_set.replace_mesh(mesh);
    }
    geometry_set.replace_curves(nullptr);
  }

  if (geometry_set.has_grease_pencil()) {
    using namespace blender::bke::greasepencil;
    const GreasePencil &grease_pencil = *geometry_set.get_grease_pencil();
    Vector<Mesh *> mesh_by_layer(grease_pencil.layers().size(), nullptr);
    for (const int layer_index : grease_pencil.layers().index_range()) {
      const Drawing *drawing = grease_pencil.get_eval_drawing(grease_pencil.layer(layer_index));
      if (drawing == nullptr) {
        continue;
      }
      const bke::CurvesGeometry &src_curves = drawing->strokes();
      if (src_curves.is_empty()) {
        continue;
      }
      const Array<meshintersect::CDT_result<double>> results = do_group_aware_cdt(
          src_curves, output_type, group_index);
      mesh_by_layer[layer_index] = cdts_to_mesh(results);
    }
    if (!mesh_by_layer.is_empty()) {
      InstancesComponent &instances_component =
          geometry_set.get_component_for_write<InstancesComponent>();
      bke::Instances *instances = instances_component.get_for_write();
      if (instances == nullptr) {
        instances = new bke::Instances();
        instances_component.replace(instances);
      }
      for (Mesh *mesh : mesh_by_layer) {
        if (!mesh) {
          /* Add an empty reference so the number of layers and instances match.
           * This makes it easy to reconstruct the layers afterwards and keep their attributes.
           * Although in this particular case we don't propagate the attributes. */
          const int handle = instances->add_reference(bke::InstanceReference());
          instances->add_instance(handle, float4x4::identity());
          continue;
        }
        GeometrySet temp_set = GeometrySet::from_mesh(mesh);
        const int handle = instances->add_reference(bke::InstanceReference{temp_set});
        instances->add_instance(handle, float4x4::identity());
      }
    }
    geometry_set.replace_grease_pencil(nullptr);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<int> group_index = params.extract_input<Field<int>>("Group ID");
  const GeometryNodeCurveFillMode mode = params.extract_input<GeometryNodeCurveFillMode>("Mode");

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry) {
    curve_fill_calculate(geometry, mode, group_index);
  });

  params.set_output("Mesh", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeFillCurve", GEO_NODE_FILL_CURVE);
  ntype.ui_name = "Fill Curve";
  ntype.ui_description =
      "Generate a mesh on the XY plane with faces on the inside of input curves";
  ntype.enum_name_legacy = "FILL_CURVE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryCurveFill", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_fill_cc
