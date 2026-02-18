/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include <cstddef>
#include <cstring>

#include "DNA_asset_types.h"
#include "DNA_node_types.h"
#include "DNA_space_types.h"

#include "BLI_array.hh"
#include "BLI_disjoint_set.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_vector_set.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_interface.hh"

#include "COM_node_operation.hh"
#include "COM_result.hh"

#include "MEM_guardedalloc.h"

#include "NOD_common.hh"
#include "NOD_composite.hh"
#include "NOD_geometry_exec.hh"
#include "NOD_node_declaration.hh"
#include "NOD_node_extra_info.hh"
#include "NOD_register.hh"
#include "NOD_socket.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "UI_resources.hh"

#include "ED_node.hh"

#include "node_common.h"
#include "node_util.hh"

namespace blender {

namespace node_interface = bke::node_interface;

/* -------------------------------------------------------------------- */
/** \name Node Group
 * \{ */

static bNodeSocket *find_matching_socket(ListBaseT<bNodeSocket> &sockets, StringRef identifier)
{
  for (bNodeSocket &socket : sockets) {
    if (socket.identifier == identifier) {
      return &socket;
    }
  }
  return nullptr;
}

bNodeSocket *node_group_find_input_socket(bNode *groupnode, const StringRef identifier)
{
  return find_matching_socket(groupnode->inputs, identifier);
}

bNodeSocket *node_group_find_output_socket(bNode *groupnode, const StringRef identifier)
{
  return find_matching_socket(groupnode->outputs, identifier);
}

void node_group_label(const bNodeTree * /*ntree*/,
                      const bNode *node,
                      char *label,
                      int label_maxncpy)
{
  BLI_strncpy(label,
              (node->id) ? node->id->name + 2 :
                           CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, "Missing Data-Block"),
              label_maxncpy);
}

int node_group_ui_class(const bNode *node)
{
  const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id);
  if (!group) {
    return NODE_CLASS_GROUP;
  }
  switch (bke::NodeColorTag(group->color_tag)) {
    case bke::NodeColorTag::None:
      return NODE_CLASS_GROUP;
    case bke::NodeColorTag::Attribute:
      return NODE_CLASS_ATTRIBUTE;
    case bke::NodeColorTag::Color:
      return NODE_CLASS_OP_COLOR;
    case bke::NodeColorTag::Converter:
      return NODE_CLASS_CONVERTER;
    case bke::NodeColorTag::Distort:
      return NODE_CLASS_DISTORT;
    case bke::NodeColorTag::Filter:
      return NODE_CLASS_OP_FILTER;
    case bke::NodeColorTag::Geometry:
      return NODE_CLASS_GEOMETRY;
    case bke::NodeColorTag::Input:
      return NODE_CLASS_INPUT;
    case bke::NodeColorTag::Matte:
      return NODE_CLASS_MATTE;
    case bke::NodeColorTag::Output:
      return NODE_CLASS_OUTPUT;
    case bke::NodeColorTag::Script:
      return NODE_CLASS_SCRIPT;
    case bke::NodeColorTag::Shader:
      return NODE_CLASS_SHADER;
    case bke::NodeColorTag::Texture:
      return NODE_CLASS_TEXTURE;
    case bke::NodeColorTag::Vector:
      return NODE_CLASS_OP_VECTOR;
    case bke::NodeColorTag::Pattern:
      return NODE_CLASS_PATTERN;
    case bke::NodeColorTag::Interface:
      return NODE_CLASS_INTERFACE;
    case bke::NodeColorTag::Group:
      return NODE_CLASS_GROUP;
  }
  return NODE_CLASS_GROUP;
}

bool node_group_poll_instance(const bNode *node,
                              const bNodeTree *nodetree,
                              const char **r_disabled_hint)
{
  if (!node->typeinfo->poll(node->typeinfo, nodetree, r_disabled_hint)) {
    return false;
  }
  const bNodeTree *grouptree = reinterpret_cast<const bNodeTree *>(node->id);
  if (!grouptree) {
    return true;
  }
  return bke::node_group_poll(nodetree, grouptree, r_disabled_hint);
}

std::string node_group_ui_description(const bNode &node)
{
  if (!node.id) {
    return "";
  }
  const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node.id);
  if (group->id.asset_data) {
    if (group->id.asset_data->description) {
      return group->id.asset_data->description;
    }
  }
  if (!group->description) {
    return "";
  }
  return group->description;
}

bool bke::node_group_poll(const bNodeTree *nodetree,
                          const bNodeTree *grouptree,
                          const char **r_disabled_hint)
{
  /* unspecified node group, generally allowed
   * (if anything, should be avoided on operator level)
   */
  if (grouptree == nullptr) {
    return true;
  }

  if (nodetree == grouptree) {
    if (r_disabled_hint) {
      *r_disabled_hint = RPT_("Nesting a node group inside of itself is not allowed");
    }
    return false;
  }
  if (nodetree->type != grouptree->type) {
    if (r_disabled_hint) {
      *r_disabled_hint = RPT_("Node group has different type");
    }
    return false;
  }

  for (const bNode *node : grouptree->all_nodes()) {
    if (node->typeinfo->poll_instance &&
        !node->typeinfo->poll_instance(node, nodetree, r_disabled_hint))
    {
      return false;
    }
  }
  return true;
}

namespace nodes {

static std::function<ID *(const bNode &node)> get_default_id_getter(
    const bNodeTreeInterface &tree_interface, const bNodeTreeInterfaceSocket &io_socket)
{
  const int item_index = tree_interface.find_item_index(io_socket.item);
  BLI_assert(item_index >= 0);

  /* Avoid capturing pointers that can become dangling. */
  return [item_index](const bNode &node) -> ID * {
    if (node.id == nullptr) {
      return nullptr;
    }
    if (GS(node.id->name) != ID_NT) {
      return nullptr;
    }
    const bNodeTree &ntree = *reinterpret_cast<const bNodeTree *>(node.id);
    const bNodeTreeInterfaceItem *io_item = ntree.tree_interface.get_item_at_index(item_index);
    const bNodeTreeInterfaceSocket *io_socket =
        node_interface::get_item_as<bNodeTreeInterfaceSocket>(io_item);
    if (!io_socket) {
      return nullptr;
    }
    return *static_cast<ID **>(io_socket->socket_data);
  };
}

static std::function<void(bNode &node, bNodeSocket &socket, const char *data_path)>
get_init_socket_fn(const bNodeTreeInterface &tree_interface,
                   const bNodeTreeInterfaceSocket &io_socket)
{
  const int item_index = tree_interface.find_item_index(io_socket.item);
  BLI_assert(item_index >= 0);

  /* Avoid capturing pointers that can become dangling. */
  return [item_index](bNode &node, bNodeSocket &socket, const char *data_path) {
    if (node.id == nullptr) {
      return;
    }
    if (GS(node.id->name) != ID_NT) {
      return;
    }
    bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(node.id);
    const bNodeTreeInterfaceItem *io_item = ntree.tree_interface.get_item_at_index(item_index);
    if (io_item == nullptr || io_item->item_type != NODE_INTERFACE_SOCKET) {
      return;
    }
    const bNodeTreeInterfaceSocket &io_socket =
        node_interface::get_item_as<bNodeTreeInterfaceSocket>(*io_item);
    bke::bNodeSocketType *typeinfo = io_socket.socket_typeinfo();
    if (typeinfo && typeinfo->interface_init_socket) {
      typeinfo->interface_init_socket(&ntree.id, &io_socket, &node, &socket, data_path);
    }
  };
}

static BaseSocketDeclarationBuilder &build_interface_socket_declaration(
    const bNodeTree &tree,
    const bNodeTreeInterfaceSocket &io_socket,
    const std::optional<StructureType> structure_type,
    const eNodeSocketInOut in_out,
    DeclarationListBuilder &b)
{
  bke::bNodeSocketType *base_typeinfo = bke::node_socket_type_find(io_socket.socket_type);
  eNodeSocketDatatype datatype = SOCK_CUSTOM;

  const StringRef name = io_socket.name;
  const StringRef identifier = io_socket.identifier;

  BaseSocketDeclarationBuilder *decl = nullptr;
  if (base_typeinfo) {
    datatype = base_typeinfo->type;
    switch (datatype) {
      case SOCK_FLOAT: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueFloat>(io_socket);
        decl = &b.add_socket<decl::Float>(name, identifier, in_out)
                    .subtype(PropertySubType(value.subtype))
                    .default_value(value.value)
                    .min(value.min)
                    .max(value.max);
        break;
      }
      case SOCK_VECTOR: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueVector>(io_socket);
        decl = &b.add_socket<decl::Vector>(name, identifier, in_out)
                    .subtype(PropertySubType(value.subtype))
                    .default_value(float4(value.value))
                    .dimensions(value.dimensions)
                    .min(value.min)
                    .max(value.max);
        break;
      }
      case SOCK_RGBA: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueRGBA>(io_socket);
        decl = &b.add_socket<decl::Color>(name, identifier, in_out).default_value(value.value);
        break;
      }
      case SOCK_SHADER: {
        decl = &b.add_socket<decl::Shader>(name, identifier, in_out);
        break;
      }
      case SOCK_BOOLEAN: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueBoolean>(io_socket);
        decl = &b.add_socket<decl::Bool>(name, identifier, in_out).default_value(value.value);
        break;
      }
      case SOCK_ROTATION: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueRotation>(
            io_socket);
        decl = &b.add_socket<decl::Rotation>(name, identifier, in_out)
                    .default_value(math::EulerXYZ(float3(value.value_euler)));
        break;
      }
      case SOCK_MATRIX: {
        decl = &b.add_socket<decl::Matrix>(name, identifier, in_out);
        break;
      }
      case SOCK_INT: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueInt>(io_socket);
        decl = &b.add_socket<decl::Int>(name, identifier, in_out)
                    .subtype(PropertySubType(value.subtype))
                    .default_value(value.value)
                    .min(value.min)
                    .max(value.max);
        break;
      }
      case SOCK_STRING: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueString>(io_socket);
        decl = &b.add_socket<decl::String>(name, identifier, in_out)
                    .subtype(PropertySubType(value.subtype))
                    .default_value(value.value);
        break;
      }
      case SOCK_MENU: {
        const auto &value = node_interface::get_socket_data_as<bNodeSocketValueMenu>(io_socket);
        decl = &b.add_socket<decl::Menu>(name, identifier, in_out)
                    .default_value(MenuValue(value.value))
                    .expanded(io_socket.flag & NODE_INTERFACE_SOCKET_MENU_EXPANDED)
                    .optional_label();
        break;
      }
      case SOCK_OBJECT: {
        decl = &b.add_socket<decl::Object>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_IMAGE: {
        decl = &b.add_socket<decl::Image>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_GEOMETRY:
        decl = &b.add_socket<decl::Geometry>(name, identifier, in_out);
        break;
      case SOCK_COLLECTION: {
        decl = &b.add_socket<decl::Collection>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_TEXTURE: {
        decl = &b.add_socket<decl::Texture>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_MATERIAL: {
        decl = &b.add_socket<decl::Material>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_FONT: {
        decl = &b.add_socket<decl::Font>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_SCENE: {
        decl = &b.add_socket<decl::Scene>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_TEXT_ID: {
        decl = &b.add_socket<decl::Text>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_MASK: {
        decl = &b.add_socket<decl::Mask>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_SOUND: {
        decl = &b.add_socket<decl::Sound>(name, identifier, in_out)
                    .default_value_fn(get_default_id_getter(tree.tree_interface, io_socket));
        break;
      }
      case SOCK_BUNDLE: {
        decl = &b.add_socket<decl::Bundle>(name, identifier, in_out);
        break;
      }
      case SOCK_CLOSURE: {
        decl = &b.add_socket<decl::Closure>(name, identifier, in_out);
        break;
      }
      case SOCK_CUSTOM: {
        decl = &b.add_socket<decl::Custom>(name, identifier, in_out)
                    .idname(io_socket.socket_type)
                    .init_socket_fn(get_init_socket_fn(tree.tree_interface, io_socket));
        break;
      }
    }
  }
  else {
    decl = &b.add_socket<decl::Custom>(name, identifier, in_out)
                .idname(io_socket.socket_type)
                .init_socket_fn(get_init_socket_fn(tree.tree_interface, io_socket));
  }
  decl->description(io_socket.description ? io_socket.description : "");
  decl->hide_value(io_socket.flag & NODE_INTERFACE_SOCKET_HIDE_VALUE);
  decl->compact(io_socket.flag & NODE_INTERFACE_SOCKET_COMPACT);
  decl->panel_toggle(io_socket.flag & NODE_INTERFACE_SOCKET_PANEL_TOGGLE);
  decl->optional_label(io_socket.flag & NODE_INTERFACE_SOCKET_OPTIONAL_LABEL);
  decl->default_input_type(NodeDefaultInputType(io_socket.default_input));
  if (structure_type) {
    decl->structure_type(*structure_type);
  }
  if (io_socket.default_input != NODE_DEFAULT_INPUT_VALUE) {
    decl->hide_value();
  }
  return *decl;
}

static void node_group_declare_panel_recursive(
    DeclarationListBuilder &b,
    const bNode &node,
    const bNodeTree &group,
    const Map<const bNodeTreeInterfaceSocket *, StructureType> &structure_type_by_socket,
    const bNodeTreeInterfacePanel &io_parent_panel,
    const bool is_root)
{
  bool layout_added = false;
  auto add_layout_if_needed = [&]() {
    /* Some custom group nodes don't have a draw function. */
    if (node.typeinfo->draw_buttons) {
      if (is_root && !layout_added) {
        b.add_default_layout();
        layout_added = true;
      }
    }
  };

  for (const bNodeTreeInterfaceItem *item : io_parent_panel.items()) {
    switch (eNodeTreeInterfaceItemType(item->item_type)) {
      case NODE_INTERFACE_SOCKET: {
        const auto &io_socket = node_interface::get_item_as<bNodeTreeInterfaceSocket>(*item);
        const eNodeSocketInOut in_out = (io_socket.flag & NODE_INTERFACE_SOCKET_INPUT) ? SOCK_IN :
                                                                                         SOCK_OUT;
        if (in_out == SOCK_IN) {
          add_layout_if_needed();
        }
        build_interface_socket_declaration(
            group, io_socket, structure_type_by_socket.lookup_try(&io_socket), in_out, b);
        break;
      }
      case NODE_INTERFACE_PANEL: {
        add_layout_if_needed();
        const auto &io_panel = node_interface::get_item_as<bNodeTreeInterfacePanel>(*item);
        auto &panel_b = b.add_panel(StringRef(io_panel.name), io_panel.identifier)
                            .description(StringRef(io_panel.description))
                            .default_closed(io_panel.flag & NODE_INTERFACE_PANEL_DEFAULT_CLOSED);
        node_group_declare_panel_recursive(
            panel_b, node, group, structure_type_by_socket, io_panel, false);
        break;
      }
    }
  }

  add_layout_if_needed();
}

void node_group_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }
  NodeDeclaration &r_declaration = b.declaration();
  const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id);
  if (!group) {
    return;
  }
  if (ID_IS_LINKED(&group->id) && (group->id.tag & ID_TAG_MISSING)) {
    r_declaration.skip_updating_sockets = true;
    return;
  }
  r_declaration.skip_updating_sockets = false;

  /* Allow the node group interface to define the socket order. */
  r_declaration.use_custom_socket_order = true;

  group->ensure_interface_cache();

  Map<const bNodeTreeInterfaceSocket *, StructureType> structure_type_by_socket;
  if (ELEM(group->type, NTREE_GEOMETRY, NTREE_COMPOSIT)) {
    structure_type_by_socket.reserve(group->interface_items().size());

    const Span<const bNodeTreeInterfaceSocket *> inputs = group->interface_inputs();
    const Span<StructureType> input_structure_types =
        group->runtime->structure_type_interface->inputs;
    for (const int i : inputs.index_range()) {
      structure_type_by_socket.add(inputs[i], input_structure_types[i]);
    }

    const Span<const bNodeTreeInterfaceSocket *> outputs = group->interface_outputs();
    const Span<StructureTypeInterface::OutputDependency> output_structure_types =
        group->runtime->structure_type_interface->outputs;
    for (const int i : outputs.index_range()) {
      structure_type_by_socket.add(outputs[i], output_structure_types[i].type);
    }
  }

  node_group_declare_panel_recursive(
      b, *node, *group, structure_type_by_socket, group->tree_interface.root_panel, true);

  if (group->type == NTREE_GEOMETRY) {
    group->ensure_interface_cache();
    const Span<const bNodeTreeInterfaceSocket *> inputs = group->interface_inputs();
    const FieldInferencingInterface &field_interface =
        *group->runtime->field_inferencing_interface;
    for (const int i : inputs.index_range()) {
      SocketDeclaration &decl = *r_declaration.inputs[i];
      decl.input_field_type = field_interface.inputs[i];
    }

    for (const int i : r_declaration.outputs.index_range()) {
      r_declaration.outputs[i]->output_field_dependency = field_interface.outputs[i];
    }
  }
}

}  // namespace nodes

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Frame
 * \{ */

static void node_frame_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeFrame *data = MEM_new<NodeFrame>("frame node storage");
  node->storage = data;

  data->flag |= NODE_FRAME_SHRINK;

  data->label_size = 20;
}

void register_node_type_frame()
{
  /* frame type is used for all tree types, needs dynamic allocation */
  bke::bNodeType *ntype = MEM_new<bke::bNodeType>("frame node type");
  ntype->free_self = [](bke::bNodeType *type) { MEM_delete(type); };

  bke::node_type_base(*ntype, "NodeFrame", NODE_FRAME);
  ntype->ui_name = "Frame";
  ntype->ui_description =
      "Collect related nodes together in a common area. Useful for organization when the "
      "re-usability of a node group is not required";
  ntype->nclass = NODE_CLASS_LAYOUT;
  ntype->enum_name_legacy = "FRAME";
  ntype->initfunc = node_frame_init;
  bke::node_type_storage(
      *ntype, "NodeFrame", node_free_standard_storage, node_copy_standard_storage);
  bke::node_type_size(*ntype, 150, 100, 0);
  ntype->flag |= NODE_BACKGROUND;

  bke::node_register_type(*ntype);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Re-Route
 * \{ */

static void node_reroute_declare(nodes::NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }

  const StringRefNull socket_idname(static_cast<const NodeReroute *>(node->storage)->type_idname);
  b.add_input<nodes::decl::Custom>("Input")
      .idname(socket_idname.c_str())
      .structure_type(nodes::StructureType::Dynamic);
  b.add_output<nodes::decl::Custom>("Output")
      .idname(socket_idname.c_str())
      .structure_type(nodes::StructureType::Dynamic);
}

static void node_reroute_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeReroute *data = MEM_new<NodeReroute>(__func__);
  STRNCPY(data->type_idname, "NodeSocketColor");
  node->storage = data;
}

static bool node_reroute_poll_instance(const bNode *node,
                                       const bNodeTree *nodetree,
                                       const char **r_disabled_hint)
{
  const NodeReroute &data = *static_cast<NodeReroute *>(node->storage);
  bke::bNodeSocketType *socket_type = bke::node_socket_type_find(data.type_idname);
  if (!socket_type) {
    if (r_disabled_hint) {
      *r_disabled_hint = "Socket type not found";
    }
    return false;
  }
  bke::bNodeTreeType &tree_type = *nodetree->typeinfo;
  if (tree_type.valid_socket_type && !tree_type.valid_socket_type(&tree_type, socket_type)) {
    if (r_disabled_hint) {
      *r_disabled_hint = "Socket type not supported";
    }
    return false;
  }
  return true;
}

void register_node_type_reroute()
{
  /* frame type is used for all tree types, needs dynamic allocation */
  bke::bNodeType *ntype = MEM_new<bke::bNodeType>("frame node type");
  ntype->free_self = [](bke::bNodeType *type) { MEM_delete(type); };

  bke::node_type_base(*ntype, "NodeReroute", NODE_REROUTE);
  ntype->ui_name = "Reroute";
  ntype->ui_description =
      "A single-socket organization tool that supports one input and multiple outputs";
  ntype->enum_name_legacy = "REROUTE";
  ntype->nclass = NODE_CLASS_LAYOUT;
  ntype->declare = node_reroute_declare;
  ntype->initfunc = node_reroute_init;
  node_type_storage(*ntype, "NodeReroute", node_free_standard_storage, node_copy_standard_storage);
  ntype->poll_instance = node_reroute_poll_instance;

  bke::node_register_type(*ntype);
}

struct RerouteTargetPriority {
  int node_i = std::numeric_limits<int>::max();
  int socket_in_node_i = std::numeric_limits<int>::max();

  RerouteTargetPriority() = default;
  RerouteTargetPriority(const bNodeSocket &socket)
      : node_i(socket.owner_node().index()), socket_in_node_i(socket.index())
  {
  }

  bool operator>(const RerouteTargetPriority other)
  {
    if (this->node_i == other.node_i) {
      return this->socket_in_node_i < other.socket_in_node_i;
    }
    return this->node_i < other.node_i;
  }
};

void ntree_update_reroute_nodes(bNodeTree *ntree)
{
  ntree->ensure_topology_cache();

  const Span<bNode *> all_reroute_nodes = ntree->nodes_by_type("NodeReroute");

  VectorSet<int> reroute_nodes;
  for (const bNode *reroute : all_reroute_nodes) {
    reroute_nodes.add(reroute->index());
  }

  /* Any reroute can be connected only to one source, or can be not connected at all.
   * So reroute forms a trees. It is possible that there will be cycle, but such cycle
   * can be only one in strongly connected set of reroutes. To propagate a types from
   * some certain target to all the reroutes in such a tree we need to know all such
   * a trees and all possible targets for each tree. */
  DisjointSet reroutes_groups(reroute_nodes.size());

  for (const bNode *src_reroute : all_reroute_nodes) {
    const int src_reroute_i = reroute_nodes.index_of(src_reroute->index());
    for (const bNodeSocket *dst_socket :
         src_reroute->output_sockets().first()->directly_linked_sockets())
    {
      const bNode &dst_node = dst_socket->owner_node();
      if (!dst_node.is_reroute()) {
        continue;
      }
      const int dst_reroute_i = reroute_nodes.index_of(dst_node.index());
      reroutes_groups.join(src_reroute_i, dst_reroute_i);
    }
  }

  VectorSet<int> reroute_groups;
  for (const int reroute_i : reroute_nodes.index_range()) {
    const int root_reroute_i = reroutes_groups.find_root(reroute_i);
    reroute_groups.add(root_reroute_i);
  }

  /* Any reroute can have only one source and many destination targets. Type propagation considers
   * source as target with highest priority. */
  Array<const bke::bNodeSocketType *> dst_type_by_reroute_group(reroute_groups.size(), nullptr);
  Array<const bke::bNodeSocketType *> src_type_by_reroute_group(reroute_groups.size(), nullptr);

  /* Reroute type priority based on the indices of target sockets in the node and the nodes in the
   * tree. */
  Array<RerouteTargetPriority> reroute_group_dst_type_priority(reroute_groups.size(),
                                                               RerouteTargetPriority{});

  for (const bNodeLink *link : ntree->all_links()) {
    const bNode *src_node = link->fromnode;
    const bNode *dst_node = link->tonode;

    if (src_node->is_reroute() == dst_node->is_reroute()) {
      continue;
    }

    if (!dst_node->is_reroute()) {
      const int src_reroute_i = reroute_nodes.index_of(src_node->index());
      const int src_reroute_root_i = reroutes_groups.find_root(src_reroute_i);
      const int src_reroute_group_i = reroute_groups.index_of(src_reroute_root_i);

      const RerouteTargetPriority type_priority(*link->tosock);
      if (reroute_group_dst_type_priority[src_reroute_group_i] > type_priority) {
        continue;
      }

      reroute_group_dst_type_priority[src_reroute_group_i] = type_priority;

      const bNodeSocket *dst_socket = link->tosock;
      /* There could be a function which will choose best from
       * #dst_type_by_reroute_group and #dst_socket, but right now this match behavior as-is. */
      dst_type_by_reroute_group[src_reroute_group_i] = dst_socket->typeinfo;
      continue;
    }

    BLI_assert(!src_node->is_reroute());
    const int dst_reroute_i = reroute_nodes.index_of(dst_node->index());
    const int dst_reroute_root_i = reroutes_groups.find_root(dst_reroute_i);
    const int dst_reroute_group_i = reroute_groups.index_of(dst_reroute_root_i);

    const bNodeSocket *src_socket = link->fromsock;
    /* There could be a function which will choose best from
     * #src_type_by_reroute_group and #src_socket, but right now this match behavior as-is. */
    src_type_by_reroute_group[dst_reroute_group_i] = src_socket->typeinfo;
  }

  const Span<bNode *> all_nodes = ntree->all_nodes();
  for (const int reroute_i : reroute_nodes.index_range()) {
    const int reroute_root_i = reroutes_groups.find_root(reroute_i);
    const int reroute_group_i = reroute_groups.index_of(reroute_root_i);

    const bke::bNodeSocketType *reroute_type = nullptr;
    if (dst_type_by_reroute_group[reroute_group_i] != nullptr) {
      reroute_type = dst_type_by_reroute_group[reroute_group_i];
    }
    if (src_type_by_reroute_group[reroute_group_i] != nullptr) {
      reroute_type = src_type_by_reroute_group[reroute_group_i];
    }

    if (reroute_type == nullptr) {
      continue;
    }

    const int reroute_index = reroute_nodes[reroute_i];
    bNode &reroute_node = *all_nodes[reroute_index];
    NodeReroute *storage = static_cast<NodeReroute *>(reroute_node.storage);
    if (reroute_type->idname != storage->type_idname) {
      StringRef(reroute_type->idname).copy_utf8_truncated(storage->type_idname);
      nodes::update_node_declaration_and_sockets(*ntree, reroute_node);
    }
  }
}

bool bke::node_is_connected_to_output(const bNodeTree &ntree, const bNode &node)
{
  ntree.ensure_topology_cache();
  Stack<const bNode *> nodes_to_check;
  for (const bNodeSocket *socket : node.output_sockets()) {
    for (const bNodeLink *link : socket->directly_linked_links()) {
      nodes_to_check.push(link->tonode);
    }
  }
  while (!nodes_to_check.is_empty()) {
    const bNode *next_node = nodes_to_check.pop();
    for (const bNodeSocket *socket : next_node->output_sockets()) {
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (link->tonode->typeinfo->nclass == NODE_CLASS_OUTPUT &&
            link->tonode->flag & NODE_DO_OUTPUT)
        {
          return true;
        }
        nodes_to_check.push(link->tonode);
      }
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implicit Conversion Node
 * \{ */

static void node_implicit_conversion_declare(nodes::NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }

  const StringRefNull socket_idname(
      static_cast<const NodeImplicitConversion *>(node->storage)->type_idname);
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<nodes::decl::Custom>("Value")
      .idname(socket_idname.c_str())
      .structure_type(nodes::StructureType::Dynamic)
      .optional_label();
  b.add_output<nodes::decl::Custom>("Value")
      .idname(socket_idname.c_str())
      .structure_type(nodes::StructureType::Dynamic)
      .reference_pass_all()
      .propagate_all()
      .align_with_previous();
}

static void node_implicit_conversion_label(const bNodeTree * /*ntree*/,
                                           const bNode *node,
                                           char *label,
                                           int label_maxncpy)
{
  const auto &data = *static_cast<NodeImplicitConversion *>(node->storage);
  const bke::bNodeSocketType *socket_type = bke::node_socket_type_find(data.type_idname);
  if (!socket_type) {
    BLI_strncpy(label,
                CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, node->typeinfo->ui_name.c_str()),
                label_maxncpy);
    return;
  }

  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_socket_data_type_items, socket_type->type, &name);
  if (!enum_label) {
    BLI_strncpy(label,
                CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, node->typeinfo->ui_name.c_str()),
                label_maxncpy);
    return;
  }

  BLI_snprintf_utf8(label, label_maxncpy, "To %s", CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, name));
}

static void node_implicit_conversion_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_implicit_conversion_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeImplicitConversion *data = MEM_new<NodeImplicitConversion>(__func__);
  STRNCPY(data->type_idname, "NodeSocketColor");
  node->storage = data;
}

static bool node_implicit_conversion_poll_instance(const bNode *node,
                                                   const bNodeTree *nodetree,
                                                   const char **r_disabled_hint)
{
  const auto &data = *static_cast<NodeImplicitConversion *>(node->storage);
  bke::bNodeSocketType *socket_type = bke::node_socket_type_find(data.type_idname);
  if (!socket_type) {
    if (r_disabled_hint) {
      *r_disabled_hint = "Socket type not found";
    }
    return false;
  }
  bke::bNodeTreeType &tree_type = *nodetree->typeinfo;
  if (tree_type.valid_socket_type && !tree_type.valid_socket_type(&tree_type, socket_type)) {
    if (r_disabled_hint) {
      *r_disabled_hint = "Socket type not supported";
    }
    return false;
  }
  return true;
}

static void node_implicit_conversion_geo_exec(nodes::GeoNodeExecParams params)
{
  auto input_value = params.extract_input<bke::SocketValueVariant>("Value");
  params.set_output("Value", std::move(input_value));
}

class ImplicitConversionOperation : public compositor::NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    using namespace compositor;
    const Result &input = this->get_input("Value");
    Result &output = this->get_result("Value");
    output.share_data(input);
  }
};

static compositor::NodeOperation *node_implicit_conversion_compositor_operation(
    compositor::Context &context, const bNode &node)
{
  return new ImplicitConversionOperation(context, node);
}

void register_node_type_implicit_conversion()
{
  /* Adapt type node is used for all tree types, needs dynamic allocation. */
  bke::bNodeType *ntype = MEM_new<bke::bNodeType>("Implicit Conversion node type");
  ntype->free_self = [](bke::bNodeType *type) { MEM_delete(type); };

  bke::node_type_base(*ntype, "NodeImplicitConversion");
  ntype->ui_name = "Implicit Conversion";
  ntype->ui_description = "Implicitly convert the input value to a fixed socket type";
  ntype->nclass = NODE_CLASS_CONVERTER;
  ntype->declare = node_implicit_conversion_declare;
  ntype->labelfunc = node_implicit_conversion_label;
  ntype->draw_buttons = node_implicit_conversion_layout;
  ntype->initfunc = node_implicit_conversion_init;
  node_type_storage(
      *ntype, "NodeImplicitConversion", node_free_standard_storage, node_copy_standard_storage);
  ntype->poll_instance = node_implicit_conversion_poll_instance;
  ntype->geometry_node_execute = node_implicit_conversion_geo_exec;
  ntype->get_compositor_operation = node_implicit_conversion_compositor_operation;

  bke::node_register_type(*ntype);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node #GROUP_INPUT / #GROUP_OUTPUT
 * \{ */

bNodeSocket *node_group_input_find_socket(bNode *node, const StringRef identifier)
{
  for (bNodeSocket &sock : node->outputs) {
    if (sock.identifier == identifier) {
      return &sock;
    }
  }
  return nullptr;
}

namespace nodes {

static void group_input_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *node_tree = b.tree_or_null();
  if (node_tree == nullptr) {
    return;
  }
  node_tree->tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
    switch (eNodeTreeInterfaceItemType(item.item_type)) {
      case NODE_INTERFACE_SOCKET: {
        const bNodeTreeInterfaceSocket &socket =
            node_interface::get_item_as<bNodeTreeInterfaceSocket>(item);
        if (socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
          /* Trying to use the evaluated structure type for the group output node introduces a
           * "dependency cycle" between this and the structure type inferencing which uses node
           * declarations. The compromise is to not use the proper structure type in the group
           * input/output declarations and instead use a special case for the choice of socket
           * shapes. */
          build_interface_socket_declaration(*node_tree, socket, std::nullopt, SOCK_OUT, b);
        }
        break;
      }
      case NODE_INTERFACE_PANEL: {
        break;
      }
    }
    return true;
  });
  b.add_output<decl::Extend>("", "__extend__");
}

static void group_output_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *node_tree = b.tree_or_null();
  if (node_tree == nullptr) {
    return;
  }
  node_tree->tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
    switch (eNodeTreeInterfaceItemType(item.item_type)) {
      case NODE_INTERFACE_SOCKET: {
        const bNodeTreeInterfaceSocket &socket =
            node_interface::get_item_as<bNodeTreeInterfaceSocket>(item);
        if (socket.flag & NODE_INTERFACE_SOCKET_OUTPUT) {
          build_interface_socket_declaration(*node_tree, socket, std::nullopt, SOCK_IN, b);
        }
        break;
      }
      case NODE_INTERFACE_PANEL: {
        break;
      }
    }
    return true;
  });
  b.add_input<decl::Extend>("", "__extend__");
}

static bool group_input_insert_link(bke::NodeInsertLinkParams &params)
{
  BLI_assert(params.link.tonode != &params.node);
  BLI_assert(params.link.tosock->in_out == SOCK_IN);
  if (!StringRef(params.link.fromsock->identifier).startswith("__extend__")) {
    return true;
  }
  if (StringRef(params.link.tosock->identifier).startswith("__extend__")) {
    /* Don't connect to other "extend" sockets. */
    return false;
  }
  const bNodeTreeInterfaceSocket *io_socket = node_interface::add_interface_socket_from_node(
      params.ntree, *params.link.tonode, *params.link.tosock);
  if (!io_socket) {
    return false;
  }
  update_node_declaration_and_sockets(params.ntree, params.node);
  params.link.fromsock = node_group_input_find_socket(&params.node, io_socket->identifier);
  return true;
}

static bool group_output_insert_link(bke::NodeInsertLinkParams &params)
{
  BLI_assert(params.link.fromnode != &params.node);
  BLI_assert(params.link.fromsock->in_out == SOCK_OUT);
  if (!StringRef(params.link.tosock->identifier).startswith("__extend__")) {
    return true;
  }
  if (StringRef(params.link.fromsock->identifier).startswith("__extend__")) {
    /* Don't connect to other "extend" sockets. */
    return false;
  }
  const bNodeTreeInterfaceSocket *io_socket = node_interface::add_interface_socket_from_node(
      params.ntree, *params.link.fromnode, *params.link.fromsock);
  if (!io_socket) {
    return false;
  }
  update_node_declaration_and_sockets(params.ntree, params.node);
  params.link.tosock = node_group_output_find_socket(&params.node, io_socket->identifier);
  return true;
}

static void node_group_input_layout(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  ed::space_node::node_tree_interface_draw(*C, layout, *id_cast<bNodeTree *>(ptr->owner_id));
}

static void node_group_output_layout(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  ed::space_node::node_tree_interface_draw(*C, layout, *id_cast<bNodeTree *>(ptr->owner_id));
}

}  // namespace nodes

static void node_group_input_extra_info(nodes::NodeExtraInfoParams &parameters)
{
  if (parameters.tree.type != NTREE_COMPOSIT) {
    return;
  }

  SpaceNode *space_node = CTX_wm_space_node(&parameters.C);
  if (space_node->edittree != space_node->nodetree) {
    return;
  }

  if (space_node->node_tree_sub_type != SNODE_COMPOSITOR_SEQUENCER) {
    return;
  }

  Span<const bNodeSocket *> group_inputs = parameters.node.output_sockets().drop_back(1);
  int color_count = 0;
  int float_count = 0;
  int other_count = 0;
  for (const bNodeSocket *input : group_inputs) {
    if (input->type == SOCK_RGBA) {
      color_count++;
    }
    else if (input->type == SOCK_FLOAT) {
      float_count++;
    }
    else {
      other_count++;
    }
  }

  if (color_count > 2) {
    nodes::NodeExtraInfoRow row;
    row.text = IFACE_("Unsupported Inputs");
    row.icon = ICON_WARNING_LARGE;
    row.tooltip = TIP_("Sequencer supports up to two Image inputs, the rest will return zero");
    parameters.rows.append(std::move(row));
  }
  if (float_count > 1) {
    nodes::NodeExtraInfoRow row;
    row.text = IFACE_("Unsupported Inputs");
    row.icon = ICON_WARNING_LARGE;
    row.tooltip = TIP_("Sequencer supports one Float input, the rest will return zero");
    parameters.rows.append(std::move(row));
  }
  if (other_count > 0) {
    nodes::NodeExtraInfoRow row;
    row.text = IFACE_("Unsupported Inputs");
    row.icon = ICON_WARNING_LARGE;
    row.tooltip = TIP_(
        "Sequencer supports only Color and Float inputs, the rest will return zero");
    parameters.rows.append(std::move(row));
  }
}

void register_node_type_group_input()
{
  /* used for all tree types, needs dynamic allocation */
  bke::bNodeType *ntype = MEM_new<bke::bNodeType>("node type");
  ntype->free_self = [](bke::bNodeType *type) { MEM_delete(type); };

  bke::node_type_base(*ntype, "NodeGroupInput", NODE_GROUP_INPUT);
  ntype->ui_name = "Group Input";
  ntype->ui_description =
      "Expose connected data from inside a node group as inputs to its interface";
  ntype->enum_name_legacy = "GROUP_INPUT";
  ntype->nclass = NODE_CLASS_INTERFACE;
  bke::node_type_size(*ntype, 140, 80, 400);
  ntype->declare = nodes::group_input_declare;
  ntype->insert_link = nodes::group_input_insert_link;
  ntype->get_extra_info = node_group_input_extra_info;
  ntype->draw_buttons_ex = nodes::node_group_input_layout;
  ntype->no_muting = true;

  bke::node_register_type(*ntype);
}

bNodeSocket *node_group_output_find_socket(bNode *node, const StringRef identifier)
{
  for (bNodeSocket &sock : node->inputs) {
    if (sock.identifier == identifier) {
      return &sock;
    }
  }
  return nullptr;
}

static void get_compositor_group_output_extra_info(blender::nodes::NodeExtraInfoParams &parameters)
{
  if (parameters.tree.type != NTREE_COMPOSIT) {
    return;
  }

  SpaceNode *space_node = CTX_wm_space_node(&parameters.C);
  if (space_node->edittree != space_node->nodetree) {
    return;
  }

  blender::Span<const bNodeSocket *> group_outputs = parameters.node.input_sockets().drop_back(1);
  if (group_outputs.is_empty()) {
    blender::nodes::NodeExtraInfoRow row;
    row.text = IFACE_("No Output");
    row.icon = ICON_ERROR;
    row.tooltip = TIP_("Node group must have a Color output socket");
    parameters.rows.append(std::move(row));
    return;
  }

  if (group_outputs[0]->type != SOCK_RGBA) {
    blender::nodes::NodeExtraInfoRow row;
    row.text = IFACE_("Wrong Output Type");
    row.icon = ICON_ERROR;
    row.tooltip = TIP_("Node group's first output must be a color output");
    parameters.rows.append(std::move(row));
    return;
  }

  if (group_outputs.size() > 1) {
    blender::nodes::NodeExtraInfoRow row;
    row.text = IFACE_("Ignored Outputs");
    row.icon = ICON_WARNING_LARGE;
    row.tooltip = TIP_("Only the first output is considered while the rest are ignored");
    parameters.rows.append(std::move(row));
    return;
  }
}

static void node_group_output_extra_info(nodes::NodeExtraInfoParams &params)
{
  get_compositor_group_output_extra_info(params);
  const Span<const bNode *> group_output_nodes = params.tree.nodes_by_type("NodeGroupOutput");
  if (group_output_nodes.size() <= 1) {
    return;
  }
  if (params.node.flag & NODE_DO_OUTPUT) {
    return;
  }
  nodes::NodeExtraInfoRow row;
  row.text = IFACE_("Unused Output");
  row.icon = ICON_ERROR;
  row.tooltip = TIP_("There are multiple group output nodes and this one is not active");
  params.rows.append(std::move(row));
}

void register_node_type_group_output()
{
  /* used for all tree types, needs dynamic allocation */
  bke::bNodeType *ntype = MEM_new<bke::bNodeType>("node type");
  ntype->free_self = [](bke::bNodeType *type) { MEM_delete(type); };

  bke::node_type_base(*ntype, "NodeGroupOutput", NODE_GROUP_OUTPUT);
  ntype->ui_name = "Group Output";
  ntype->ui_description = "Output data from inside of a node group";
  ntype->enum_name_legacy = "GROUP_OUTPUT";
  ntype->nclass = NODE_CLASS_INTERFACE;
  bke::node_type_size(*ntype, 140, 80, 400);
  ntype->declare = nodes::group_output_declare;
  ntype->insert_link = nodes::group_output_insert_link;
  ntype->get_extra_info = node_group_output_extra_info;
  ntype->draw_buttons_ex = nodes::node_group_output_layout;

  ntype->no_muting = true;

  bke::node_register_type(*ntype);
}

/** \} */

}  // namespace blender
