/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_attribute_math.hh"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_attribute_capture_cc {

NODE_STORAGE_FUNCS(NodeGeometryAttributeCapture)

static void node_declare(NodeDeclarationBuilder &b)
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

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryAttributeCapture *data = MEM_cnew<NodeGeometryAttributeCapture>(__func__);
  data->data_type = CD_PROP_FLOAT;
  data->domain = ATTR_DOMAIN_POINT;

  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryAttributeCapture &storage = node_storage(*node);
  const eCustomDataType data_type = static_cast<eCustomDataType>(storage.data_type);

  bNodeSocket *socket_value_geometry = (bNodeSocket *)node->inputs.first;
  bNodeSocket *socket_value_vector = socket_value_geometry->next;
  bNodeSocket *socket_value_float = socket_value_vector->next;
  bNodeSocket *socket_value_color4f = socket_value_float->next;
  bNodeSocket *socket_value_boolean = socket_value_color4f->next;
  bNodeSocket *socket_value_int32 = socket_value_boolean->next;

  nodeSetSocketAvailability(ntree, socket_value_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(ntree, socket_value_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(ntree, socket_value_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(ntree, socket_value_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(ntree, socket_value_int32, data_type == CD_PROP_INT32);

  bNodeSocket *out_socket_value_geometry = (bNodeSocket *)node->outputs.first;
  bNodeSocket *out_socket_value_vector = out_socket_value_geometry->next;
  bNodeSocket *out_socket_value_float = out_socket_value_vector->next;
  bNodeSocket *out_socket_value_color4f = out_socket_value_float->next;
  bNodeSocket *out_socket_value_boolean = out_socket_value_color4f->next;
  bNodeSocket *out_socket_value_int32 = out_socket_value_boolean->next;

  nodeSetSocketAvailability(ntree, out_socket_value_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(ntree, out_socket_value_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(ntree, out_socket_value_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(ntree, out_socket_value_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(ntree, out_socket_value_int32, data_type == CD_PROP_INT32);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  search_link_ops_for_declarations(params, declaration.inputs().take_front(1));
  search_link_ops_for_declarations(params, declaration.outputs().take_front(1));

  const bNodeType &node_type = params.node_type();
  const std::optional<eCustomDataType> type = node_data_type_to_custom_data_type(
      (eNodeSocketDatatype)params.other_socket().type);
  if (type && *type != CD_PROP_STRING) {
    if (params.in_out() == SOCK_OUT) {
      params.add_item(IFACE_("Attribute"), [node_type, type](LinkSearchOpParams &params) {
        bNode &node = params.add_node(node_type);
        node_storage(node).data_type = *type;
        params.update_and_connect_available_socket(node, "Attribute");
      });
    }
    else {
      params.add_item(IFACE_("Value"), [node_type, type](LinkSearchOpParams &params) {
        bNode &node = params.add_node(node_type);
        node_storage(node).data_type = *type;
        params.update_and_connect_available_socket(node, "Value");
      });
    }
  }
}

static void try_capture_field_on_geometry(GeometryComponent &component,
                                          const AttributeIDRef &attribute_id,
                                          const eAttrDomain domain,
                                          const GField &field)
{
  GeometryComponentFieldContext field_context{component, domain};
  const int domain_num = component.attribute_domain_num(domain);
  const IndexMask mask{IndexMask(domain_num)};

  const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(field.cpp_type());
  OutputAttribute output_attribute = component.attribute_try_get_for_output_only(
      attribute_id, domain, data_type);

  fn::FieldEvaluator evaluator{field_context, &mask};
  evaluator.add_with_destination(field, output_attribute.varray());
  evaluator.evaluate();

  output_attribute.save();
}

static StringRefNull identifier_suffix(eCustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      return "_001";
    case CD_PROP_INT32:
      return "_004";
    case CD_PROP_COLOR:
      return "_002";
    case CD_PROP_BOOL:
      return "_003";
    case CD_PROP_FLOAT3:
      return "";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (!params.output_is_required("Geometry")) {
    params.error_message_add(
        NodeWarningType::Info,
        TIP_("The attribute output can not be used without the geometry output"));
    params.set_default_remaining_outputs();
    return;
  }

  const NodeGeometryAttributeCapture &storage = node_storage(params.node());
  const eCustomDataType data_type = static_cast<eCustomDataType>(storage.data_type);
  const eAttrDomain domain = static_cast<eAttrDomain>(storage.domain);

  const std::string output_identifier = "Attribute" + identifier_suffix(data_type);

  if (!params.output_is_required(output_identifier)) {
    params.set_output("Geometry", geometry_set);
    return;
  }

  const std::string input_identifier = "Value" + identifier_suffix(data_type);
  GField field;

  switch (data_type) {
    case CD_PROP_FLOAT:
      field = params.get_input<Field<float>>(input_identifier);
      break;
    case CD_PROP_FLOAT3:
      field = params.get_input<Field<float3>>(input_identifier);
      break;
    case CD_PROP_COLOR:
      field = params.get_input<Field<ColorGeometry4f>>(input_identifier);
      break;
    case CD_PROP_BOOL:
      field = params.get_input<Field<bool>>(input_identifier);
      break;
    case CD_PROP_INT32:
      field = params.get_input<Field<int>>(input_identifier);
      break;
    default:
      break;
  }

  WeakAnonymousAttributeID anonymous_id{"Attribute"};
  const CPPType &type = field.cpp_type();

  /* Run on the instances component separately to only affect the top level of instances. */
  if (domain == ATTR_DOMAIN_INSTANCE) {
    if (geometry_set.has_instances()) {
      GeometryComponent &component = geometry_set.get_component_for_write(
          GEO_COMPONENT_TYPE_INSTANCES);
      try_capture_field_on_geometry(component, anonymous_id.get(), domain, field);
    }
  }
  else {
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
  }

  GField output_field{std::make_shared<bke::AnonymousAttributeFieldInput>(
      std::move(anonymous_id), type, params.attribute_producer_name())};

  switch (data_type) {
    case CD_PROP_FLOAT: {
      params.set_output(output_identifier, Field<float>(output_field));
      break;
    }
    case CD_PROP_FLOAT3: {
      params.set_output(output_identifier, Field<float3>(output_field));
      break;
    }
    case CD_PROP_COLOR: {
      params.set_output(output_identifier, Field<ColorGeometry4f>(output_field));
      break;
    }
    case CD_PROP_BOOL: {
      params.set_output(output_identifier, Field<bool>(output_field));
      break;
    }
    case CD_PROP_INT32: {
      params.set_output(output_identifier, Field<int>(output_field));
      break;
    }
    default:
      break;
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes::node_geo_attribute_capture_cc

void register_node_type_geo_attribute_capture()
{
  namespace file_ns = blender::nodes::node_geo_attribute_capture_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_CAPTURE_ATTRIBUTE, "Capture Attribute", NODE_CLASS_ATTRIBUTE);
  node_type_storage(&ntype,
                    "NodeGeometryAttributeCapture",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}
