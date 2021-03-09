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

#include "BKE_colorband.h"

#include "UI_interface.h"
#include "UI_resources.h"

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
  OutputAttributePtr position_attribute = component.attribute_try_get_for_output(
      "position", ATTR_DOMAIN_POINT, CD_PROP_FLOAT3);
  if (!position_attribute) {
    return;
  }
  ReadAttributePtr attribute = params.get_input_attribute(
      "Translation", component, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, nullptr);
  if (!attribute) {
    return;
  }

  Span<float3> data = attribute->get_span<float3>();
  MutableSpan<float3> scale_span = position_attribute->get_span<float3>();
  for (const int i : scale_span.index_range()) {
    scale_span[i] = scale_span[i] + data[i];
  }

  position_attribute.apply_span_and_save();
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

  data->input_type = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
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
