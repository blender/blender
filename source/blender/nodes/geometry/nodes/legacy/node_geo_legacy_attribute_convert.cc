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

namespace blender::nodes::node_geo_legacy_attribute_convert_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Attribute"));
  b.add_input<decl::String>(N_("Result"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "domain", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "data_type", 0, IFACE_("Type"), ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeAttributeConvert *data = MEM_cnew<NodeAttributeConvert>(__func__);

  data->data_type = CD_AUTO_FROM_NAME;
  data->domain = ATTR_DOMAIN_AUTO;
  node->storage = data;
}

static AttributeMetaData get_result_domain_and_type(const GeometryComponent &component,
                                                    const StringRef source_name,
                                                    const StringRef result_name)
{
  std::optional<AttributeMetaData> result_info = component.attribute_get_meta_data(result_name);
  if (result_info) {
    return *result_info;
  }
  std::optional<AttributeMetaData> source_info = component.attribute_get_meta_data(source_name);
  if (source_info) {
    return *source_info;
  }
  /* The node won't do anything in this case, but we still have to return a value. */
  return AttributeMetaData{ATTR_DOMAIN_POINT, CD_PROP_BOOL};
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
  std::optional<AttributeMetaData> info = component.attribute_get_meta_data(result_name);
  if (!info) {
    return false;
  }
  if (info->domain != result_domain) {
    return false;
  }
  if (info->data_type != result_type) {
    return false;
  }
  return true;
}

static void attribute_convert_calc(GeometryComponent &component,
                                   const GeoNodeExecParams &params,
                                   const StringRef source_name,
                                   const StringRef result_name,
                                   const CustomDataType data_type,
                                   const AttributeDomain domain)
{
  const AttributeMetaData auto_info = get_result_domain_and_type(
      component, source_name, result_name);
  const AttributeDomain result_domain = (domain == ATTR_DOMAIN_AUTO) ? auto_info.domain : domain;
  const CustomDataType result_type = (data_type == CD_AUTO_FROM_NAME) ? auto_info.data_type :
                                                                        data_type;

  if (conversion_can_be_skipped(component, source_name, result_name, result_domain, result_type)) {
    return;
  }

  GVArray source_attribute = component.attribute_try_get_for_read(
      source_name, result_domain, result_type);
  if (!source_attribute) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("No attribute with name \"") + source_name + "\"");
    return;
  }

  OutputAttribute result_attribute = component.attribute_try_get_for_output_only(
      result_name, result_domain, result_type);
  if (!result_attribute) {
    return;
  }

  GVArray_GSpan source_span{source_attribute};
  GMutableSpan result_span = result_attribute.as_span();

  BLI_assert(source_span.size() == result_span.size());

  const CPPType *cpp_type = bke::custom_data_type_to_cpp_type(result_type);
  BLI_assert(cpp_type != nullptr);

  cpp_type->copy_assign_n(source_span.data(), result_span.data(), result_span.size());
  result_attribute.save();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry::realize_instances_legacy(geometry_set);

  const std::string result_name = params.extract_input<std::string>("Result");
  const std::string source_name = params.extract_input<std::string>("Attribute");
  const NodeAttributeConvert &node_storage = *(const NodeAttributeConvert *)params.node().storage;
  const CustomDataType data_type = static_cast<CustomDataType>(node_storage.data_type);
  const AttributeDomain domain = static_cast<AttributeDomain>(node_storage.domain);

  if (result_name.empty()) {
    params.set_default_remaining_outputs();
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
  if (geometry_set.has<CurveComponent>()) {
    attribute_convert_calc(geometry_set.get_component_for_write<CurveComponent>(),
                           params,
                           source_name,
                           result_name,
                           data_type,
                           domain);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes::node_geo_legacy_attribute_convert_cc

void register_node_type_geo_attribute_convert()
{
  namespace file_ns = blender::nodes::node_geo_legacy_attribute_convert_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_ATTRIBUTE_CONVERT, "Attribute Convert", NODE_CLASS_ATTRIBUTE);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(
      &ntype, "NodeAttributeConvert", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
