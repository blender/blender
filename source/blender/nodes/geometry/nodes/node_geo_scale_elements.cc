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

#include "BLI_array.hh"
#include "BLI_disjoint_set.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_scale_elements_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Float>(N_("Scale"), "Scale").default_value(1.0f).min(0.0f).supports_field();
  b.add_input<decl::Vector>(N_("Center"))
      .subtype(PROP_TRANSLATION)
      .implicit_field()
      .description(N_("Origin of the scaling for each element. If multiple elements are "
                      "connected, their center is averaged"));
  b.add_input<decl::Vector>(N_("Axis"))
      .default_value({1.0f, 0.0f, 0.0f})
      .supports_field()
      .description(N_("Direction in which to scale the element"));
  b.add_output<decl::Geometry>(N_("Geometry"));
};

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "scale_mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = ATTR_DOMAIN_FACE;
  node->custom2 = GEO_NODE_SCALE_ELEMENTS_UNIFORM;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *geometry_socket = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *selection_socket = geometry_socket->next;
  bNodeSocket *scale_float_socket = selection_socket->next;
  bNodeSocket *center_socket = scale_float_socket->next;
  bNodeSocket *axis_socket = center_socket->next;

  const GeometryNodeScaleElementsMode mode = static_cast<GeometryNodeScaleElementsMode>(
      node->custom2);
  const bool use_single_axis = mode == GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS;

  nodeSetSocketAvailability(ntree, axis_socket, use_single_axis);
}

struct UniformScaleFields {
  Field<bool> selection;
  Field<float> scale;
  Field<float3> center;
};

struct UniformScaleParams {
  IndexMask selection;
  VArray<float> scales;
  VArray<float3> centers;
};

struct AxisScaleFields {
  Field<bool> selection;
  Field<float> scale;
  Field<float3> center;
  Field<float3> axis;
};

struct AxisScaleParams {
  IndexMask selection;
  VArray<float> scales;
  VArray<float3> centers;
  VArray<float3> axis_vectors;
};

/**
 * When multiple elements share the same vertices, they are scaled together.
 */
struct ElementIsland {
  /* Either face or edge indices. */
  Vector<int> element_indices;
};

static float3 transform_with_uniform_scale(const float3 &position,
                                           const float3 &center,
                                           const float scale)
{
  const float3 diff = position - center;
  const float3 scaled_diff = scale * diff;
  const float3 new_position = center + scaled_diff;
  return new_position;
}

static float4x4 create_single_axis_transform(const float3 &center,
                                             const float3 &axis,
                                             const float scale)
{
  /* Scale along x axis. The other axis need to be orthogonal, but their specific value does not
   * matter. */
  const float3 x_axis = math::normalize(axis);
  float3 y_axis = math::cross(x_axis, float3(0.0f, 0.0f, 1.0f));
  if (math::is_zero(y_axis)) {
    y_axis = math::cross(x_axis, float3(0.0f, 1.0f, 0.0f));
  }
  y_axis = math::normalize(y_axis);
  const float3 z_axis = math::cross(x_axis, y_axis);

  float4x4 transform = float4x4::identity();

  /* Move scaling center to the origin. */
  sub_v3_v3(transform.values[3], center);

  /* `base_change` and `base_change_inv` are used to rotate space so that scaling along the
   * provided axis is the same as scaling along the x axis. */
  float4x4 base_change = float4x4::identity();
  copy_v3_v3(base_change.values[0], x_axis);
  copy_v3_v3(base_change.values[1], y_axis);
  copy_v3_v3(base_change.values[2], z_axis);

  /* Can invert by transposing, because the matrix is orthonormal. */
  float4x4 base_change_inv = base_change.transposed();

  float4x4 scale_transform = float4x4::identity();
  scale_transform.values[0][0] = scale;

  transform = base_change * scale_transform * base_change_inv * transform;

  /* Move scaling center back to where it was. */
  add_v3_v3(transform.values[3], center);

  return transform;
}

using GetVertexIndicesFn =
    FunctionRef<void(const Mesh &mesh, int element_index, VectorSet<int> &r_vertex_indices)>;

static void scale_vertex_islands_uniformly(Mesh &mesh,
                                           const Span<ElementIsland> islands,
                                           const UniformScaleParams &params,
                                           const GetVertexIndicesFn get_vertex_indices)
{
  threading::parallel_for(islands.index_range(), 256, [&](const IndexRange range) {
    for (const int island_index : range) {
      const ElementIsland &island = islands[island_index];

      float scale = 0.0f;
      float3 center = {0.0f, 0.0f, 0.0f};

      VectorSet<int> vertex_indices;
      for (const int poly_index : island.element_indices) {
        get_vertex_indices(mesh, poly_index, vertex_indices);
        center += params.centers[poly_index];
        scale += params.scales[poly_index];
      }

      /* Divide by number of elements to get the average. */
      const float f = 1.0f / island.element_indices.size();
      scale *= f;
      center *= f;

      for (const int vert_index : vertex_indices) {
        MVert &vert = mesh.mvert[vert_index];
        const float3 old_position = vert.co;
        const float3 new_position = transform_with_uniform_scale(old_position, center, scale);
        copy_v3_v3(vert.co, new_position);
      }
    }
  });

  /* Positions have changed, so the normals will have to be recomputed. */
  BKE_mesh_normals_tag_dirty(&mesh);
}

static void scale_vertex_islands_on_axis(Mesh &mesh,
                                         const Span<ElementIsland> islands,
                                         const AxisScaleParams &params,
                                         const GetVertexIndicesFn get_vertex_indices)
{
  threading::parallel_for(islands.index_range(), 256, [&](const IndexRange range) {
    for (const int island_index : range) {
      const ElementIsland &island = islands[island_index];

      float scale = 0.0f;
      float3 center = {0.0f, 0.0f, 0.0f};
      float3 axis = {0.0f, 0.0f, 0.0f};

      VectorSet<int> vertex_indices;
      for (const int poly_index : island.element_indices) {
        get_vertex_indices(mesh, poly_index, vertex_indices);
        center += params.centers[poly_index];
        scale += params.scales[poly_index];
        axis += params.axis_vectors[poly_index];
      }

      /* Divide by number of elements to get the average. */
      const float f = 1.0f / island.element_indices.size();
      scale *= f;
      center *= f;
      axis *= f;

      if (math::is_zero(axis)) {
        axis = float3(1.0f, 0.0f, 0.0f);
      }

      const float4x4 transform = create_single_axis_transform(center, axis, scale);
      for (const int vert_index : vertex_indices) {
        MVert &vert = mesh.mvert[vert_index];
        const float3 old_position = vert.co;
        const float3 new_position = transform * old_position;
        copy_v3_v3(vert.co, new_position);
      }
    }
  });

  /* Positions have changed, so the normals will have to be recomputed. */
  BKE_mesh_normals_tag_dirty(&mesh);
}

static Vector<ElementIsland> prepare_face_islands(const Mesh &mesh, const IndexMask face_selection)
{
  /* Use the disjoint set data structure to determine which vertices have to be scaled together. */
  DisjointSet disjoint_set(mesh.totvert);
  for (const int poly_index : face_selection) {
    const MPoly &poly = mesh.mpoly[poly_index];
    const Span<MLoop> poly_loops{mesh.mloop + poly.loopstart, poly.totloop};
    for (const int loop_index : IndexRange(poly.totloop - 1)) {
      const int v1 = poly_loops[loop_index].v;
      const int v2 = poly_loops[loop_index + 1].v;
      disjoint_set.join(v1, v2);
    }
    disjoint_set.join(poly_loops.first().v, poly_loops.last().v);
  }

  VectorSet<int> island_ids;
  Vector<ElementIsland> islands;
  /* There are at most as many islands as there are selected faces. */
  islands.reserve(face_selection.size());

  /* Gather all of the face indices in each island into separate vectors. */
  for (const int poly_index : face_selection) {
    const MPoly &poly = mesh.mpoly[poly_index];
    const Span<MLoop> poly_loops{mesh.mloop + poly.loopstart, poly.totloop};
    const int island_id = disjoint_set.find_root(poly_loops[0].v);
    const int island_index = island_ids.index_of_or_add(island_id);
    if (island_index == islands.size()) {
      islands.append_as();
    }
    ElementIsland &island = islands[island_index];
    island.element_indices.append(poly_index);
  }

  return islands;
}

static void get_face_vertices(const Mesh &mesh, int face_index, VectorSet<int> &r_vertex_indices)
{
  const MPoly &poly = mesh.mpoly[face_index];
  const Span<MLoop> poly_loops{mesh.mloop + poly.loopstart, poly.totloop};
  for (const MLoop &loop : poly_loops) {
    r_vertex_indices.add(loop.v);
  }
}

static AxisScaleParams evaluate_axis_scale_fields(FieldEvaluator &evaluator,
                                                  const AxisScaleFields &fields)
{
  AxisScaleParams out;
  evaluator.set_selection(fields.selection);
  evaluator.add(fields.scale, &out.scales);
  evaluator.add(fields.center, &out.centers);
  evaluator.add(fields.axis, &out.axis_vectors);
  evaluator.evaluate();
  out.selection = evaluator.get_evaluated_selection_as_mask();
  return out;
}

static void scale_faces_on_axis(MeshComponent &mesh_component, const AxisScaleFields &fields)
{
  Mesh &mesh = *mesh_component.get_for_write();
  mesh.mvert = static_cast<MVert *>(
      CustomData_duplicate_referenced_layer(&mesh.vdata, CD_MVERT, mesh.totvert));

  GeometryComponentFieldContext field_context{mesh_component, ATTR_DOMAIN_FACE};
  FieldEvaluator evaluator{field_context, mesh.totpoly};
  AxisScaleParams params = evaluate_axis_scale_fields(evaluator, fields);

  Vector<ElementIsland> island = prepare_face_islands(mesh, params.selection);
  scale_vertex_islands_on_axis(mesh, island, params, get_face_vertices);
}

static UniformScaleParams evaluate_uniform_scale_fields(FieldEvaluator &evaluator,
                                                        const UniformScaleFields &fields)
{
  UniformScaleParams out;
  evaluator.set_selection(fields.selection);
  evaluator.add(fields.scale, &out.scales);
  evaluator.add(fields.center, &out.centers);
  evaluator.evaluate();
  out.selection = evaluator.get_evaluated_selection_as_mask();
  return out;
}

static void scale_faces_uniformly(MeshComponent &mesh_component, const UniformScaleFields &fields)
{
  Mesh &mesh = *mesh_component.get_for_write();
  mesh.mvert = static_cast<MVert *>(
      CustomData_duplicate_referenced_layer(&mesh.vdata, CD_MVERT, mesh.totvert));

  GeometryComponentFieldContext field_context{mesh_component, ATTR_DOMAIN_FACE};
  FieldEvaluator evaluator{field_context, mesh.totpoly};
  UniformScaleParams params = evaluate_uniform_scale_fields(evaluator, fields);

  Vector<ElementIsland> island = prepare_face_islands(mesh, params.selection);
  scale_vertex_islands_uniformly(mesh, island, params, get_face_vertices);
}

static Vector<ElementIsland> prepare_edge_islands(const Mesh &mesh, const IndexMask edge_selection)
{
  /* Use the disjoint set data structure to determine which vertices have to be scaled together. */
  DisjointSet disjoint_set(mesh.totvert);
  for (const int edge_index : edge_selection) {
    const MEdge &edge = mesh.medge[edge_index];
    disjoint_set.join(edge.v1, edge.v2);
  }

  VectorSet<int> island_ids;
  Vector<ElementIsland> islands;
  /* There are at most as many islands as there are selected edges. */
  islands.reserve(edge_selection.size());

  /* Gather all of the edge indices in each island into separate vectors. */
  for (const int edge_index : edge_selection) {
    const MEdge &edge = mesh.medge[edge_index];
    const int island_id = disjoint_set.find_root(edge.v1);
    const int island_index = island_ids.index_of_or_add(island_id);
    if (island_index == islands.size()) {
      islands.append_as();
    }
    ElementIsland &island = islands[island_index];
    island.element_indices.append(edge_index);
  }

  return islands;
}

static void get_edge_vertices(const Mesh &mesh, int edge_index, VectorSet<int> &r_vertex_indices)
{
  const MEdge &edge = mesh.medge[edge_index];
  r_vertex_indices.add(edge.v1);
  r_vertex_indices.add(edge.v2);
}

static void scale_edges_uniformly(MeshComponent &mesh_component, const UniformScaleFields &fields)
{
  Mesh &mesh = *mesh_component.get_for_write();
  mesh.mvert = static_cast<MVert *>(
      CustomData_duplicate_referenced_layer(&mesh.vdata, CD_MVERT, mesh.totvert));

  GeometryComponentFieldContext field_context{mesh_component, ATTR_DOMAIN_EDGE};
  FieldEvaluator evaluator{field_context, mesh.totedge};
  UniformScaleParams params = evaluate_uniform_scale_fields(evaluator, fields);

  Vector<ElementIsland> island = prepare_edge_islands(mesh, params.selection);
  scale_vertex_islands_uniformly(mesh, island, params, get_edge_vertices);
}

static void scale_edges_on_axis(MeshComponent &mesh_component, const AxisScaleFields &fields)
{
  Mesh &mesh = *mesh_component.get_for_write();
  mesh.mvert = static_cast<MVert *>(
      CustomData_duplicate_referenced_layer(&mesh.vdata, CD_MVERT, mesh.totvert));

  GeometryComponentFieldContext field_context{mesh_component, ATTR_DOMAIN_EDGE};
  FieldEvaluator evaluator{field_context, mesh.totedge};
  AxisScaleParams params = evaluate_axis_scale_fields(evaluator, fields);

  Vector<ElementIsland> island = prepare_edge_islands(mesh, params.selection);
  scale_vertex_islands_on_axis(mesh, island, params, get_edge_vertices);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const AttributeDomain domain = static_cast<AttributeDomain>(node.custom1);
  const GeometryNodeScaleElementsMode scale_mode = static_cast<GeometryNodeScaleElementsMode>(
      node.custom2);

  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");

  Field<bool> selection_field = params.get_input<Field<bool>>("Selection");
  Field<float> scale_field = params.get_input<Field<float>>("Scale");
  Field<float3> center_field = params.get_input<Field<float3>>("Center");
  Field<float3> axis_field;
  if (scale_mode == GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS) {
    axis_field = params.get_input<Field<float3>>("Axis");
  }

  geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (!geometry.has_mesh()) {
      return;
    }
    MeshComponent &mesh_component = geometry.get_component_for_write<MeshComponent>();
    switch (domain) {
      case ATTR_DOMAIN_FACE: {
        switch (scale_mode) {
          case GEO_NODE_SCALE_ELEMENTS_UNIFORM: {
            scale_faces_uniformly(mesh_component, {selection_field, scale_field, center_field});
            break;
          }
          case GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS: {
            scale_faces_on_axis(mesh_component,
                                {selection_field, scale_field, center_field, axis_field});
            break;
          }
        }
        break;
      }
      case ATTR_DOMAIN_EDGE: {
        switch (scale_mode) {
          case GEO_NODE_SCALE_ELEMENTS_UNIFORM: {
            scale_edges_uniformly(mesh_component, {selection_field, scale_field, center_field});
            break;
          }
          case GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS: {
            scale_edges_on_axis(mesh_component,
                                {selection_field, scale_field, center_field, axis_field});
            break;
          }
        }
        break;
      }
      default:
        BLI_assert_unreachable();
        break;
    }
  });

  params.set_output("Geometry", std::move(geometry));
}

}  // namespace blender::nodes::node_geo_scale_elements_cc

void register_node_type_geo_scale_elements()
{
  namespace file_ns = blender::nodes::node_geo_scale_elements_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SCALE_ELEMENTS, "Scale Elements", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  nodeRegisterType(&ntype);
}
