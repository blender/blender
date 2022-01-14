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

#include "BLI_task.hh"

#include "BKE_colorband.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_legacy_attribute_color_ramp_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Attribute"));
  b.add_input<decl::String>(N_("Result"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiTemplateColorRamp(layout, ptr, "color_ramp", false);
}

static void node_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeColorRamp *node_storage = MEM_cnew<NodeAttributeColorRamp>(__func__);
  BKE_colorband_init(&node_storage->color_ramp, true);
  node->storage = node_storage;
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         StringRef input_name,
                                         StringRef result_name)
{
  /* Use the domain of the result attribute if it already exists. */
  std::optional<AttributeMetaData> result_info = component.attribute_get_meta_data(result_name);
  if (result_info) {
    return result_info->domain;
  }

  /* Otherwise use the input attribute's domain if it exists. */
  std::optional<AttributeMetaData> source_info = component.attribute_get_meta_data(input_name);
  if (source_info) {
    return source_info->domain;
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
  const AttributeDomain result_domain = get_result_domain(component, input_name, result_name);

  OutputAttribute_Typed<ColorGeometry4f> attribute_result =
      component.attribute_try_get_for_output_only<ColorGeometry4f>(result_name, result_domain);
  if (!attribute_result) {
    return;
  }

  VArray<float> attribute_in = component.attribute_get_for_read<float>(
      input_name, result_domain, 0.0f);

  MutableSpan<ColorGeometry4f> results = attribute_result.as_span();

  ColorBand *color_ramp = &node_storage->color_ramp;
  threading::parallel_for(IndexRange(attribute_in.size()), 512, [&](IndexRange range) {
    for (const int i : range) {
      BKE_colorband_evaluate(color_ramp, attribute_in[i], results[i]);
    }
  });

  attribute_result.save();
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

}  // namespace blender::nodes::node_geo_legacy_attribute_color_ramp_cc

void register_node_type_geo_attribute_color_ramp()
{
  namespace file_ns = blender::nodes::node_geo_legacy_attribute_color_ramp_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_ATTRIBUTE_COLOR_RAMP, "Attribute Color Ramp", NODE_CLASS_ATTRIBUTE);
  node_type_storage(
      &ntype, "NodeAttributeColorRamp", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, file_ns::node_init);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
