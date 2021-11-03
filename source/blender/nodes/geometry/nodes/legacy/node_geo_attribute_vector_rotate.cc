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

#include "node_geometry_util.hh"

#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes {

static void geo_node_attribute_vector_rotate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Vector"));
  b.add_input<decl::Vector>(N_("Vector"), "Vector_001").min(0.0f).max(1.0f).hide_value();
  b.add_input<decl::String>(N_("Center"));
  b.add_input<decl::Vector>(N_("Center"), "Center_001").subtype(PROP_XYZ);
  b.add_input<decl::String>(N_("Axis"));
  b.add_input<decl::Vector>(N_("Axis"), "Axis_001").min(-1.0f).max(1.0f).subtype(PROP_XYZ);
  b.add_input<decl::String>(N_("Angle"));
  b.add_input<decl::Float>(N_("Angle"), "Angle_001").subtype(PROP_ANGLE);
  b.add_input<decl::String>(N_("Rotation"));
  b.add_input<decl::Vector>(N_("Rotation"), "Rotation_001").subtype(PROP_EULER);
  b.add_input<decl::Bool>(N_("Invert"));
  b.add_input<decl::String>(N_("Result"));

  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void geo_node_attribute_vector_rotate_layout(uiLayout *layout,
                                                    bContext *UNUSED(C),
                                                    PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  const NodeAttributeVectorRotate &node_storage = *(NodeAttributeVectorRotate *)node->storage;
  const GeometryNodeAttributeVectorRotateMode mode = (const GeometryNodeAttributeVectorRotateMode)
                                                         node_storage.mode;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiLayout *column = uiLayoutColumn(layout, false);

  uiItemR(column, ptr, "rotation_mode", 0, "", ICON_NONE);

  uiItemR(column, ptr, "input_type_vector", 0, IFACE_("Vector"), ICON_NONE);
  uiItemR(column, ptr, "input_type_center", 0, IFACE_("Center"), ICON_NONE);
  if (mode == GEO_NODE_VECTOR_ROTATE_TYPE_AXIS) {
    uiItemR(column, ptr, "input_type_axis", 0, IFACE_("Axis"), ICON_NONE);
  }
  if (mode != GEO_NODE_VECTOR_ROTATE_TYPE_EULER_XYZ) {
    uiItemR(column, ptr, "input_type_angle", 0, IFACE_("Angle"), ICON_NONE);
  }
  if (mode == GEO_NODE_VECTOR_ROTATE_TYPE_EULER_XYZ) {
    uiItemR(column, ptr, "input_type_rotation", 0, IFACE_("Rotation"), ICON_NONE);
  }
}

static void geo_node_attribute_vector_rotate_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeAttributeVectorRotate *node_storage = (NodeAttributeVectorRotate *)node->storage;
  const GeometryNodeAttributeVectorRotateMode mode = (const GeometryNodeAttributeVectorRotateMode)
                                                         node_storage->mode;

  update_attribute_input_socket_availabilities(
      *node, "Vector", (GeometryNodeAttributeInputMode)node_storage->input_type_vector);
  update_attribute_input_socket_availabilities(
      *node, "Center", (GeometryNodeAttributeInputMode)node_storage->input_type_center);
  update_attribute_input_socket_availabilities(
      *node,
      "Axis",
      (GeometryNodeAttributeInputMode)node_storage->input_type_axis,
      (mode == GEO_NODE_VECTOR_ROTATE_TYPE_AXIS));
  update_attribute_input_socket_availabilities(
      *node,
      "Angle",
      (GeometryNodeAttributeInputMode)node_storage->input_type_angle,
      (mode != GEO_NODE_VECTOR_ROTATE_TYPE_EULER_XYZ));
  update_attribute_input_socket_availabilities(
      *node,
      "Rotation",
      (GeometryNodeAttributeInputMode)node_storage->input_type_rotation,
      (mode == GEO_NODE_VECTOR_ROTATE_TYPE_EULER_XYZ));
}

static float3 vector_rotate_around_axis(const float3 vector,
                                        const float3 center,
                                        const float3 axis,
                                        const float angle)
{
  float3 result = vector - center;
  float mat[3][3];
  axis_angle_to_mat3(mat, axis, angle);
  mul_m3_v3(mat, result);
  return result + center;
}

static void geo_node_attribute_vector_rotate_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeVectorRotate *node_storage = (NodeAttributeVectorRotate *)MEM_callocN(
      sizeof(NodeAttributeVectorRotate), __func__);

  node_storage->mode = GEO_NODE_VECTOR_ROTATE_TYPE_AXIS;
  node_storage->input_type_vector = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  node_storage->input_type_center = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;
  node_storage->input_type_axis = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;
  node_storage->input_type_angle = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  node_storage->input_type_rotation = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;

  node->storage = node_storage;
}

static float3 vector_rotate_euler(const float3 vector,
                                  const float3 center,
                                  const float3 rotation,
                                  const bool invert)
{
  float mat[3][3];
  float3 result = vector - center;
  eul_to_mat3(mat, rotation);
  if (invert) {
    invert_m3(mat);
  }
  mul_m3_v3(mat, result);
  return result + center;
}

static void do_vector_rotate_around_axis(const VArray<float3> &vector,
                                         const VArray<float3> &center,
                                         const VArray<float3> &axis,
                                         const VArray<float> &angle,
                                         MutableSpan<float3> results,
                                         const bool invert)
{
  VArray_Span<float3> span_vector{vector};
  VArray_Span<float3> span_center{center};
  VArray_Span<float3> span_axis{axis};
  VArray_Span<float> span_angle{angle};

  threading::parallel_for(IndexRange(results.size()), 1024, [&](IndexRange range) {
    for (const int i : range) {
      float angle = (invert) ? -span_angle[i] : span_angle[i];
      results[i] = vector_rotate_around_axis(span_vector[i], span_center[i], span_axis[i], angle);
    }
  });
}

static void do_vector_rotate_around_fixed_axis(const VArray<float3> &vector,
                                               const VArray<float3> &center,
                                               const float3 axis,
                                               const VArray<float> &angle,
                                               MutableSpan<float3> results,
                                               const bool invert)
{
  VArray_Span<float3> span_vector{vector};
  VArray_Span<float3> span_center{center};
  VArray_Span<float> span_angle{angle};

  threading::parallel_for(IndexRange(results.size()), 1024, [&](IndexRange range) {
    for (const int i : range) {
      float angle = (invert) ? -span_angle[i] : span_angle[i];
      results[i] = vector_rotate_around_axis(span_vector[i], span_center[i], axis, angle);
    }
  });
}

static void do_vector_rotate_euler(const VArray<float3> &vector,
                                   const VArray<float3> &center,
                                   const VArray<float3> &rotation,
                                   MutableSpan<float3> results,
                                   const bool invert)
{
  VArray_Span<float3> span_vector{vector};
  VArray_Span<float3> span_center{center};
  VArray_Span<float3> span_rotation{rotation};

  threading::parallel_for(IndexRange(results.size()), 1024, [&](IndexRange range) {
    for (const int i : range) {
      results[i] = vector_rotate_euler(span_vector[i], span_center[i], span_rotation[i], invert);
    }
  });
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         const GeoNodeExecParams &params,
                                         StringRef result_name)
{
  /* Use the domain of the result attribute if it already exists. */
  std::optional<AttributeMetaData> meta_data = component.attribute_get_meta_data(result_name);
  if (meta_data) {
    return meta_data->domain;
  }

  /* Otherwise use the highest priority domain from existing input attributes, or the default. */
  const AttributeDomain default_domain = ATTR_DOMAIN_POINT;
  return params.get_highest_priority_input_domain({"Vector", "Center"}, component, default_domain);
}

static void execute_on_component(const GeoNodeExecParams &params, GeometryComponent &component)
{
  const bNode &node = params.node();
  const NodeAttributeVectorRotate *node_storage = (const NodeAttributeVectorRotate *)node.storage;
  const GeometryNodeAttributeVectorRotateMode mode = (GeometryNodeAttributeVectorRotateMode)
                                                         node_storage->mode;
  const std::string result_name = params.get_input<std::string>("Result");
  const AttributeDomain result_domain = get_result_domain(component, params, result_name);
  const bool invert = params.get_input<bool>("Invert");

  GVArrayPtr attribute_vector = params.get_input_attribute(
      "Vector", component, result_domain, CD_PROP_FLOAT3, nullptr);
  if (!attribute_vector) {
    return;
  }
  GVArrayPtr attribute_center = params.get_input_attribute(
      "Center", component, result_domain, CD_PROP_FLOAT3, nullptr);
  if (!attribute_center) {
    return;
  }

  OutputAttribute attribute_result = component.attribute_try_get_for_output_only(
      result_name, result_domain, CD_PROP_FLOAT3);
  if (!attribute_result) {
    return;
  }

  if (mode == GEO_NODE_VECTOR_ROTATE_TYPE_EULER_XYZ) {
    GVArrayPtr attribute_rotation = params.get_input_attribute(
        "Rotation", component, result_domain, CD_PROP_FLOAT3, nullptr);
    if (!attribute_rotation) {
      return;
    }
    do_vector_rotate_euler(attribute_vector->typed<float3>(),
                           attribute_center->typed<float3>(),
                           attribute_rotation->typed<float3>(),
                           attribute_result.as_span<float3>(),
                           invert);
    attribute_result.save();
    return;
  }

  GVArrayPtr attribute_angle = params.get_input_attribute(
      "Angle", component, result_domain, CD_PROP_FLOAT, nullptr);
  if (!attribute_angle) {
    return;
  }

  switch (mode) {
    case GEO_NODE_VECTOR_ROTATE_TYPE_AXIS: {
      GVArrayPtr attribute_axis = params.get_input_attribute(
          "Axis", component, result_domain, CD_PROP_FLOAT3, nullptr);
      if (!attribute_axis) {
        return;
      }
      do_vector_rotate_around_axis(attribute_vector->typed<float3>(),
                                   attribute_center->typed<float3>(),
                                   attribute_axis->typed<float3>(),
                                   attribute_angle->typed<float>(),
                                   attribute_result.as_span<float3>(),
                                   invert);
    } break;
    case GEO_NODE_VECTOR_ROTATE_TYPE_AXIS_X:
      do_vector_rotate_around_fixed_axis(attribute_vector->typed<float3>(),
                                         attribute_center->typed<float3>(),
                                         float3(1.0f, 0.0f, 0.0f),
                                         attribute_angle->typed<float>(),
                                         attribute_result.as_span<float3>(),
                                         invert);
      break;
    case GEO_NODE_VECTOR_ROTATE_TYPE_AXIS_Y:
      do_vector_rotate_around_fixed_axis(attribute_vector->typed<float3>(),
                                         attribute_center->typed<float3>(),
                                         float3(0.0f, 1.0f, 0.0f),
                                         attribute_angle->typed<float>(),
                                         attribute_result.as_span<float3>(),
                                         invert);

      break;
    case GEO_NODE_VECTOR_ROTATE_TYPE_AXIS_Z:
      do_vector_rotate_around_fixed_axis(attribute_vector->typed<float3>(),
                                         attribute_center->typed<float3>(),
                                         float3(0.0f, 0.0f, 1.0f),
                                         attribute_angle->typed<float>(),
                                         attribute_result.as_span<float3>(),
                                         invert);

      break;
    case GEO_NODE_VECTOR_ROTATE_TYPE_EULER_XYZ:
      /* Euler is handled before other modes to avoid processing the unavailable angle socket. */
      BLI_assert_unreachable();
      break;
  }
  attribute_result.save();
}

static void geo_node_attribute_vector_rotate_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    execute_on_component(params, geometry_set.get_component_for_write<MeshComponent>());
  }
  if (geometry_set.has<PointCloudComponent>()) {
    execute_on_component(params, geometry_set.get_component_for_write<PointCloudComponent>());
  }
  if (geometry_set.has<CurveComponent>()) {
    execute_on_component(params, geometry_set.get_component_for_write<CurveComponent>());
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_vector_rotate()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_LEGACY_ATTRIBUTE_VECTOR_ROTATE,
                     "Attribute Vector Rotate",
                     NODE_CLASS_ATTRIBUTE,
                     0);
  node_type_update(&ntype, blender::nodes::geo_node_attribute_vector_rotate_update);
  node_type_init(&ntype, blender::nodes::geo_node_attribute_vector_rotate_init);
  node_type_size(&ntype, 165, 100, 600);
  node_type_storage(
      &ntype, "NodeAttributeVectorRotate", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_vector_rotate_exec;
  ntype.draw_buttons = blender::nodes::geo_node_attribute_vector_rotate_layout;
  ntype.declare = blender::nodes::geo_node_attribute_vector_rotate_declare;
  nodeRegisterType(&ntype);
}
