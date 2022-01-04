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

namespace blender::nodes::node_geo_legacy_point_translate_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Translation"));
  b.add_input<decl::Vector>(N_("Translation"), "Translation_001").subtype(PROP_TRANSLATION);
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "input_type", 0, IFACE_("Type"), ICON_NONE);
}

static void execute_on_component(GeoNodeExecParams params, GeometryComponent &component)
{
  OutputAttribute_Typed<float3> position_attribute =
      component.attribute_try_get_for_output<float3>("position", ATTR_DOMAIN_POINT, {0, 0, 0});
  if (!position_attribute) {
    return;
  }
  VArray<float3> attribute = params.get_input_attribute<float3>(
      "Translation", component, ATTR_DOMAIN_POINT, {0, 0, 0});

  for (const int i : attribute.index_range()) {
    position_attribute->set(i, position_attribute->get(i) + attribute[i]);
  }

  position_attribute.save();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry::realize_instances_legacy(geometry_set);

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

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryPointTranslate *data = MEM_cnew<NodeGeometryPointTranslate>(__func__);

  data->input_type = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  NodeGeometryPointTranslate &node_storage = *(NodeGeometryPointTranslate *)node->storage;

  update_attribute_input_socket_availabilities(
      *ntree, *node, "Translation", (GeometryNodeAttributeInputMode)node_storage.input_type);
}

}  // namespace blender::nodes::node_geo_legacy_point_translate_cc

void register_node_type_geo_point_translate()
{
  namespace file_ns = blender::nodes::node_geo_legacy_point_translate_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_POINT_TRANSLATE, "Point Translate", NODE_CLASS_GEOMETRY);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  node_type_storage(&ntype,
                    "NodeGeometryPointTranslate",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
