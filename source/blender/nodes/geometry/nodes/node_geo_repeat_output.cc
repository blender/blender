/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_compute_contexts.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_add_node_search.hh"
#include "NOD_geometry.hh"
#include "NOD_socket.hh"

#include "BLI_string_utils.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static std::unique_ptr<SocketDeclaration> socket_declaration_for_repeat_item(
    const NodeRepeatItem &item, const eNodeSocketInOut in_out, const int corresponding_input = -1)
{
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);

  std::unique_ptr<SocketDeclaration> decl;

  auto handle_field_decl = [&](SocketDeclaration &decl) {
    if (in_out == SOCK_IN) {
      decl.input_field_type = InputSocketFieldType::IsSupported;
    }
    else {
      decl.output_field_dependency = OutputFieldDependency::ForPartiallyDependentField(
          {corresponding_input});
    }
  };

  switch (socket_type) {
    case SOCK_FLOAT:
      decl = std::make_unique<decl::Float>();
      handle_field_decl(*decl);
      break;
    case SOCK_VECTOR:
      decl = std::make_unique<decl::Vector>();
      handle_field_decl(*decl);
      break;
    case SOCK_RGBA:
      decl = std::make_unique<decl::Color>();
      handle_field_decl(*decl);
      break;
    case SOCK_BOOLEAN:
      decl = std::make_unique<decl::Bool>();
      handle_field_decl(*decl);
      break;
    case SOCK_ROTATION:
      decl = std::make_unique<decl::Rotation>();
      handle_field_decl(*decl);
      break;
    case SOCK_INT:
      decl = std::make_unique<decl::Int>();
      handle_field_decl(*decl);
      break;
    case SOCK_STRING:
      decl = std::make_unique<decl::String>();
      break;
    case SOCK_GEOMETRY:
      decl = std::make_unique<decl::Geometry>();
      break;
    case SOCK_OBJECT:
      decl = std::make_unique<decl::Object>();
      break;
    case SOCK_IMAGE:
      decl = std::make_unique<decl::Image>();
      break;
    case SOCK_COLLECTION:
      decl = std::make_unique<decl::Collection>();
      break;
    case SOCK_MATERIAL:
      decl = std::make_unique<decl::Material>();
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  decl->name = item.name ? item.name : "";
  decl->identifier = item.identifier_str();
  decl->in_out = in_out;
  return decl;
}

void socket_declarations_for_repeat_items(const Span<NodeRepeatItem> items,
                                          NodeDeclaration &r_declaration)
{
  for (const int i : items.index_range()) {
    const NodeRepeatItem &item = items[i];
    r_declaration.inputs.append(socket_declaration_for_repeat_item(item, SOCK_IN));
    r_declaration.outputs.append(
        socket_declaration_for_repeat_item(item, SOCK_OUT, r_declaration.inputs.size() - 1));
  }
  r_declaration.inputs.append(decl::create_extend_declaration(SOCK_IN));
  r_declaration.outputs.append(decl::create_extend_declaration(SOCK_OUT));
}
}  // namespace blender::nodes
namespace blender::nodes::node_geo_repeat_output_cc {

NODE_STORAGE_FUNCS(NodeGeometryRepeatOutput);

static void node_declare_dynamic(const bNodeTree & /*node_tree*/,
                                 const bNode &node,
                                 NodeDeclaration &r_declaration)
{
  const NodeGeometryRepeatOutput &storage = node_storage(node);
  socket_declarations_for_repeat_items(storage.items_span(), r_declaration);
}

static void search_node_add_ops(GatherAddNodeSearchParams &params)
{
  AddNodeItem item;
  item.ui_name = IFACE_("Repeat Zone");
  item.description = TIP_("Add new repeat input and output nodes to the node tree");
  item.add_fn = [](const bContext &C, bNodeTree &node_tree, float2 cursor) {
    bNode *input = nodeAddNode(&C, &node_tree, "GeometryNodeRepeatInput");
    bNode *output = nodeAddNode(&C, &node_tree, "GeometryNodeRepeatOutput");
    static_cast<NodeGeometryRepeatInput *>(input->storage)->output_node_id = output->identifier;

    NodeRepeatItem &item = node_storage(*output).items[0];

    update_node_declaration_and_sockets(node_tree, *input);
    update_node_declaration_and_sockets(node_tree, *output);

    const std::string identifier = item.identifier_str();
    nodeAddLink(&node_tree,
                input,
                nodeFindSocket(input, SOCK_OUT, identifier.c_str()),
                output,
                nodeFindSocket(output, SOCK_IN, identifier.c_str()));

    input->locx = cursor.x / UI_SCALE_FAC - 150;
    input->locy = cursor.y / UI_SCALE_FAC + 20;
    output->locx = cursor.x / UI_SCALE_FAC + 150;
    output->locy = cursor.y / UI_SCALE_FAC + 20;

    return Vector<bNode *>({input, output});
  };
  params.add_item(std::move(item));
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryRepeatOutput *data = MEM_cnew<NodeGeometryRepeatOutput>(__func__);

  data->next_identifier = 0;

  data->items = MEM_cnew_array<NodeRepeatItem>(1, __func__);
  data->items[0].name = BLI_strdup(DATA_("Geometry"));
  data->items[0].socket_type = SOCK_GEOMETRY;
  data->items[0].identifier = data->next_identifier++;
  data->items_num = 1;

  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  NodeGeometryRepeatOutput &storage = node_storage(*node);
  for (NodeRepeatItem &item : storage.items_span()) {
    MEM_SAFE_FREE(item.name);
  }
  MEM_SAFE_FREE(storage.items);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryRepeatOutput &src_storage = node_storage(*src_node);
  NodeGeometryRepeatOutput *dst_storage = MEM_cnew<NodeGeometryRepeatOutput>(__func__);

  dst_storage->items = MEM_cnew_array<NodeRepeatItem>(src_storage.items_num, __func__);
  dst_storage->items_num = src_storage.items_num;
  dst_storage->active_index = src_storage.active_index;
  dst_storage->next_identifier = src_storage.next_identifier;
  for (const int i : IndexRange(src_storage.items_num)) {
    if (char *name = src_storage.items[i].name) {
      dst_storage->items[i].identifier = src_storage.items[i].identifier;
      dst_storage->items[i].name = BLI_strdup(name);
      dst_storage->items[i].socket_type = src_storage.items[i].socket_type;
    }
  }

  dst_node->storage = dst_storage;
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  NodeGeometryRepeatOutput &storage = node_storage(*node);
  if (link->tonode == node) {
    if (link->tosock->identifier == StringRef("__extend__")) {
      if (const NodeRepeatItem *item = storage.add_item(link->fromsock->name,
                                                        eNodeSocketDatatype(link->fromsock->type)))
      {
        update_node_declaration_and_sockets(*ntree, *node);
        link->tosock = nodeFindSocket(node, SOCK_IN, item->identifier_str().c_str());
        return true;
      }
    }
    else {
      return true;
    }
  }
  if (link->fromnode == node) {
    if (link->fromsock->identifier == StringRef("__extend__")) {
      if (const NodeRepeatItem *item = storage.add_item(link->tosock->name,
                                                        eNodeSocketDatatype(link->tosock->type)))
      {
        update_node_declaration_and_sockets(*ntree, *node);
        link->fromsock = nodeFindSocket(node, SOCK_OUT, item->identifier_str().c_str());
        return true;
      }
    }
    else {
      return true;
    }
  }
  return false;
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_REPEAT_OUTPUT, "Repeat Output", NODE_CLASS_INTERFACE);
  ntype.initfunc = node_init;
  ntype.declare_dynamic = node_declare_dynamic;
  ntype.gather_add_node_search_ops = search_node_add_ops;
  ntype.insert_link = node_insert_link;
  node_type_storage(&ntype, "NodeGeometryRepeatOutput", node_free_storage, node_copy_storage);
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_repeat_output_cc

blender::Span<NodeRepeatItem> NodeGeometryRepeatOutput::items_span() const
{
  return blender::Span<NodeRepeatItem>(items, items_num);
}

blender::MutableSpan<NodeRepeatItem> NodeGeometryRepeatOutput::items_span()
{
  return blender::MutableSpan<NodeRepeatItem>(items, items_num);
}

bool NodeRepeatItem::supports_type(const eNodeSocketDatatype type)
{
  return ELEM(type,
              SOCK_FLOAT,
              SOCK_VECTOR,
              SOCK_RGBA,
              SOCK_BOOLEAN,
              SOCK_ROTATION,
              SOCK_INT,
              SOCK_STRING,
              SOCK_GEOMETRY,
              SOCK_OBJECT,
              SOCK_MATERIAL,
              SOCK_IMAGE,
              SOCK_COLLECTION);
}

std::string NodeRepeatItem::identifier_str() const
{
  return "Item_" + std::to_string(this->identifier);
}

NodeRepeatItem *NodeGeometryRepeatOutput::add_item(const char *name,
                                                   const eNodeSocketDatatype type)
{
  if (!NodeRepeatItem::supports_type(type)) {
    return nullptr;
  }
  const int insert_index = this->items_num;
  NodeRepeatItem *old_items = this->items;

  this->items = MEM_cnew_array<NodeRepeatItem>(this->items_num + 1, __func__);
  std::copy_n(old_items, insert_index, this->items);
  NodeRepeatItem &new_item = this->items[insert_index];
  std::copy_n(old_items + insert_index + 1,
              this->items_num - insert_index,
              this->items + insert_index + 1);

  new_item.identifier = this->next_identifier++;
  this->set_item_name(new_item, name);
  new_item.socket_type = type;

  this->items_num++;
  MEM_SAFE_FREE(old_items);
  return &new_item;
}

void NodeGeometryRepeatOutput::set_item_name(NodeRepeatItem &item, const char *name)
{
  char unique_name[MAX_NAME + 4];
  STRNCPY(unique_name, name);

  struct Args {
    NodeGeometryRepeatOutput *storage;
    const NodeRepeatItem *item;
  } args = {this, &item};

  const char *default_name = nodeStaticSocketLabel(item.socket_type, 0);
  BLI_uniquename_cb(
      [](void *arg, const char *name) {
        const Args &args = *static_cast<Args *>(arg);
        for (const NodeRepeatItem &item : args.storage->items_span()) {
          if (&item != args.item) {
            if (STREQ(item.name, name)) {
              return true;
            }
          }
        }
        return false;
      },
      &args,
      default_name,
      '.',
      unique_name,
      ARRAY_SIZE(unique_name));

  MEM_SAFE_FREE(item.name);
  item.name = BLI_strdup(unique_name);
}
