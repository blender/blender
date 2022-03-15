/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_named_attribute_cc {

NODE_STORAGE_FUNCS(NodeGeometryInputNamedAttribute)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>(N_("Name")).is_attribute_name();

  b.add_output<decl::Vector>(N_("Attribute"), "Attribute_Vector").field_source();
  b.add_output<decl::Float>(N_("Attribute"), "Attribute_Float").field_source();
  b.add_output<decl::Color>(N_("Attribute"), "Attribute_Color").field_source();
  b.add_output<decl::Bool>(N_("Attribute"), "Attribute_Bool").field_source();
  b.add_output<decl::Int>(N_("Attribute"), "Attribute_Int").field_source();
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryInputNamedAttribute *data = MEM_cnew<NodeGeometryInputNamedAttribute>(__func__);
  data->data_type = CD_PROP_FLOAT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryInputNamedAttribute &storage = node_storage(*node);
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);

  bNodeSocket *socket_vector = (bNodeSocket *)node->outputs.first;
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
  search_link_ops_for_declarations(params, declaration.inputs());

  if (params.in_out() == SOCK_OUT) {
    const std::optional<CustomDataType> type = node_data_type_to_custom_data_type(
        static_cast<eNodeSocketDatatype>(params.other_socket().type));
    if (type && *type != CD_PROP_STRING) {
      /* The input and output sockets have the same name. */
      params.add_item(IFACE_("Attribute"), [type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeInputNamedAttribute");
        node_storage(node).data_type = *type;
        params.update_and_connect_available_socket(node, "Attribute");
      });
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryInputNamedAttribute &storage = node_storage(params.node());
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);

  const std::string name = params.extract_input<std::string>("Name");

  if (!U.experimental.use_named_attribute_nodes) {
    params.set_default_remaining_outputs();
    return;
  }

  switch (data_type) {
    case CD_PROP_FLOAT:
      params.set_output("Attribute_Float", AttributeFieldInput::Create<float>(std::move(name)));
      break;
    case CD_PROP_FLOAT3:
      params.set_output("Attribute_Vector", AttributeFieldInput::Create<float3>(std::move(name)));
      break;
    case CD_PROP_COLOR:
      params.set_output("Attribute_Color",
                        AttributeFieldInput::Create<ColorGeometry4f>(std::move(name)));
      break;
    case CD_PROP_BOOL:
      params.set_output("Attribute_Bool", AttributeFieldInput::Create<bool>(std::move(name)));
      break;
    case CD_PROP_INT32:
      params.set_output("Attribute_Int", AttributeFieldInput::Create<int>(std::move(name)));
      break;
    default:
      break;
  }
}

}  // namespace blender::nodes::node_geo_input_named_attribute_cc

void register_node_type_geo_input_named_attribute()
{
  namespace file_ns = blender::nodes::node_geo_input_named_attribute_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_NAMED_ATTRIBUTE, "Named Attribute", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  ntype.updatefunc = file_ns::node_update;
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(&ntype,
                    "NodeGeometryInputNamedAttribute",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
