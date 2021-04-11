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

static bNodeSocketTemplate geo_node_attribute_color_ramp_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Attribute")},
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_color_ramp_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_attribute_color_ramp_layout(uiLayout *layout,
                                                 bContext *UNUSED(C),
                                                 PointerRNA *ptr)
{
  uiTemplateColorRamp(layout, ptr, "color_ramp", false);
}

namespace blender::nodes {

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         StringRef input_name,
                                         StringRef result_name)
{
  /* Use the domain of the result attribute if it already exists. */
  ReadAttributePtr result_attribute = component.attribute_try_get_for_read(result_name);
  if (result_attribute) {
    return result_attribute->domain();
  }

  /* Otherwise use the input attribute's domain if it exists. */
  ReadAttributePtr input_attribute = component.attribute_try_get_for_read(input_name);
  if (input_attribute) {
    return input_attribute->domain();
  }

  return ATTR_DOMAIN_POINT;
}

static void execute_on_component(const GeoNodeExecParams &params, GeometryComponent &component)
{
  const bNode &bnode = params.node();
  NodeAttributeColorRamp *node_storage = (NodeAttributeColorRamp *)bnode.storage;
  const std::string result_name = params.get_input<std::string>("Result");
  const std::string input_name = params.get_input<std::string>("Attribute");

  /* Always output a color attribute for now. We might want to allow users to customize.
   * Using the type of an existing attribute could work, but does not have a real benefit
   * currently. */
  const CustomDataType result_type = CD_PROP_COLOR;
  const AttributeDomain result_domain = get_result_domain(component, input_name, result_name);

  OutputAttributePtr attribute_result = component.attribute_try_get_for_output(
      result_name, result_domain, result_type);
  if (!attribute_result) {
    return;
  }

  FloatReadAttribute attribute_in = component.attribute_get_for_read<float>(
      input_name, result_domain, 0.0f);

  Span<float> data_in = attribute_in.get_span();
  MutableSpan<Color4f> data_out = attribute_result->get_span_for_write_only<Color4f>();

  ColorBand *color_ramp = &node_storage->color_ramp;
  for (const int i : data_in.index_range()) {
    BKE_colorband_evaluate(color_ramp, data_in[i], data_out[i]);
  }

  attribute_result.apply_span_and_save();
}

static void geo_node_attribute_color_ramp_exec(GeoNodeExecParams params)
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

static void geo_node_attribute_color_ramp_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeColorRamp *node_storage = (NodeAttributeColorRamp *)MEM_callocN(
      sizeof(NodeAttributeColorRamp), __func__);
  BKE_colorband_init(&node_storage->color_ramp, true);
  node->storage = node_storage;
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_color_ramp()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_COLOR_RAMP, "Attribute Color Ramp", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(
      &ntype, geo_node_attribute_color_ramp_in, geo_node_attribute_color_ramp_out);
  node_type_storage(
      &ntype, "NodeAttributeColorRamp", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, blender::nodes::geo_node_attribute_color_ramp_init);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_color_ramp_exec;
  ntype.draw_buttons = geo_node_attribute_color_ramp_layout;
  nodeRegisterType(&ntype);
}
