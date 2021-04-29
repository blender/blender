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

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_point_translate_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Translation")},
    {SOCK_VECTOR, N_("Translation"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_point_translate_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_point_translate_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "input_type", 0, IFACE_("Type"), ICON_NONE);
}

namespace blender::nodes {

static void execute_on_component(GeoNodeExecParams params, GeometryComponent &component)
{
  OutputAttribute_Typed<float3> position_attribute =
      component.attribute_try_get_for_output<float3>("position", ATTR_DOMAIN_POINT, {0, 0, 0});
  if (!position_attribute) {
    return;
  }
  GVArray_Typed<float3> attribute = params.get_input_attribute<float3>(
      "Translation", component, ATTR_DOMAIN_POINT, {0, 0, 0});

  for (const int i : IndexRange(attribute.size())) {
    position_attribute->set(i, position_attribute->get(i) + attribute[i]);
  }

  position_attribute.save();
}

static void geo_node_point_translate_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    execute_on_component(params, geometry_set.get_component_for_write<MeshComponent>());
  }
  if (geometry_set.has<PointCloudComponent>()) {
    execute_on_component(params, geometry_set.get_component_for_write<PointCloudComponent>());
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void geo_node_point_translate_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryPointTranslate *data = (NodeGeometryPointTranslate *)MEM_callocN(
      sizeof(NodeGeometryPointTranslate), __func__);

  data->input_type = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;
  node->storage = data;
}

static void geo_node_point_translate_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryPointTranslate &node_storage = *(NodeGeometryPointTranslate *)node->storage;

  update_attribute_input_socket_availabilities(
      *node, "Translation", (GeometryNodeAttributeInputMode)node_storage.input_type);
}

}  // namespace blender::nodes

void register_node_type_geo_point_translate()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINT_TRANSLATE, "Point Translate", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_point_translate_in, geo_node_point_translate_out);
  node_type_init(&ntype, blender::nodes::geo_node_point_translate_init);
  node_type_update(&ntype, blender::nodes::geo_node_point_translate_update);
  node_type_storage(&ntype,
                    "NodeGeometryPointTranslate",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_point_translate_exec;
  ntype.draw_buttons = geo_node_point_translate_layout;
  nodeRegisterType(&ntype);
}
