/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_named_attribute_cc {

NODE_STORAGE_FUNCS(NodeGeometryInputNamedAttribute)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Name").is_attribute_name();

  b.add_output<decl::Vector>("Attribute", "Attribute_Vector").field_source();
  b.add_output<decl::Float>("Attribute", "Attribute_Float").field_source();
  b.add_output<decl::Color>("Attribute", "Attribute_Color").field_source();
  b.add_output<decl::Bool>("Attribute", "Attribute_Bool").field_source();
  b.add_output<decl::Int>("Attribute", "Attribute_Int").field_source();

  b.add_output<decl::Bool>("Exists").field_source();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryInputNamedAttribute *data = MEM_cnew<NodeGeometryInputNamedAttribute>(__func__);
  data->data_type = CD_PROP_FLOAT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryInputNamedAttribute &storage = node_storage(*node);
  const eCustomDataType data_type = eCustomDataType(storage.data_type);

  bNodeSocket *socket_vector = static_cast<bNodeSocket *>(node->outputs.first);
  bNodeSocket *socket_float = socket_vector->next;
  bNodeSocket *socket_color4f = socket_float->next;
  bNodeSocket *socket_boolean = socket_color4f->next;
  bNodeSocket *socket_int32 = socket_boolean->next;

  bke::nodeSetSocketAvailability(ntree, socket_vector, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_float, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_color4f, data_type == CD_PROP_COLOR);
  bke::nodeSetSocketAvailability(ntree, socket_boolean, data_type == CD_PROP_BOOL);
  bke::nodeSetSocketAvailability(ntree, socket_int32, data_type == CD_PROP_INT32);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  search_link_ops_for_declarations(params, declaration.inputs);

  const bNodeType &node_type = params.node_type();
  if (params.in_out() == SOCK_OUT) {
    const std::optional<eCustomDataType> type = node_data_type_to_custom_data_type(
        eNodeSocketDatatype(params.other_socket().type));
    if (type && *type != CD_PROP_STRING) {
      /* The input and output sockets have the same name. */
      params.add_item(IFACE_("Attribute"), [node_type, type](LinkSearchOpParams &params) {
        bNode &node = params.add_node(node_type);
        node_storage(node).data_type = *type;
        params.update_and_connect_available_socket(node, "Attribute");
      });
      params.add_item(
          IFACE_("Exists"),
          [node_type](LinkSearchOpParams &params) {
            bNode &node = params.add_node(node_type);
            params.update_and_connect_available_socket(node, "Exists");
          },
          -1);
    }
  }
}

class AttributeExistsFieldInput final : public bke::GeometryFieldInput {
 private:
  std::string name_;

 public:
  AttributeExistsFieldInput(std::string name, const CPPType &type)
      : GeometryFieldInput(type, name), name_(std::move(name))
  {
    category_ = Category::Generated;
  }

  static Field<bool> Create(std::string name)
  {
    const CPPType &type = CPPType::get<bool>();
    auto field_input = std::make_shared<AttributeExistsFieldInput>(std::move(name), type);
    return Field<bool>(field_input);
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask /*mask*/) const final
  {
    const bool exists = context.attributes()->contains(name_);
    const int domain_size = context.attributes()->domain_size(context.domain());
    return VArray<bool>::ForSingle(exists, domain_size);
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryInputNamedAttribute &storage = node_storage(params.node());
  const eCustomDataType data_type = eCustomDataType(storage.data_type);

  const std::string name = params.extract_input<std::string>("Name");

  if (name.empty()) {
    params.set_default_remaining_outputs();
    return;
  }
  if (!bke::allow_procedural_attribute_access(name)) {
    params.error_message_add(NodeWarningType::Info, TIP_(bke::no_procedural_access_message));
    params.set_default_remaining_outputs();
    return;
  }

  params.used_named_attribute(name, NamedAttributeUsage::Read);

  switch (data_type) {
    case CD_PROP_FLOAT:
      params.set_output("Attribute_Float", AttributeFieldInput::Create<float>(name));
      break;
    case CD_PROP_FLOAT3:
      params.set_output("Attribute_Vector", AttributeFieldInput::Create<float3>(name));
      break;
    case CD_PROP_COLOR:
      params.set_output("Attribute_Color", AttributeFieldInput::Create<ColorGeometry4f>(name));
      break;
    case CD_PROP_BOOL:
      params.set_output("Attribute_Bool", AttributeFieldInput::Create<bool>(name));
      break;
    case CD_PROP_INT32:
      params.set_output("Attribute_Int", AttributeFieldInput::Create<int>(name));
      break;
    default:
      break;
  }

  params.set_output("Exists", AttributeExistsFieldInput::Create(std::move(name)));
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
  ntype.initfunc = file_ns::node_init;
  node_type_storage(&ntype,
                    "NodeGeometryInputNamedAttribute",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
