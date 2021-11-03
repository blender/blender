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

#include "BKE_attribute_math.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_attribute_capture_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Vector>(N_("Value")).supports_field();
  b.add_input<decl::Float>(N_("Value"), "Value_001").supports_field();
  b.add_input<decl::Color>(N_("Value"), "Value_002").supports_field();
  b.add_input<decl::Bool>(N_("Value"), "Value_003").supports_field();
  b.add_input<decl::Int>(N_("Value"), "Value_004").supports_field();

  b.add_output<decl::Geometry>(N_("Geometry"));
  b.add_output<decl::Vector>(N_("Attribute")).field_source();
  b.add_output<decl::Float>(N_("Attribute"), "Attribute_001").field_source();
  b.add_output<decl::Color>(N_("Attribute"), "Attribute_002").field_source();
  b.add_output<decl::Bool>(N_("Attribute"), "Attribute_003").field_source();
  b.add_output<decl::Int>(N_("Attribute"), "Attribute_004").field_source();
}

static void geo_node_attribute_capture_layout(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
}

static void geo_node_attribute_capture_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryAttributeCapture *data = (NodeGeometryAttributeCapture *)MEM_callocN(
      sizeof(NodeGeometryAttributeCapture), __func__);
  data->data_type = CD_PROP_FLOAT;
  data->domain = ATTR_DOMAIN_POINT;

  node->storage = data;
}

static void geo_node_attribute_capture_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeGeometryAttributeCapture &storage = *(const NodeGeometryAttributeCapture *)
                                                     node->storage;
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);

  bNodeSocket *socket_value_geometry = (bNodeSocket *)node->inputs.first;
  bNodeSocket *socket_value_vector = socket_value_geometry->next;
  bNodeSocket *socket_value_float = socket_value_vector->next;
  bNodeSocket *socket_value_color4f = socket_value_float->next;
  bNodeSocket *socket_value_boolean = socket_value_color4f->next;
  bNodeSocket *socket_value_int32 = socket_value_boolean->next;

  nodeSetSocketAvailability(socket_value_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_value_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_value_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(socket_value_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(socket_value_int32, data_type == CD_PROP_INT32);

  bNodeSocket *out_socket_value_geometry = (bNodeSocket *)node->outputs.first;
  bNodeSocket *out_socket_value_vector = out_socket_value_geometry->next;
  bNodeSocket *out_socket_value_float = out_socket_value_vector->next;
  bNodeSocket *out_socket_value_color4f = out_socket_value_float->next;
  bNodeSocket *out_socket_value_boolean = out_socket_value_color4f->next;
  bNodeSocket *out_socket_value_int32 = out_socket_value_boolean->next;

  nodeSetSocketAvailability(out_socket_value_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(out_socket_value_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(out_socket_value_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(out_socket_value_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(out_socket_value_int32, data_type == CD_PROP_INT32);
}

static void try_capture_field_on_geometry(GeometryComponent &component,
                                          const AttributeIDRef &attribute_id,
                                          const AttributeDomain domain,
                                          const GField &field)
{
  GeometryComponentFieldContext field_context{component, domain};
  const int domain_size = component.attribute_domain_size(domain);
  const IndexMask mask{IndexMask(domain_size)};

  const CustomDataType data_type = bke::cpp_type_to_custom_data_type(field.cpp_type());
  OutputAttribute output_attribute = component.attribute_try_get_for_output_only(
      attribute_id, domain, data_type);

  fn::FieldEvaluator evaluator{field_context, &mask};
  evaluator.add_with_destination(field, output_attribute.varray());
  evaluator.evaluate();

  output_attribute.save();
}

static void geo_node_attribute_capture_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const bNode &node = params.node();
  const NodeGeometryAttributeCapture &storage = *(const NodeGeometryAttributeCapture *)
                                                     node.storage;
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);
  const AttributeDomain domain = static_cast<AttributeDomain>(storage.domain);

  GField field;
  switch (data_type) {
    case CD_PROP_FLOAT:
      field = params.get_input<Field<float>>("Value_001");
      break;
    case CD_PROP_FLOAT3:
      field = params.get_input<Field<float3>>("Value");
      break;
    case CD_PROP_COLOR:
      field = params.get_input<Field<ColorGeometry4f>>("Value_002");
      break;
    case CD_PROP_BOOL:
      field = params.get_input<Field<bool>>("Value_003");
      break;
    case CD_PROP_INT32:
      field = params.get_input<Field<int>>("Value_004");
      break;
    default:
      break;
  }

  WeakAnonymousAttributeID anonymous_id{"Attribute"};
  const CPPType &type = field.cpp_type();

  static const Array<GeometryComponentType> types = {
      GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_CURVE};
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    for (const GeometryComponentType type : types) {
      if (geometry_set.has(type)) {
        GeometryComponent &component = geometry_set.get_component_for_write(type);
        try_capture_field_on_geometry(component, anonymous_id.get(), domain, field);
      }
    }
  });

  GField output_field{std::make_shared<bke::AnonymousAttributeFieldInput>(
      std::move(anonymous_id), type, params.attribute_producer_name())};

  switch (data_type) {
    case CD_PROP_FLOAT: {
      params.set_output("Attribute_001", Field<float>(output_field));
      break;
    }
    case CD_PROP_FLOAT3: {
      params.set_output("Attribute", Field<float3>(output_field));
      break;
    }
    case CD_PROP_COLOR: {
      params.set_output("Attribute_002", Field<ColorGeometry4f>(output_field));
      break;
    }
    case CD_PROP_BOOL: {
      params.set_output("Attribute_003", Field<bool>(output_field));
      break;
    }
    case CD_PROP_INT32: {
      params.set_output("Attribute_004", Field<int>(output_field));
      break;
    }
    default:
      break;
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_capture()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_CAPTURE_ATTRIBUTE, "Capture Attribute", NODE_CLASS_ATTRIBUTE, 0);
  node_type_storage(&ntype,
                    "NodeGeometryAttributeCapture",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  node_type_init(&ntype, blender::nodes::geo_node_attribute_capture_init);
  node_type_update(&ntype, blender::nodes::geo_node_attribute_capture_update);
  ntype.declare = blender::nodes::geo_node_attribute_capture_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_capture_exec;
  ntype.draw_buttons = blender::nodes::geo_node_attribute_capture_layout;
  nodeRegisterType(&ntype);
}
