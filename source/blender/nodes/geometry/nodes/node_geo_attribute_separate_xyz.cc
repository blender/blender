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

static bNodeSocketTemplate geo_node_attribute_separate_xyz_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Vector")},
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_STRING, N_("Result X")},
    {SOCK_STRING, N_("Result Y")},
    {SOCK_STRING, N_("Result Z")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_separate_xyz_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_attribute_separate_xyz_layout(uiLayout *layout,
                                                   bContext *UNUSED(C),
                                                   PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "input_type", 0, IFACE_("Type"), ICON_NONE);
}

namespace blender::nodes {

static void geo_node_attribute_separate_xyz_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeAttributeSeparateXYZ *data = (NodeAttributeSeparateXYZ *)MEM_callocN(
      sizeof(NodeAttributeSeparateXYZ), __func__);
  data->input_type = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  node->storage = data;
}

static void geo_node_attribute_separate_xyz_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeSeparateXYZ *node_storage = (NodeAttributeSeparateXYZ *)node->storage;
  update_attribute_input_socket_availabilities(
      *node, "Vector", (GeometryNodeAttributeInputMode)node_storage->input_type);
}

static void extract_input(const int index, const Span<float3> &input, MutableSpan<float> result)
{
  for (const int i : result.index_range()) {
    /* Get the component of the float3. (0: X, 1: Y, 2: Z). */
    const float component = input[i][index];
    result[i] = component;
  }
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         const GeoNodeExecParams &params,
                                         const StringRef name_x,
                                         const StringRef name_y,
                                         const StringRef name_z)
{
  /* Use the highest priority domain from any existing attribute outputs. */
  Vector<AttributeDomain, 3> output_domains;
  std::optional<AttributeMetaData> info_x = component.attribute_get_meta_data(name_x);
  std::optional<AttributeMetaData> info_y = component.attribute_get_meta_data(name_y);
  std::optional<AttributeMetaData> info_z = component.attribute_get_meta_data(name_z);
  if (info_x) {
    output_domains.append(info_x->domain);
  }
  if (info_y) {
    output_domains.append(info_y->domain);
  }
  if (info_z) {
    output_domains.append(info_z->domain);
  }
  if (output_domains.size() > 0) {
    return bke::attribute_domain_highest_priority(output_domains);
  }

  /* Otherwise use the domain of the input attribute, or the default. */
  return params.get_highest_priority_input_domain({"Vector"}, component, ATTR_DOMAIN_POINT);
}

static void separate_attribute(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const std::string result_name_x = params.get_input<std::string>("Result X");
  const std::string result_name_y = params.get_input<std::string>("Result Y");
  const std::string result_name_z = params.get_input<std::string>("Result Z");
  if (result_name_x.empty() && result_name_y.empty() && result_name_z.empty()) {
    return;
  }

  /* The node is only for float3 to float conversions. */
  const AttributeDomain result_domain = get_result_domain(
      component, params, result_name_x, result_name_y, result_name_z);

  GVArray_Typed<float3> attribute_input = params.get_input_attribute<float3>(
      "Vector", component, result_domain, {0, 0, 0});
  VArray_Span<float3> input_span{*attribute_input};

  OutputAttribute_Typed<float> attribute_result_x =
      component.attribute_try_get_for_output_only<float>(result_name_x, result_domain);
  OutputAttribute_Typed<float> attribute_result_y =
      component.attribute_try_get_for_output_only<float>(result_name_y, result_domain);
  OutputAttribute_Typed<float> attribute_result_z =
      component.attribute_try_get_for_output_only<float>(result_name_z, result_domain);

  /* Only extract the components for the outputs with a given attribute. */
  if (attribute_result_x) {
    extract_input(0, input_span, attribute_result_x.as_span());
    attribute_result_x.save();
  }
  if (attribute_result_y) {
    extract_input(1, input_span, attribute_result_y.as_span());
    attribute_result_y.save();
  }
  if (attribute_result_z) {
    extract_input(2, input_span, attribute_result_z.as_span());
    attribute_result_z.save();
  }
}

static void geo_node_attribute_separate_xyz_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    separate_attribute(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    separate_attribute(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_separate_xyz()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_SEPARATE_XYZ, "Attribute Separate XYZ", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(
      &ntype, geo_node_attribute_separate_xyz_in, geo_node_attribute_separate_xyz_out);
  node_type_init(&ntype, blender::nodes::geo_node_attribute_separate_xyz_init);
  node_type_update(&ntype, blender::nodes::geo_node_attribute_separate_xyz_update);
  node_type_storage(
      &ntype, "NodeAttributeSeparateXYZ", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_separate_xyz_exec;
  ntype.draw_buttons = geo_node_attribute_separate_xyz_layout;
  nodeRegisterType(&ntype);
}
