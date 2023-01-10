/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_curves.hh"
#include "BKE_material.h"
#include "BKE_mesh.h"

#include "node_geometry_util.hh"

#ifdef WITH_BULLET
#  include "RBI_hull_api.h"
#endif

namespace blender::nodes::node_geo_convex_hull_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_output<decl::Geometry>(N_("Convex Hull"));
}

#ifdef WITH_BULLET

static Mesh *hull_from_bullet(const Mesh *mesh, Span<float3> coords)
{
  plConvexHull hull = plConvexHullCompute((float(*)[3])coords.data(), coords.size());

  const int verts_num = plConvexHullNumVertices(hull);
  const int faces_num = verts_num <= 2 ? 0 : plConvexHullNumFaces(hull);
  const int loops_num = verts_num <= 2 ? 0 : plConvexHullNumLoops(hull);
  /* Half as many edges as loops, because the mesh is manifold. */
  const int edges_num = verts_num == 2 ? 1 : verts_num < 2 ? 0 : loops_num / 2;

  /* Create Mesh *result with proper capacity. */
  Mesh *result;
  if (mesh) {
    result = BKE_mesh_new_nomain_from_template(
        mesh, verts_num, edges_num, 0, loops_num, faces_num);
  }
  else {
    result = BKE_mesh_new_nomain(verts_num, edges_num, 0, loops_num, faces_num);
    BKE_id_material_eval_ensure_default_slot(&result->id);
  }

  /* Copy vertices. */
  MutableSpan<float3> dst_positions = result->vert_positions_for_write();
  for (const int i : IndexRange(verts_num)) {
    int original_index;
    plConvexHullGetVertex(hull, i, dst_positions[i], &original_index);

    if (original_index >= 0 && original_index < coords.size()) {
#  if 0 /* Disabled because it only works for meshes, not predictable enough. */
      /* Copy custom data on vertices, like vertex groups etc. */
      if (mesh && original_index < mesh->totvert) {
        CustomData_copy_data(&mesh->vdata, &result->vdata, int(original_index), int(i), 1);
      }
#  endif
    }
    else {
      BLI_assert_msg(0, "Unexpected new vertex in hull output");
    }
  }

  /* Copy edges and loops. */

  /* NOTE: ConvexHull from Bullet uses a half-edge data structure
   * for its mesh. To convert that, each half-edge needs to be converted
   * to a loop and edges need to be created from that. */
  Array<MLoop> mloop_src(loops_num);
  uint edge_index = 0;
  MutableSpan<MEdge> edges = result->edges_for_write();

  for (const int i : IndexRange(loops_num)) {
    int v_from;
    int v_to;
    plConvexHullGetLoop(hull, i, &v_from, &v_to);

    mloop_src[i].v = uint(v_from);
    /* Add edges for ascending order loops only. */
    if (v_from < v_to) {
      MEdge &edge = edges[edge_index];
      edge.v1 = v_from;
      edge.v2 = v_to;
      edge.flag = ME_EDGEDRAW;

      /* Write edge index into both loops that have it. */
      int reverse_index = plConvexHullGetReversedLoopIndex(hull, i);
      mloop_src[i].e = edge_index;
      mloop_src[reverse_index].e = edge_index;
      edge_index++;
    }
  }
  if (edges_num == 1) {
    /* In this case there are no loops. */
    MEdge &edge = edges[0];
    edge.v1 = 0;
    edge.v2 = 1;
    edge.flag = ME_EDGEDRAW;
    edge_index++;
  }
  BLI_assert(edge_index == edges_num);

  /* Copy faces. */
  Array<int> loops;
  int j = 0;
  MutableSpan<MPoly> polys = result->polys_for_write();
  MutableSpan<MLoop> mesh_loops = result->loops_for_write();
  MLoop *loop = mesh_loops.data();

  for (const int i : IndexRange(faces_num)) {
    const int len = plConvexHullGetFaceSize(hull, i);

    BLI_assert(len > 2);

    /* Get face loop indices. */
    loops.reinitialize(len);
    plConvexHullGetFaceLoops(hull, i, loops.data());

    MPoly &face = polys[i];
    face.loopstart = j;
    face.totloop = len;
    for (const int k : IndexRange(len)) {
      MLoop &src_loop = mloop_src[loops[k]];
      loop->v = src_loop.v;
      loop->e = src_loop.e;
      loop++;
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

  if (const MeshComponent *component = geometry_set.get_component_for_read<MeshComponent>()) {
    count++;
    if (const VArray<float3> positions = component->attributes()->lookup<float3>(
            "position", ATTR_DOMAIN_POINT)) {
      if (positions.is_span()) {
        span_count++;
        positions_span = positions.get_internal_span();
      }
      total_num += positions.size();
    }
  }

  if (const PointCloudComponent *component =
          geometry_set.get_component_for_read<PointCloudComponent>()) {
    count++;
    if (const VArray<float3> positions = component->attributes()->lookup<float3>(
            "position", ATTR_DOMAIN_POINT)) {
      if (positions.is_span()) {
        span_count++;
        positions_span = positions.get_internal_span();
      }
      total_num += positions.size();
    }
  }

  if (const Curves *curves_id = geometry_set.get_curves_for_read()) {
    count++;
    span_count++;
    const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id->geometry);
    positions_span = curves.evaluated_positions();
    total_num += positions_span.size();
  }

  if (count == 0) {
    return nullptr;
  }

  /* If there is only one positions virtual array and it is already contiguous, avoid copying
   * all of the positions and instead pass the span directly to the convex hull function. */
  if (span_count == 1 && count == 1) {
    return hull_from_bullet(geometry_set.get_mesh_for_read(), positions_span);
  }

  Array<float3> positions(total_num);
  int offset = 0;

  if (const MeshComponent *component = geometry_set.get_component_for_read<MeshComponent>()) {
    if (const VArray<float3> varray = component->attributes()->lookup<float3>("position",
                                                                              ATTR_DOMAIN_POINT)) {
      varray.materialize(positions.as_mutable_span().slice(offset, varray.size()));
      offset += varray.size();
    }
  }

  if (const PointCloudComponent *component =
          geometry_set.get_component_for_read<PointCloudComponent>()) {
    if (const VArray<float3> varray = component->attributes()->lookup<float3>("position",
                                                                              ATTR_DOMAIN_POINT)) {
      varray.materialize(positions.as_mutable_span().slice(offset, varray.size()));
      offset += varray.size();
    }
  }

  if (const Curves *curves_id = geometry_set.get_curves_for_read()) {
    const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id->geometry);
    Span<float3> array = curves.evaluated_positions();
    positions.as_mutable_span().slice(offset, array.size()).copy_from(array);
    offset += array.size();
  }

  return hull_from_bullet(geometry_set.get_mesh_for_read(), positions);
}

#endif /* WITH_BULLET */

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

#ifdef WITH_BULLET

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    Mesh *mesh = compute_hull(geometry_set);
    geometry_set.replace_mesh(mesh);
    geometry_set.keep_only_during_modify({GEO_COMPONENT_TYPE_MESH});
  });

  params.set_output("Convex Hull", std::move(geometry_set));
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without Bullet"));
  params.set_default_remaining_outputs();
#endif /* WITH_BULLET */
}

}  // namespace blender::nodes::node_geo_convex_hull_cc

void register_node_type_geo_convex_hull()
{
  namespace file_ns = blender::nodes::node_geo_convex_hull_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CONVEX_HULL, "Convex Hull", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
