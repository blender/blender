/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_disjoint_set.hh"
#include "BLI_math_matrix.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BKE_mesh.hh"

#include "NOD_rna_define.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_scale_elements_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry").supported_type(GeometryComponent::Type::Mesh);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Float>("Scale", "Scale").default_value(1.0f).min(0.0f).field_on_all();
  b.add_input<decl::Vector>("Center")
      .subtype(PROP_TRANSLATION)
      .implicit_field_on_all(implicit_field_inputs::position)
      .description(
          "Origin of the scaling for each element. If multiple elements are connected, their "
          "center is averaged");
  b.add_input<decl::Vector>("Axis")
      .default_value({1.0f, 0.0f, 0.0f})
      .field_on_all()
      .description("Direction in which to scale the element")
      .make_available([](bNode &node) { node.custom2 = GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS; });
  b.add_output<decl::Geometry>("Geometry").propagate_all();
};

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "scale_mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
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

  const GeometryNodeScaleElementsMode mode = GeometryNodeScaleElementsMode(node->custom2);
  const bool use_single_axis = mode == GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS;

  bke::nodeSetSocketAvailability(ntree, axis_socket, use_single_axis);
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
  transform.location() -= center;

  /* `base_change` and `base_change_inv` are used to rotate space so that scaling along the
   * provided axis is the same as scaling along the x axis. */
  float4x4 base_change = float4x4::identity();
  base_change.x_axis() = x_axis;
  base_change.y_axis() = y_axis;
  base_change.z_axis() = z_axis;

  /* Can invert by transposing, because the matrix is orthonormal. */
  float4x4 base_change_inv = math::transpose(base_change);

  float4x4 scale_transform = float4x4::identity();
  scale_transform[0][0] = scale;

  transform = base_change * scale_transform * base_change_inv * transform;

  /* Move scaling center back to where it was. */
  transform.location() += center;

  return transform;
}

using GetVertexIndicesFn = FunctionRef<void(Span<int2> edges,
                                            OffsetIndices<int> faces,
                                            Span<int> corner_verts,
                                            int element_index,
                                            VectorSet<int> &r_vertex_indices)>;

static void scale_vertex_islands_uniformly(Mesh &mesh,
                                           const Span<ElementIsland> islands,
                                           const UniformScaleParams &params,
                                           const GetVertexIndicesFn get_vertex_indices)
{
  MutableSpan<float3> positions = mesh.vert_positions_for_write();
  const Span<int2> edges = mesh.edges();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  threading::parallel_for(islands.index_range(), 256, [&](const IndexRange range) {
    for (const int island_index : range) {
      const ElementIsland &island = islands[island_index];

      float scale = 0.0f;
      float3 center = {0.0f, 0.0f, 0.0f};

      VectorSet<int> vertex_indices;
      for (const int face_index : island.element_indices) {
        get_vertex_indices(edges, faces, corner_verts, face_index, vertex_indices);
        center += params.centers[face_index];
        scale += params.scales[face_index];
      }

      /* Divide by number of elements to get the average. */
      const float f = 1.0f / island.element_indices.size();
      scale *= f;
      center *= f;

      for (const int vert_index : vertex_indices) {
        positions[vert_index] = transform_with_uniform_scale(positions[vert_index], center, scale);
      }
    }
  });

  BKE_mesh_tag_positions_changed(&mesh);
}

static void scale_vertex_islands_on_axis(Mesh &mesh,
                                         const Span<ElementIsland> islands,
                                         const AxisScaleParams &params,
                                         const GetVertexIndicesFn get_vertex_indices)
{
  MutableSpan<float3> positions = mesh.vert_positions_for_write();
  const Span<int2> edges = mesh.edges();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  threading::parallel_for(islands.index_range(), 256, [&](const IndexRange range) {
    for (const int island_index : range) {
      const ElementIsland &island = islands[island_index];

      float scale = 0.0f;
      float3 center = {0.0f, 0.0f, 0.0f};
      float3 axis = {0.0f, 0.0f, 0.0f};

      VectorSet<int> vertex_indices;
      for (const int face_index : island.element_indices) {
        get_vertex_indices(edges, faces, corner_verts, face_index, vertex_indices);
        center += params.centers[face_index];
        scale += params.scales[face_index];
        axis += params.axis_vectors[face_index];
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
        positions[vert_index] = math::transform_point(transform, positions[vert_index]);
      }
    }
  });

  BKE_mesh_tag_positions_changed(&mesh);
}

static Vector<ElementIsland> prepare_face_islands(const Mesh &mesh,
                                                  const IndexMask &face_selection)
{
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  /* Use the disjoint set data structure to determine which vertices have to be scaled together. */
  DisjointSet<int> disjoint_set(mesh.totvert);
  face_selection.foreach_index([&](const int face_index) {
    const Span<int> face_verts = corner_verts.slice(faces[face_index]);
    for (const int loop_index : face_verts.index_range().drop_back(1)) {
      const int v1 = face_verts[loop_index];
      const int v2 = face_verts[loop_index + 1];
      disjoint_set.join(v1, v2);
    }
    disjoint_set.join(face_verts.first(), face_verts.last());
  });

  VectorSet<int> island_ids;
  Vector<ElementIsland> islands;
  /* There are at most as many islands as there are selected faces. */
  islands.reserve(face_selection.size());

  /* Gather all of the face indices in each island into separate vectors. */
  face_selection.foreach_index([&](const int face_index) {
    const Span<int> face_verts = corner_verts.slice(faces[face_index]);
    const int island_id = disjoint_set.find_root(face_verts[0]);
    const int island_index = island_ids.index_of_or_add(island_id);
    if (island_index == islands.size()) {
      islands.append_as();
    }
    ElementIsland &island = islands[island_index];
    island.element_indices.append(face_index);
  });

  return islands;
}

static void get_face_verts(const Span<int2> /*edges*/,
                           const OffsetIndices<int> faces,
                           const Span<int> corner_verts,
                           int face_index,
                           VectorSet<int> &r_vertex_indices)
{
  r_vertex_indices.add_multiple(corner_verts.slice(faces[face_index]));
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

static void scale_faces_on_axis(Mesh &mesh, const AxisScaleFields &fields)
{
  const bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_FACE};
  FieldEvaluator evaluator{field_context, mesh.faces_num};
  AxisScaleParams params = evaluate_axis_scale_fields(evaluator, fields);

  Vector<ElementIsland> island = prepare_face_islands(mesh, params.selection);
  scale_vertex_islands_on_axis(mesh, island, params, get_face_verts);
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

static void scale_faces_uniformly(Mesh &mesh, const UniformScaleFields &fields)
{
  const bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_FACE};
  FieldEvaluator evaluator{field_context, mesh.faces_num};
  UniformScaleParams params = evaluate_uniform_scale_fields(evaluator, fields);

  Vector<ElementIsland> island = prepare_face_islands(mesh, params.selection);
  scale_vertex_islands_uniformly(mesh, island, params, get_face_verts);
}

static Vector<ElementIsland> prepare_edge_islands(const Mesh &mesh,
                                                  const IndexMask &edge_selection)
{
  const Span<int2> edges = mesh.edges();

  /* Use the disjoint set data structure to determine which vertices have to be scaled together. */
  DisjointSet<int> disjoint_set(mesh.totvert);
  edge_selection.foreach_index([&](const int edge_index) {
    const int2 &edge = edges[edge_index];
    disjoint_set.join(edge[0], edge[1]);
  });

  VectorSet<int> island_ids;
  Vector<ElementIsland> islands;
  /* There are at most as many islands as there are selected edges. */
  islands.reserve(edge_selection.size());

  /* Gather all of the edge indices in each island into separate vectors. */
  edge_selection.foreach_index([&](const int edge_index) {
    const int2 &edge = edges[edge_index];
    const int island_id = disjoint_set.find_root(edge[0]);
    const int island_index = island_ids.index_of_or_add(island_id);
    if (island_index == islands.size()) {
      islands.append_as();
    }
    ElementIsland &island = islands[island_index];
    island.element_indices.append(edge_index);
  });

  return islands;
}

static void get_edge_verts(const Span<int2> edges,
                           const OffsetIndices<int> /*faces*/,
                           const Span<int> /*corner_verts*/,
                           int edge_index,
                           VectorSet<int> &r_vertex_indices)
{
  const int2 &edge = edges[edge_index];
  r_vertex_indices.add(edge[0]);
  r_vertex_indices.add(edge[1]);
}

static void scale_edges_uniformly(Mesh &mesh, const UniformScaleFields &fields)
{
  const bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_EDGE};
  FieldEvaluator evaluator{field_context, mesh.totedge};
  UniformScaleParams params = evaluate_uniform_scale_fields(evaluator, fields);

  Vector<ElementIsland> island = prepare_edge_islands(mesh, params.selection);
  scale_vertex_islands_uniformly(mesh, island, params, get_edge_verts);
}

static void scale_edges_on_axis(Mesh &mesh, const AxisScaleFields &fields)
{
  const bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_EDGE};
  FieldEvaluator evaluator{field_context, mesh.totedge};
  AxisScaleParams params = evaluate_axis_scale_fields(evaluator, fields);

  Vector<ElementIsland> island = prepare_edge_islands(mesh, params.selection);
  scale_vertex_islands_on_axis(mesh, island, params, get_edge_verts);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const eAttrDomain domain = eAttrDomain(node.custom1);
  const GeometryNodeScaleElementsMode scale_mode = GeometryNodeScaleElementsMode(node.custom2);

  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");

  Field<bool> selection_field = params.get_input<Field<bool>>("Selection");
  Field<float> scale_field = params.get_input<Field<float>>("Scale");
  Field<float3> center_field = params.get_input<Field<float3>>("Center");
  Field<float3> axis_field;
  if (scale_mode == GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS) {
    axis_field = params.get_input<Field<float3>>("Axis");
  }

  geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      switch (domain) {
        case ATTR_DOMAIN_FACE: {
          switch (scale_mode) {
            case GEO_NODE_SCALE_ELEMENTS_UNIFORM: {
              scale_faces_uniformly(*mesh, {selection_field, scale_field, center_field});
              break;
            }
            case GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS: {
              scale_faces_on_axis(*mesh, {selection_field, scale_field, center_field, axis_field});
              break;
            }
          }
          break;
        }
        case ATTR_DOMAIN_EDGE: {
          switch (scale_mode) {
            case GEO_NODE_SCALE_ELEMENTS_UNIFORM: {
              scale_edges_uniformly(*mesh, {selection_field, scale_field, center_field});
              break;
            }
            case GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS: {
              scale_edges_on_axis(*mesh, {selection_field, scale_field, center_field, axis_field});
              break;
            }
          }
          break;
        }
        default:
          BLI_assert_unreachable();
          break;
      }
    }
  });

  params.set_output("Geometry", std::move(geometry));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem domain_items[] = {
      {ATTR_DOMAIN_FACE,
       "FACE",
       ICON_NONE,
       "Face",
       "Scale individual faces or neighboring face islands"},
      {ATTR_DOMAIN_EDGE,
       "EDGE",
       ICON_NONE,
       "Edge",
       "Scale individual edges or neighboring edge islands"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem scale_mode_items[] = {
      {GEO_NODE_SCALE_ELEMENTS_UNIFORM,
       "UNIFORM",
       ICON_NONE,
       "Uniform",
       "Scale elements by the same factor in every direction"},
      {GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS,
       "SINGLE_AXIS",
       ICON_NONE,
       "Single Axis",
       "Scale elements in a single direction"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Element type to transform",
                    domain_items,
                    NOD_inline_enum_accessors(custom1),
                    ATTR_DOMAIN_FACE);

  RNA_def_node_enum(
      srna, "scale_mode", "Scale Mode", "", scale_mode_items, NOD_inline_enum_accessors(custom2));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SCALE_ELEMENTS, "Scale Elements", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.updatefunc = node_update;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_scale_elements_cc
