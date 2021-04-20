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

static bNodeSocketTemplate geo_node_attribute_convert_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Attribute")},
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_convert_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_attribute_convert_layout(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
}

static void geo_node_attribute_convert_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeAttributeConvert *data = (NodeAttributeConvert *)MEM_callocN(sizeof(NodeAttributeConvert),
                                                                   __func__);

  data->data_type = CD_PROP_FLOAT;
  data->domain = ATTR_DOMAIN_AUTO;
  node->storage = data;
}

namespace blender::nodes {

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         StringRef source_name,
                                         StringRef result_name)
{
  ReadAttributePtr result_attribute = component.attribute_try_get_for_read(result_name);
  if (result_attribute) {
    return result_attribute->domain();
  }
  ReadAttributePtr source_attribute = component.attribute_try_get_for_read(source_name);
  if (source_attribute) {
    return source_attribute->domain();
  }
  return ATTR_DOMAIN_POINT;
}

static bool conversion_can_be_skipped(const GeometryComponent &component,
                                      const StringRef source_name,
                                      const StringRef result_name,
                                      const AttributeDomain result_domain,
                                      const CustomDataType result_type)
{
  if (source_name != result_name) {
    return false;
  }
  ReadAttributePtr read_attribute = component.attribute_try_get_for_read(source_name);
  if (!read_attribute) {
    return false;
  }
  if (read_attribute->domain() != result_domain) {
    return false;
  }
  if (read_attribute->cpp_type() != *bke::custom_data_type_to_cpp_type(result_type)) {
    return false;
  }
  return true;
}

static void attribute_convert_calc(GeometryComponent &component,
                                   const GeoNodeExecParams &params,
                                   const StringRef source_name,
                                   const StringRef result_name,
                                   const CustomDataType result_type,
                                   const AttributeDomain domain)
{
  const AttributeDomain result_domain = (domain == ATTR_DOMAIN_AUTO) ?
                                            get_result_domain(
                                                component, source_name, result_name) :
                                            domain;

  if (conversion_can_be_skipped(component, source_name, result_name, result_domain, result_type)) {
    return;
  }

  ReadAttributePtr source_attribute = component.attribute_try_get_for_read(
      source_name, result_domain, result_type);
  if (!source_attribute) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("No attribute with name \"") + source_name + "\"");
    return;
  }

  OutputAttributePtr result_attribute = component.attribute_try_get_for_output(
      result_name, result_domain, result_type);
  if (!result_attribute) {
    return;
  }

  fn::GSpan source_span = source_attribute->get_span();
  fn::GMutableSpan result_span = result_attribute->get_span_for_write_only();
  if (source_span.is_empty() || result_span.is_empty()) {
    return;
  }
  BLI_assert(source_span.size() == result_span.size());

  const CPPType *cpp_type = bke::custom_data_type_to_cpp_type(result_type);
  BLI_assert(cpp_type != nullptr);

  cpp_type->copy_to_initialized_n(source_span.data(), result_span.data(), result_span.size());

  result_attribute.apply_span_and_save();
}

static void geo_node_attribute_convert_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  const std::string result_name = params.extract_input<std::string>("Result");
  const std::string source_name = params.extract_input<std::string>("Attribute");
  const NodeAttributeConvert &node_storage = *(const NodeAttributeConvert *)params.node().storage;
  const CustomDataType data_type = static_cast<CustomDataType>(node_storage.data_type);
  const AttributeDomain domain = static_cast<AttributeDomain>(node_storage.domain);

  if (result_name.empty()) {
    params.set_output("Geometry", geometry_set);
    return;
  }

  if (geometry_set.has<MeshComponent>()) {
    attribute_convert_calc(geometry_set.get_component_for_write<MeshComponent>(),
                           params,
                           source_name,
                           result_name,
                           data_type,
                           domain);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    attribute_convert_calc(geometry_set.get_component_for_write<PointCloudComponent>(),
                           params,
                           source_name,
                           result_name,
                           data_type,
                           domain);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_convert()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_CONVERT, "Attribute Convert", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(
      &ntype, geo_node_attribute_convert_in, geo_node_attribute_convert_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_convert_exec;
  ntype.draw_buttons = geo_node_attribute_convert_layout;
  node_type_init(&ntype, geo_node_attribute_convert_init);
  node_type_storage(
      &ntype, "NodeAttributeConvert", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
