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

#include "BLI_compiler_attrs.h"

#include "DNA_texture_types.h"

#include "BKE_texture.h"

#include "RE_texture.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_attribute_sample_texture_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Mapping")},
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_sample_texture_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_attribute_sample_texture_layout(uiLayout *layout,
                                                     bContext *C,
                                                     PointerRNA *ptr)
{
  uiTemplateID(layout, C, ptr, "texture", "texture.new", nullptr, nullptr, 0, ICON_NONE, nullptr);
}

namespace blender::nodes {

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         StringRef result_attribute_name,
                                         StringRef map_attribute_name)
{
  /* Use the domain of the result attribute if it already exists. */
  ReadAttributePtr result_attribute = component.attribute_try_get_for_read(result_attribute_name);
  if (result_attribute) {
    return result_attribute->domain();
  }

  /* Otherwise use the name of the map attribute. */
  ReadAttributePtr map_attribute = component.attribute_try_get_for_read(map_attribute_name);
  if (map_attribute) {
    return map_attribute->domain();
  }

  /* The node won't execute in this case, but we still have to return a value. */
  return ATTR_DOMAIN_POINT;
}

static void execute_on_component(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  Tex *texture = reinterpret_cast<Tex *>(node.id);
  if (texture == nullptr) {
    return;
  }

  const std::string result_attribute_name = params.get_input<std::string>("Result");
  const std::string mapping_name = params.get_input<std::string>("Mapping");
  if (!component.attribute_exists(mapping_name)) {
    return;
  }

  const AttributeDomain result_domain = get_result_domain(
      component, result_attribute_name, mapping_name);

  OutputAttributePtr attribute_out = component.attribute_try_get_for_output(
      result_attribute_name, result_domain, CD_PROP_COLOR);
  if (!attribute_out) {
    return;
  }

  Float3ReadAttribute mapping_attribute = component.attribute_get_for_read<float3>(
      mapping_name, result_domain, {0, 0, 0});

  MutableSpan<Color4f> colors = attribute_out->get_span<Color4f>();
  for (const int i : IndexRange(mapping_attribute.size())) {
    TexResult texture_result = {0};
    const float3 position = mapping_attribute[i];
    /* For legacy reasons we have to map [0, 1] to [-1, 1] to support uv mappings. */
    const float3 remapped_position = position * 2.0f - float3(1.0f);
    BKE_texture_get_value(nullptr, texture, remapped_position, &texture_result, false);
    colors[i] = {texture_result.tr, texture_result.tg, texture_result.tb, texture_result.ta};
  }
  attribute_out.apply_span_and_save();
}

static void geo_node_attribute_sample_texture_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    execute_on_component(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    execute_on_component(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_sample_texture()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_ATTRIBUTE_SAMPLE_TEXTURE,
                     "Attribute Sample Texture",
                     NODE_CLASS_ATTRIBUTE,
                     0);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_socket_templates(
      &ntype, geo_node_attribute_sample_texture_in, geo_node_attribute_sample_texture_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_sample_texture_exec;
  ntype.draw_buttons = geo_node_attribute_sample_texture_layout;
  nodeRegisterType(&ntype);
}
