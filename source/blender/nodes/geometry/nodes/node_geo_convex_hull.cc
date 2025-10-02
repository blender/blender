/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_pointcloud_types.h"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

#ifdef WITH_BULLET
#  include "RBI_hull_api.h"
#endif

namespace blender::nodes::node_geo_convex_hull_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry").description("Points to compute the convex hull of");
  b.add_output<decl::Geometry>("Convex Hull").propagate_all_instance_attributes();
}

#ifdef WITH_BULLET

static Mesh *hull_from_bullet(const Mesh *mesh, Span<float3> coords)
{
  plConvexHull hull = plConvexHullCompute((float (*)[3])coords.data(), coords.size());

  const int verts_num = plConvexHullNumVertices(hull);
  const int faces_num = verts_num <= 2 ? 0 : plConvexHullNumFaces(hull);
  const int loops_num = verts_num <= 2 ? 0 : plConvexHullNumLoops(hull);
  /* Half as many edges as loops, because the mesh is manifold. */
  const int edges_num = verts_num == 2 ? 1 : verts_num < 2 ? 0 : loops_num / 2;

  /* Create Mesh *result with proper capacity. */
  Mesh *result;
  if (mesh) {
    result = BKE_mesh_new_nomain_from_template(mesh, verts_num, edges_num, faces_num, loops_num);
  }
  else {
    result = BKE_mesh_new_nomain(verts_num, edges_num, faces_num, loops_num);
    BKE_id_material_eval_ensure_default_slot(&result->id);
  }
  bke::mesh_smooth_set(*result, false);

  /* Copy vertices. */
  MutableSpan<float3> dst_positions = result->vert_positions_for_write();
  for (const int i : IndexRange(verts_num)) {
    float3 dummy_co;
    int original_index;
    plConvexHullGetVertex(hull, i, dummy_co, &original_index);
    if (UNLIKELY(!coords.index_range().contains(original_index))) {
      BLI_assert_unreachable();
      dst_positions[i] = float3(0);
      continue;
    }
    dst_positions[i] = coords[original_index];
  }

  /* Copy edges and loops. */

  /* NOTE: ConvexHull from Bullet uses a half-edge data structure
   * for its mesh. To convert that, each half-edge needs to be converted
   * to a loop and edges need to be created from that. */
  Array<int> corner_verts(loops_num);
  Array<int> corner_edges(loops_num);
  uint edge_index = 0;
  MutableSpan<int2> edges = result->edges_for_write();

  for (const int i : IndexRange(loops_num)) {
    int v_from;
    int v_to;
    plConvexHullGetLoop(hull, i, &v_from, &v_to);

    corner_verts[i] = v_from;
    /* Add edges for ascending order loops only. */
    if (v_from < v_to) {
      edges[edge_index] = int2(v_from, v_to);

      /* Write edge index into both loops that have it. */
      int reverse_index = plConvexHullGetReversedLoopIndex(hull, i);
      corner_edges[i] = edge_index;
      corner_edges[reverse_index] = edge_index;
      edge_index++;
    }
  }
  if (edges_num == 1) {
    /* In this case there are no loops. */
    edges[0] = int2(0, 1);
    edge_index++;
  }
  BLI_assert(edge_index == edges_num);

  /* Copy faces. */
  Array<int> loops;
  int j = 0;
  MutableSpan<int> face_offsets = result->face_offsets_for_write();
  MutableSpan<int> mesh_corner_verts = result->corner_verts_for_write();
  MutableSpan<int> mesh_corner_edges = result->corner_edges_for_write();
  int dst_corner = 0;

  for (const int i : IndexRange(faces_num)) {
    const int len = plConvexHullGetFaceSize(hull, i);

    BLI_assert(len > 2);

    /* Get face loop indices. */
    loops.reinitialize(len);
    plConvexHullGetFaceLoops(hull, i, loops.data());

    face_offsets[i] = j;
    for (const int k : IndexRange(len)) {
      mesh_corner_verts[dst_corner] = corner_verts[loops[k]];
      mesh_corner_edges[dst_corner] = corner_edges[loops[k]];
      dst_corner++;
    }
    j += len;
  }

  plConvexHullDelete(hull);
  return result;
}

static Mesh *compute_hull(const GeometrySet &geometry_set)
{
  int span_count = 0;
  int count = 0;
  int total_num = 0;

  Span<float3> positions_span;

  if (const Mesh *mesh = geometry_set.get_mesh()) {
    count++;
    if (const VArray positions = *mesh->attributes().lookup<float3>("position")) {
      if (positions.is_span()) {
        span_count++;
        positions_span = positions.get_internal_span();
      }
      total_num += positions.size();
    }
  }

  if (const PointCloud *points = geometry_set.get_pointcloud()) {
    count++;
    if (const VArray positions = *points->attributes().lookup<float3>("position")) {
      if (positions.is_span()) {
        span_count++;
        positions_span = positions.get_internal_span();
      }
      total_num += positions.size();
    }
  }

  if (const Curves *curves_id = geometry_set.get_curves()) {
    count++;
    span_count++;
    const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    positions_span = curves.evaluated_positions();
    total_num += positions_span.size();
  }

  if (count == 0) {
    return nullptr;
  }

  /* If there is only one positions virtual array and it is already contiguous, avoid copying
   * all of the positions and instead pass the span directly to the convex hull function. */
  if (span_count == 1 && count == 1) {
    return hull_from_bullet(geometry_set.get_mesh(), positions_span);
  }

  Array<float3> positions(total_num);
  int offset = 0;

  if (const Mesh *mesh = geometry_set.get_mesh()) {
    if (const VArray varray = *mesh->attributes().lookup<float3>("position")) {
      varray.materialize(positions.as_mutable_span().slice(offset, varray.size()));
      offset += varray.size();
    }
  }

  if (const PointCloud *points = geometry_set.get_pointcloud()) {
    if (const VArray varray = *points->attributes().lookup<float3>("position")) {
      varray.materialize(positions.as_mutable_span().slice(offset, varray.size()));
      offset += varray.size();
    }
  }

  if (const Curves *curves_id = geometry_set.get_curves()) {
    const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    Span<float3> array = curves.evaluated_positions();
    positions.as_mutable_span().slice(offset, array.size()).copy_from(array);
    offset += array.size();
  }

  return hull_from_bullet(geometry_set.get_mesh(), positions);
}

static void convex_hull_grease_pencil(GeometrySet &geometry_set)
{
  using namespace blender::bke::greasepencil;

  const GreasePencil &grease_pencil = *geometry_set.get_grease_pencil();
  Array<Mesh *> mesh_by_layer(grease_pencil.layers().size(), nullptr);

  for (const int layer_index : grease_pencil.layers().index_range()) {
    const Drawing *drawing = grease_pencil.get_eval_drawing(grease_pencil.layer(layer_index));
    if (drawing == nullptr) {
      continue;
    }
    const bke::CurvesGeometry &curves = drawing->strokes();
    const Span<float3> positions_span = curves.evaluated_positions();
    if (positions_span.is_empty()) {
      continue;
    }
    mesh_by_layer[layer_index] = hull_from_bullet(nullptr, positions_span);
  }

  if (mesh_by_layer.is_empty()) {
    return;
  }

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
  geometry_set.replace_grease_pencil(nullptr);
}

#endif /* WITH_BULLET */

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

#ifdef WITH_BULLET

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    Mesh *mesh = compute_hull(geometry_set);
    if (mesh) {
      geometry::debug_randomize_mesh_order(mesh);
    }
    geometry_set.replace_mesh(mesh);
    if (geometry_set.has_grease_pencil()) {
      convex_hull_grease_pencil(geometry_set);
    }
    geometry_set.keep_only({GeometryComponent::Type::Mesh,
                            GeometryComponent::Type::Instance,
                            GeometryComponent::Type::Edit});
  });

  params.set_output("Convex Hull", std::move(geometry_set));
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without Bullet"));
  params.set_default_remaining_outputs();
#endif /* WITH_BULLET */
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeConvexHull", GEO_NODE_CONVEX_HULL);
  ntype.ui_name = "Convex Hull";
  ntype.ui_description =
      "Create a mesh that encloses all points in the input geometry with the smallest number of "
      "points";
  ntype.enum_name_legacy = "CONVEX_HULL";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_convex_hull_cc
