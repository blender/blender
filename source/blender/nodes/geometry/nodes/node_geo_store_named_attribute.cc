/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_store_named_attribute_cc {

NODE_STORAGE_FUNCS(NodeGeometryStoreNamedAttribute)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Name")).is_attribute_name();
  b.add_input<decl::Vector>(N_("Value"), "Value_Vector").supports_field();
  b.add_input<decl::Float>(N_("Value"), "Value_Float").supports_field();
  b.add_input<decl::Color>(N_("Value"), "Value_Color").supports_field();
  b.add_input<decl::Bool>(N_("Value"), "Value_Bool").supports_field();
  b.add_input<decl::Int>(N_("Value"), "Value_Int").supports_field();

  b.add_output<decl::Geometry>(N_("Geometry"));
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
  NodeGeometryStoreNamedAttribute *data = MEM_cnew<NodeGeometryStoreNamedAttribute>(__func__);
  data->data_type = CD_PROP_FLOAT;
  data->domain = ATTR_DOMAIN_POINT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryStoreNamedAttribute &storage = node_storage(*node);
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);

  bNodeSocket *socket_geometry = (bNodeSocket *)node->inputs.first;
  bNodeSocket *socket_name = socket_geometry->next;
  bNodeSocket *socket_vector = socket_name->next;
  bNodeSocket *socket_float = socket_vector->next;
  bNodeSocket *socket_color4f = socket_float->next;
  bNodeSocket *socket_boolean = socket_color4f->next;
  bNodeSocket *socket_int32 = socket_boolean->next;

  nodeSetSocketAvailability(ntree, socket_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(ntree, socket_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(ntree, socket_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(ntree, socket_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(ntree, socket_int32, data_type == CD_PROP_INT32);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_named_attribute_nodes == 0) {
    return;
  }
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  search_link_ops_for_declarations(params, declaration.inputs().take_front(2));

  if (params.in_out() == SOCK_OUT) {
    const std::optional<CustomDataType> type = node_data_type_to_custom_data_type(
        static_cast<eNodeSocketDatatype>(params.other_socket().type));
    if (type && *type != CD_PROP_STRING) {
      /* The input and output sockets have the same name. */
      params.add_item(IFACE_("Value"), [type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeStoreNamedAttribute");
        node_storage(node).data_type = *type;
        params.update_and_connect_available_socket(node, "Value");
      });
    }
  }
}

static void try_capture_field_on_geometry(GeometryComponent &component,
                                          const StringRef name,
                                          const AttributeDomain domain,
                                          const GField &field)
{
  GeometryComponentFieldContext field_context{component, domain};
  const int domain_size = component.attribute_domain_size(domain);
  const IndexMask mask{IndexMask(domain_size)};

  const CustomDataType data_type = bke::cpp_type_to_custom_data_type(field.cpp_type());

  /* Don't use #add_with_destination because the field might depend on an attribute
   * with that name, and changing it as part of evaluation might affect the result. */
  fn::FieldEvaluator evaluator{field_context, &mask};
  evaluator.add(field);
  evaluator.evaluate();
  const GVArray &result = evaluator.get_evaluated(0);
  OutputAttribute attribute = component.attribute_try_get_for_output_only(name, domain, data_type);
  if (attribute) {
    result.materialize(attribute.as_span().data());
    attribute.save();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  std::string name = params.extract_input<std::string>("Name");

  if (!U.experimental.use_named_attribute_nodes) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  const NodeGeometryStoreNamedAttribute &storage = node_storage(params.node());
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);
  const AttributeDomain domain = static_cast<AttributeDomain>(storage.domain);

  GField field;
  switch (data_type) {
    case CD_PROP_FLOAT:
      field = params.get_input<Field<float>>("Value_Float");
      break;
    case CD_PROP_FLOAT3:
      field = params.get_input<Field<float3>>("Value_Vector");
      break;
    case CD_PROP_COLOR:
      field = params.get_input<Field<ColorGeometry4f>>("Value_Color");
      break;
    case CD_PROP_BOOL:
      field = params.get_input<Field<bool>>("Value_Bool");
      break;
    case CD_PROP_INT32:
      field = params.get_input<Field<int>>("Value_Int");
      break;
    default:
      break;
  }

  /* Run on the instances component separately to only affect the top level of instances. */
  if (domain == ATTR_DOMAIN_INSTANCE) {
    if (geometry_set.has_instances()) {
      GeometryComponent &component = geometry_set.get_component_for_write(
          GEO_COMPONENT_TYPE_INSTANCES);
      try_capture_field_on_geometry(component, name, domain, field);
    }
  }
  else {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      for (const GeometryComponentType type :
           {GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_CURVE}) {
        if (geometry_set.has(type)) {
          GeometryComponent &component = geometry_set.get_component_for_write(type);
          try_capture_field_on_geometry(component, name, domain, field);
        }
      }
    });
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_store_named_attribute_cc

void register_node_type_geo_store_named_attribute()
{
  namespace file_ns = blender::nodes::node_geo_store_named_attribute_cc;
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_STORE_NAMED_ATTRIBUTE, "Store Named Attribute", NODE_CLASS_ATTRIBUTE);
  node_type_storage(&ntype,
                    "NodeGeometryStoreNamedAttribute",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  node_type_size(&ntype, 140, 100, 700);
  node_type_init(&ntype, file_ns::node_init);
  ntype.updatefunc = file_ns::node_update;
  ntype.declare = file_ns::node_declare;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
