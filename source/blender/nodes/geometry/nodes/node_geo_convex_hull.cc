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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_spline.hh"

#include "node_geometry_util.hh"

#ifdef WITH_BULLET
#  include "RBI_hull_api.h"
#endif

namespace blender::nodes {

static void geo_node_convex_hull_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_output<decl::Geometry>(N_("Convex Hull"));
}

using bke::GeometryInstanceGroup;

#ifdef WITH_BULLET

static Mesh *hull_from_bullet(const Mesh *mesh, Span<float3> coords)
{
  plConvexHull hull = plConvexHullCompute((float(*)[3])coords.data(), coords.size());

  const int num_verts = plConvexHullNumVertices(hull);
  const int num_faces = num_verts <= 2 ? 0 : plConvexHullNumFaces(hull);
  const int num_loops = num_verts <= 2 ? 0 : plConvexHullNumLoops(hull);
  /* Half as many edges as loops, because the mesh is manifold. */
  const int num_edges = num_verts == 2 ? 1 : num_verts < 2 ? 0 : num_loops / 2;

  /* Create Mesh *result with proper capacity. */
  Mesh *result;
  if (mesh) {
    result = BKE_mesh_new_nomain_from_template(
        mesh, num_verts, num_edges, 0, num_loops, num_faces);
  }
  else {
    result = BKE_mesh_new_nomain(num_verts, num_edges, 0, num_loops, num_faces);
    BKE_id_material_eval_ensure_default_slot(&result->id);
  }

  /* Copy vertices. */
  for (const int i : IndexRange(num_verts)) {
    float co[3];
    int original_index;
    plConvexHullGetVertex(hull, i, co, &original_index);

    if (original_index >= 0 && original_index < coords.size()) {
#  if 0 /* Disabled because it only works for meshes, not predictable enough. */
      /* Copy custom data on vertices, like vertex groups etc. */
      if (mesh && original_index < mesh->totvert) {
        CustomData_copy_data(&mesh->vdata, &result->vdata, (int)original_index, (int)i, 1);
      }
#  endif
      /* Copy the position of the original point. */
      copy_v3_v3(result->mvert[i].co, co);
    }
    else {
      BLI_assert_msg(0, "Unexpected new vertex in hull output");
    }
  }

  /* Copy edges and loops. */

  /* NOTE: ConvexHull from Bullet uses a half-edge data structure
   * for its mesh. To convert that, each half-edge needs to be converted
   * to a loop and edges need to be created from that. */
  Array<MLoop> mloop_src(num_loops);
  uint edge_index = 0;
  for (const int i : IndexRange(num_loops)) {
    int v_from;
    int v_to;
    plConvexHullGetLoop(hull, i, &v_from, &v_to);

    mloop_src[i].v = (uint)v_from;
    /* Add edges for ascending order loops only. */
    if (v_from < v_to) {
      MEdge &edge = result->medge[edge_index];
      edge.v1 = v_from;
      edge.v2 = v_to;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;

      /* Write edge index into both loops that have it. */
      int reverse_index = plConvexHullGetReversedLoopIndex(hull, i);
      mloop_src[i].e = edge_index;
      mloop_src[reverse_index].e = edge_index;
      edge_index++;
    }
  }
  if (num_edges == 1) {
    /* In this case there are no loops. */
    MEdge &edge = result->medge[0];
    edge.v1 = 0;
    edge.v2 = 1;
    edge.flag |= ME_EDGEDRAW | ME_EDGERENDER | ME_LOOSEEDGE;
    edge_index++;
  }
  BLI_assert(edge_index == num_edges);

  /* Copy faces. */
  Array<int> loops;
  int j = 0;
  MLoop *loop = result->mloop;
  for (const int i : IndexRange(num_faces)) {
    const int len = plConvexHullGetFaceSize(hull, i);

    BLI_assert(len > 2);

    /* Get face loop indices. */
    loops.reinitialize(len);
    plConvexHullGetFaceLoops(hull, i, loops.data());

    MPoly &face = result->mpoly[i];
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

  BKE_mesh_normals_tag_dirty(result);
  return result;
}

static Mesh *compute_hull(const GeometrySet &geometry_set)
{
  int span_count = 0;
  int count = 0;
  int total_size = 0;

  Span<float3> positions_span;

  if (geometry_set.has_mesh()) {
    count++;
    const MeshComponent *component = geometry_set.get_component_for_read<MeshComponent>();
    total_size += component->attribute_domain_size(ATTR_DOMAIN_POINT);
  }

  if (geometry_set.has_pointcloud()) {
    count++;
    span_count++;
    const PointCloudComponent *component =
        geometry_set.get_component_for_read<PointCloudComponent>();
    GVArray_Typed<float3> varray = component->attribute_get_for_read<float3>(
        "position", ATTR_DOMAIN_POINT, {0, 0, 0});
    total_size += varray->size();
    positions_span = varray->get_internal_span();
  }

  if (geometry_set.has_curve()) {
    const CurveEval &curve = *geometry_set.get_curve_for_read();
    for (const SplinePtr &spline : curve.splines()) {
      positions_span = spline->evaluated_positions();
      total_size += positions_span.size();
      count++;
      span_count++;
    }
  }

  if (count == 0) {
    return nullptr;
  }

  /* If there is only one positions virtual array and it is already contiguous, avoid copying
   * all of the positions and instead pass the span directly to the convex hull function. */
  if (span_count == 1 && count == 1) {
    return hull_from_bullet(nullptr, positions_span);
  }

  Array<float3> positions(total_size);
  int offset = 0;

  if (geometry_set.has_mesh()) {
    const MeshComponent *component = geometry_set.get_component_for_read<MeshComponent>();
    GVArray_Typed<float3> varray = component->attribute_get_for_read<float3>(
        "position", ATTR_DOMAIN_POINT, {0, 0, 0});
    varray->materialize(positions.as_mutable_span().slice(offset, varray.size()));
    offset += varray.size();
  }

  if (geometry_set.has_pointcloud()) {
    const PointCloudComponent *component =
        geometry_set.get_component_for_read<PointCloudComponent>();
    GVArray_Typed<float3> varray = component->attribute_get_for_read<float3>(
        "position", ATTR_DOMAIN_POINT, {0, 0, 0});
    varray->materialize(positions.as_mutable_span().slice(offset, varray.size()));
    offset += varray.size();
  }

  if (geometry_set.has_curve()) {
    const CurveEval &curve = *geometry_set.get_curve_for_read();
    for (const SplinePtr &spline : curve.splines()) {
      Span<float3> array = spline->evaluated_positions();
      positions.as_mutable_span().slice(offset, array.size()).copy_from(array);
      offset += array.size();
    }
  }

  return hull_from_bullet(geometry_set.get_mesh_for_read(), positions);
}

/* Since only positions are read from the instances, this can be used as an internal optimization
 * to avoid the cost of realizing instances before the node. But disable this for now, since
 * re-enabling that optimization will be a separate step. */
#  if 0
static void read_positions(const GeometryComponent &component,
                                           Span<float4x4> transforms,
                                           Vector<float3> *r_coords)
{
  GVArray_Typed<float3> positions = component.attribute_get_for_read<float3>(
      "position", ATTR_DOMAIN_POINT, {0, 0, 0});

  /* NOTE: could use convex hull operation here to
   * cut out some vertices, before accumulating,
   * but can also be done by the user beforehand. */

  r_coords->reserve(r_coords->size() + positions.size() * transforms.size());
  for (const float4x4 &transform : transforms) {
    for (const int i : positions.index_range()) {
      const float3 position = positions[i];
      const float3 transformed_position = transform * position;
      r_coords->append(transformed_position);
    }
  }
}

static void read_curve_positions(const CurveEval &curve,
                                 Span<float4x4> transforms,
                                 Vector<float3> *r_coords)
{
  const Array<int> offsets = curve.evaluated_point_offsets();
  const int total_size = offsets.last();
  r_coords->reserve(r_coords->size() + total_size * transforms.size());
  for (const SplinePtr &spline : curve.splines()) {
    Span<float3> positions = spline->evaluated_positions();
    for (const float4x4 &transform : transforms) {
      for (const float3 &position : positions) {
        r_coords->append(transform * position);
      }
    }
  }
}

static Mesh *convex_hull_from_instances(const GeometrySet &geometry_set)
{
  Vector<GeometryInstanceGroup> set_groups;
  bke::geometry_set_gather_instances(geometry_set, set_groups);

  Vector<float3> coords;

  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    Span<float4x4> transforms = set_group.transforms;

    if (set.has_pointcloud()) {
      read_positions(*set.get_component_for_read<PointCloudComponent>(), transforms, &coords);
    }
    if (set.has_mesh()) {
      read_positions(*set.get_component_for_read<MeshComponent>(), transforms, &coords);
    }
    if (set.has_curve()) {
      read_curve_positions(*set.get_curve_for_read(), transforms, &coords);
    }
  }
  return hull_from_bullet(nullptr, coords);
}
#  endif

#endif /* WITH_BULLET */

static void geo_node_convex_hull_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

#ifdef WITH_BULLET

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    Mesh *mesh = compute_hull(geometry_set);
    geometry_set.replace_mesh(mesh);
    geometry_set.keep_only({GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_INSTANCES});
  });

  params.set_output("Convex Hull", std::move(geometry_set));
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without Bullet"));
  params.set_output("Convex Hull", geometry_set);
#endif /* WITH_BULLET */
}

}  // namespace blender::nodes

void register_node_type_geo_convex_hull()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CONVEX_HULL, "Convex Hull", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_convex_hull_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_convex_hull_exec;
  nodeRegisterType(&ntype);
}
