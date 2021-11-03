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

#include "BKE_colorband.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_point_scale_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Factor"));
  b.add_input<decl::Vector>(N_("Factor"), "Factor_001")
      .default_value({1.0f, 1.0f, 1.0f})
      .subtype(PROP_XYZ);
  b.add_input<decl::Float>(N_("Factor"), "Factor_002").default_value(1.0f).min(0.0f);
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void geo_node_point_scale_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "input_type", 0, IFACE_("Type"), ICON_NONE);
}

static void geo_node_point_scale_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryPointScale *data = (NodeGeometryPointScale *)MEM_callocN(
      sizeof(NodeGeometryPointScale), __func__);

  data->input_type = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;
  node->storage = data;
}

static void geo_node_point_scale_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryPointScale &node_storage = *(NodeGeometryPointScale *)node->storage;

  update_attribute_input_socket_availabilities(
      *node, "Factor", (GeometryNodeAttributeInputMode)node_storage.input_type);
}

static void execute_on_component(GeoNodeExecParams params, GeometryComponent &component)
{
  /* Note that scale doesn't necessarily need to be created with a vector type-- it could also use
   * the highest complexity of the existing attribute's type (if it exists) and the data type used
   * for the factor. But for it's simpler to simply always use float3, since that is usually
   * expected anyway. */
  static const float3 scale_default = float3(1.0f);
  OutputAttribute_Typed<float3> scale_attribute = component.attribute_try_get_for_output(
      "scale", ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, &scale_default);
  if (!scale_attribute) {
    return;
  }

  const bNode &node = params.node();
  const NodeGeometryPointScale &node_storage = *(const NodeGeometryPointScale *)node.storage;
  const GeometryNodeAttributeInputMode input_type = (GeometryNodeAttributeInputMode)
                                                        node_storage.input_type;
  const CustomDataType data_type = (input_type == GEO_NODE_ATTRIBUTE_INPUT_FLOAT) ? CD_PROP_FLOAT :
                                                                                    CD_PROP_FLOAT3;

  GVArrayPtr attribute = params.get_input_attribute(
      "Factor", component, ATTR_DOMAIN_POINT, data_type, nullptr);
  if (!attribute) {
    return;
  }

  MutableSpan<float3> scale_span = scale_attribute.as_span();
  if (data_type == CD_PROP_FLOAT) {
    GVArray_Typed<float> factors{*attribute};
    for (const int i : scale_span.index_range()) {
      scale_span[i] = scale_span[i] * factors[i];
    }
  }
  else if (data_type == CD_PROP_FLOAT3) {
    GVArray_Typed<float3> factors{*attribute};
    for (const int i : scale_span.index_range()) {
      scale_span[i] = scale_span[i] * factors[i];
    }
  }

  scale_attribute.save();
}

static void geo_node_point_scale_exec(GeoNodeExecParams params)
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

void register_node_type_geo_point_scale()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_LEGACY_POINT_SCALE, "Point Scale", NODE_CLASS_GEOMETRY, 0);

  ntype.declare = blender::nodes::geo_node_point_scale_declare;
  node_type_init(&ntype, blender::nodes::geo_node_point_scale_init);
  node_type_update(&ntype, blender::nodes::geo_node_point_scale_update);
  node_type_storage(
      &ntype, "NodeGeometryPointScale", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_point_scale_exec;
  ntype.draw_buttons = blender::nodes::geo_node_point_scale_layout;
  nodeRegisterType(&ntype);
}
