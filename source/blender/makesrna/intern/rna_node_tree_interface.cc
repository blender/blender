/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_node_tree_interface_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "rna_internal.hh"

#include "WM_types.hh"

const EnumPropertyItem rna_enum_node_tree_interface_item_type_items[] = {
    {NODE_INTERFACE_SOCKET, "SOCKET", 0, "Socket", ""},
    {NODE_INTERFACE_PANEL, "PANEL", 0, "Panel", ""},
    {0, nullptr, 0, nullptr, nullptr}};

static const EnumPropertyItem node_tree_interface_socket_in_out_items[] = {
    {NODE_INTERFACE_SOCKET_INPUT, "INPUT", 0, "Input", "Generate a input node socket"},
    {NODE_INTERFACE_SOCKET_OUTPUT, "OUTPUT", 0, "Output", "Generate a output node socket"},
    {0, nullptr, 0, nullptr, nullptr}};

const EnumPropertyItem rna_enum_node_socket_structure_type_items[] = {
    {NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO,
     "AUTO",
     0,
     "Auto",
     "Automatically detect a good structure type based on how the socket is used"},
    {NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC,
     "DYNAMIC",
     0,
     "Dynamic",
     "Socket can work with different kinds of structures"},
    {NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_FIELD, "FIELD", 0, "Field", "Socket expects a field"},
    {NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_GRID, "GRID", 0, "Grid", "Socket expects a grid"},
    {NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_LIST, "LIST", 0, "List", "Socket expects a list"},
    {NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_SINGLE,
     "SINGLE",
     0,
     "Single",
     "Socket expects a single value"},
    {0, nullptr, 0, nullptr, nullptr}};

static const EnumPropertyItem node_default_input_items[] = {
    {NODE_DEFAULT_INPUT_VALUE, "VALUE", 0, "Default Value", "The node socket's default value"},
    {NODE_DEFAULT_INPUT_INDEX_FIELD, "INDEX", 0, "Index", "The index from the context"},
    {NODE_DEFAULT_INPUT_ID_INDEX_FIELD,
     "ID_OR_INDEX",
     0,
     "ID or Index",
     "The \"id\" attribute if available, otherwise the index"},
    {NODE_DEFAULT_INPUT_NORMAL_FIELD, "NORMAL", 0, "Normal", "The geometry's normal direction"},
    {NODE_DEFAULT_INPUT_POSITION_FIELD,
     "POSITION",
     0,
     "Position",
     "The position from the context"},
    {NODE_DEFAULT_INPUT_INSTANCE_TRANSFORM_FIELD,
     "INSTANCE_TRANSFORM",
     0,
     "Instance Transform",
     "Transformation of each instance from the geometry context"},
    {NODE_DEFAULT_INPUT_HANDLE_LEFT_FIELD,
     "HANDLE_LEFT",
     0,
     "Left Handle",
     "The left Bézier control point handle from the context"},
    {NODE_DEFAULT_INPUT_HANDLE_RIGHT_FIELD,
     "HANDLE_RIGHT",
     0,
     "Right Handle",
     "The right Bézier control point handle from the context"},
    {0, nullptr, 0, nullptr, nullptr}};

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLI_string_ref.hh"

#  include "BKE_attribute.hh"
#  include "BKE_main_invariants.hh"
#  include "BKE_node.hh"
#  include "BKE_node_enum.hh"
#  include "BKE_node_runtime.hh"
#  include "BKE_node_tree_interface.hh"
#  include "BKE_node_tree_update.hh"

#  include "BLI_set.hh"

#  include "BLT_translation.hh"

#  include "NOD_node_declaration.hh"
#  include "NOD_rna_define.hh"
#  include "NOD_socket.hh"

#  include "DNA_material_types.h"

#  include "WM_api.hh"

#  include "ED_node.hh"

/* Internal RNA function declarations, used to invoke registered callbacks. */
extern FunctionRNA rna_NodeTreeInterfaceSocket_draw_func;
extern FunctionRNA rna_NodeTreeInterfaceSocket_init_socket_func;
extern FunctionRNA rna_NodeTreeInterfaceSocket_from_socket_func;

namespace node_interface = blender::bke::node_interface;

static void rna_NodeTreeInterfaceItem_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ntree) {
    /* This can happen because of the dummy socket in #rna_NodeTreeInterfaceSocket_register. */
    return;
  }
  ntree->tree_interface.tag_item_property_changed();
  BKE_main_ensure_invariants(*bmain, ntree->id);
}

static StructRNA *rna_NodeTreeInterfaceItem_refine(PointerRNA *ptr)
{
  bNodeTreeInterfaceItem *item = static_cast<bNodeTreeInterfaceItem *>(ptr->data);

  switch (NodeTreeInterfaceItemType(item->item_type)) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &socket = node_interface::get_item_as<bNodeTreeInterfaceSocket>(
          *item);
      if (socket.socket_type) {
        blender::bke::bNodeSocketType *socket_typeinfo = blender::bke::node_socket_type_find(
            socket.socket_type);
        if (socket_typeinfo && socket_typeinfo->ext_interface.srna) {
          return socket_typeinfo->ext_interface.srna;
        }
      }
      return &RNA_NodeTreeInterfaceSocket;
    }
    case NODE_INTERFACE_PANEL:
      return &RNA_NodeTreeInterfacePanel;
    default:
      return &RNA_NodeTreeInterfaceItem;
  }
}

static std::optional<std::string> rna_NodeTreeInterfaceItem_path(const PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  const bNodeTreeInterfaceItem *item = static_cast<const bNodeTreeInterfaceItem *>(ptr->data);
  if (!ntree->runtime) {
    return std::nullopt;
  }

  ntree->ensure_interface_cache();
  for (const int index : ntree->interface_items().index_range()) {
    if (ntree->interface_items()[index] == item) {
      return fmt::format("interface.items_tree[{}]", index);
    }
  }
  return std::nullopt;
}

static PointerRNA rna_NodeTreeInterfaceItem_parent_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  const bNodeTreeInterfaceItem *item = static_cast<const bNodeTreeInterfaceItem *>(ptr->data);
  bNodeTreeInterfacePanel *parent = ntree->tree_interface.find_item_parent(*item, true);
  PointerRNA result = RNA_pointer_create_discrete(&ntree->id, &RNA_NodeTreeInterfacePanel, parent);
  return result;
}

static int rna_NodeTreeInterfaceItem_position_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  const bNodeTreeInterfaceItem *item = static_cast<const bNodeTreeInterfaceItem *>(ptr->data);
  return ntree->tree_interface.find_item_position(*item);
}

static int rna_NodeTreeInterfaceItem_index_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  const bNodeTreeInterfaceItem *item = static_cast<const bNodeTreeInterfaceItem *>(ptr->data);
  return ntree->tree_interface.find_item_index(*item);
}

static bool rna_NodeTreeInterfaceSocket_unregister(Main * /*bmain*/, StructRNA *type)
{
  blender::bke::bNodeSocketType *st = static_cast<blender::bke::bNodeSocketType *>(
      RNA_struct_blender_type_get(type));
  if (!st) {
    return false;
  }

  RNA_struct_free_extension(type, &st->ext_interface);

  RNA_struct_free(&BLENDER_RNA, type);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  return true;
}

static void rna_NodeTreeInterfaceSocket_draw_builtin(ID *id,
                                                     bNodeTreeInterfaceSocket *interface_socket,
                                                     bContext *C,
                                                     uiLayout *layout)
{
  blender::bke::bNodeSocketType *typeinfo = interface_socket->socket_typeinfo();
  if (typeinfo && typeinfo->interface_draw) {
    typeinfo->interface_draw(id, interface_socket, C, layout);
  }
}

static void rna_NodeTreeInterfaceSocket_draw_custom(ID *id,
                                                    bNodeTreeInterfaceSocket *interface_socket,
                                                    bContext *C,
                                                    uiLayout *layout)
{
  blender::bke::bNodeSocketType *typeinfo = blender::bke::node_socket_type_find(
      interface_socket->socket_type);
  if (typeinfo == nullptr) {
    return;
  }

  PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_NodeTreeInterfaceSocket, interface_socket);

  FunctionRNA *func = &rna_NodeTreeInterfaceSocket_draw_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  typeinfo->ext_interface.call(C, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_NodeTreeInterfaceSocket_init_socket_builtin(
    ID *id,
    bNodeTreeInterfaceSocket *interface_socket,
    bNode *node,
    bNodeSocket *socket,
    const char *data_path)
{
  blender::bke::bNodeSocketType *typeinfo = interface_socket->socket_typeinfo();
  if (typeinfo && typeinfo->interface_draw) {
    typeinfo->interface_init_socket(id, interface_socket, node, socket, data_path);
  }
}

static void rna_NodeTreeInterfaceSocket_init_socket_custom(
    ID *id,
    const bNodeTreeInterfaceSocket *interface_socket,
    bNode *node,
    bNodeSocket *socket,
    const blender::StringRefNull data_path)
{
  blender::bke::bNodeSocketType *typeinfo = blender::bke::node_socket_type_find(
      interface_socket->socket_type);
  if (typeinfo == nullptr) {
    return;
  }

  PointerRNA ptr = RNA_pointer_create_discrete(
      id, &RNA_NodeTreeInterfaceSocket, const_cast<bNodeTreeInterfaceSocket *>(interface_socket));

  FunctionRNA *func = &rna_NodeTreeInterfaceSocket_init_socket_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "node", node);
  RNA_parameter_set_lookup(&list, "socket", socket);
  RNA_parameter_set_lookup(&list, "data_path", &data_path);
  typeinfo->ext_interface.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_NodeTreeInterfaceSocket_from_socket_builtin(
    ID *id, bNodeTreeInterfaceSocket *interface_socket, bNode *node, bNodeSocket *socket)
{
  blender::bke::bNodeSocketType *typeinfo = interface_socket->socket_typeinfo();
  if (typeinfo && typeinfo->interface_draw) {
    typeinfo->interface_from_socket(id, interface_socket, node, socket);
  }
}

static void rna_NodeTreeInterfaceSocket_from_socket_custom(
    ID *id,
    bNodeTreeInterfaceSocket *interface_socket,
    const bNode *node,
    const bNodeSocket *socket)
{
  blender::bke::bNodeSocketType *typeinfo = blender::bke::node_socket_type_find(
      interface_socket->socket_type);
  if (typeinfo == nullptr) {
    return;
  }

  PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_NodeTreeInterfaceSocket, interface_socket);

  FunctionRNA *func = &rna_NodeTreeInterfaceSocket_from_socket_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "node", node);
  RNA_parameter_set_lookup(&list, "socket", socket);
  typeinfo->ext_interface.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static StructRNA *rna_NodeTreeInterfaceSocket_register(Main * /*bmain*/,
                                                       ReportList * /*reports*/,
                                                       void *data,
                                                       const char *identifier,
                                                       StructValidateFunc validate,
                                                       StructCallbackFunc call,
                                                       StructFreeFunc free)
{
  bNodeTreeInterfaceSocket dummy_socket = {};
  /* Set #item_type so that refining the type ends up with RNA_NodeTreeInterfaceSocket. */
  dummy_socket.item.item_type = NODE_INTERFACE_SOCKET;

  PointerRNA dummy_socket_ptr = RNA_pointer_create_discrete(
      nullptr, &RNA_NodeTreeInterfaceSocket, &dummy_socket);

  /* Validate the python class. */
  bool have_function[3];
  if (validate(&dummy_socket_ptr, data, have_function) != 0) {
    return nullptr;
  }

  /* Check if we have registered this socket type before. */
  blender::bke::bNodeSocketType *st = blender::bke::node_socket_type_find(
      dummy_socket.socket_type);
  if (st) {
    /* Socket type registered before. */
  }
  else {
    /* Create a new node socket type. */
    st = MEM_new<blender::bke::bNodeSocketType>(__func__);
    st->idname = dummy_socket.socket_type;

    blender::bke::node_register_socket_type(*st);
  }

  st->free_self = [](blender::bke::bNodeSocketType *type) { MEM_delete(type); };

  /* if RNA type is already registered, unregister first */
  if (st->ext_interface.srna) {
    StructRNA *srna = st->ext_interface.srna;
    RNA_struct_free_extension(srna, &st->ext_interface);
    RNA_struct_free(&BLENDER_RNA, srna);
  }
  st->ext_interface.srna = RNA_def_struct_ptr(
      &BLENDER_RNA, identifier, &RNA_NodeTreeInterfaceSocket);
  st->ext_interface.data = data;
  st->ext_interface.call = call;
  st->ext_interface.free = free;
  RNA_struct_blender_type_set(st->ext_interface.srna, st);

  st->interface_draw = (have_function[0]) ? rna_NodeTreeInterfaceSocket_draw_custom : nullptr;
  st->interface_init_socket = (have_function[1]) ? rna_NodeTreeInterfaceSocket_init_socket_custom :
                                                   nullptr;
  st->interface_from_socket = (have_function[2]) ? rna_NodeTreeInterfaceSocket_from_socket_custom :
                                                   nullptr;

  /* Cleanup local dummy type. */
  MEM_SAFE_FREE(dummy_socket.socket_type);

  /* Update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return st->ext_interface.srna;
}

static IDProperty **rna_NodeTreeInterfaceSocket_idprops(PointerRNA *ptr)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  return &socket->properties;
}

static void rna_NodeTreeInterfaceSocket_identifier_get(PointerRNA *ptr, char *value)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  strcpy(value, socket->identifier);
}

static int rna_NodeTreeInterfaceSocket_identifier_length(PointerRNA *ptr)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  return strlen(socket->identifier);
}

static int rna_NodeTreeInterfaceSocket_socket_type_get(PointerRNA *ptr)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  return rna_node_socket_idname_to_enum(socket->socket_type);
}

static void rna_NodeTreeInterfaceSocket_socket_type_set(PointerRNA *ptr, int value)
{
  blender::bke::bNodeSocketType *typeinfo = rna_node_socket_type_from_enum(value);

  if (typeinfo) {
    bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
    socket->set_socket_type(typeinfo->idname);
  }
}

static bool is_socket_type_supported(blender::bke::bNodeTreeType *ntreetype,
                                     blender::bke::bNodeSocketType *socket_type)
{
  /* Check if the node tree supports the socket type. */
  if (ntreetype->valid_socket_type && !ntreetype->valid_socket_type(ntreetype, socket_type)) {
    return false;
  }

  /* Only basic socket types are supported. Custom sockets don't have a base type. */
  if (socket_type->type != SOCK_CUSTOM) {
    blender::bke::bNodeSocketType *base_socket_type = blender::bke::node_socket_type_find_static(
        socket_type->type, PROP_NONE);
    BLI_assert(base_socket_type != nullptr);
    if (socket_type != base_socket_type) {
      return false;
    }
  }

  return true;
}

static blender::bke::bNodeSocketType *find_supported_socket_type(
    blender::bke::bNodeTreeType *ntree_type)
{
  for (blender::bke::bNodeSocketType *socket_type : blender::bke::node_socket_types_get()) {
    if (is_socket_type_supported(ntree_type, socket_type)) {
      return socket_type;
    }
  }
  return nullptr;
}

static bool rna_NodeTreeInterfaceSocket_socket_type_poll(
    void *userdata, blender::bke::bNodeSocketType *socket_type)
{
  blender::bke::bNodeTreeType *ntreetype = static_cast<blender::bke::bNodeTreeType *>(userdata);
  return is_socket_type_supported(ntreetype, socket_type);
}

static const EnumPropertyItem *rna_NodeTreeInterfaceSocket_socket_type_itemf(
    bContext * /*C*/, PointerRNA *ptr, PropertyRNA * /*prop*/, bool *r_free)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);

  if (!ntree) {
    return rna_enum_dummy_NULL_items;
  }

  return rna_node_socket_type_itemf(
      ntree->typeinfo, rna_NodeTreeInterfaceSocket_socket_type_poll, r_free);
}

/**
 * Also control the structure type when setting the "Is Single" status. To be removed when the
 * structure type feature is moved out of experimental.
 */
static void rna_NodeTreeInterfaceSocket_force_non_field_set(PointerRNA *ptr, const bool value)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  SET_FLAG_FROM_TEST(socket->flag, value, NODE_INTERFACE_SOCKET_SINGLE_VALUE_ONLY_LEGACY);
  socket->structure_type = value ? NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_SINGLE :
                                   NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO;
}

const EnumPropertyItem *rna_NodeSocket_structure_type_item_filter(
    const bNodeTree *ntree, const eNodeSocketDatatype socket_type, bool *r_free)
{
  if (!ntree) {
    return rna_enum_dummy_NULL_items;
  }
  const bool is_geometry_nodes = ntree->type == NTREE_GEOMETRY;

  const bool supports_fields = is_geometry_nodes &&
                               blender::nodes::socket_type_supports_fields(socket_type);
  const bool supports_grids = is_geometry_nodes &&
                              blender::nodes::socket_type_supports_grids(socket_type);
  const bool supports_lists = is_geometry_nodes && supports_fields;

  *r_free = true;
  EnumPropertyItem *items = nullptr;
  int items_count = 0;

  for (const EnumPropertyItem *item = rna_enum_node_socket_structure_type_items; item->identifier;
       item++)
  {
    switch (NodeSocketInterfaceStructureType(item->value)) {
      case NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_SINGLE:
      case NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO: {
        RNA_enum_item_add(&items, &items_count, item);
        break;
      }
      case NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC: {
        if (supports_fields || supports_grids) {
          RNA_enum_item_add(&items, &items_count, item);
        }
        break;
      }
      case NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_FIELD: {
        if (supports_fields) {
          RNA_enum_item_add(&items, &items_count, item);
        }
        break;
      }
      case NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_GRID: {
        if (supports_grids) {
          RNA_enum_item_add(&items, &items_count, item);
        }
        break;
      }
      case NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_LIST: {
        if (U.experimental.use_geometry_nodes_lists) {
          if (supports_lists) {
            RNA_enum_item_add(&items, &items_count, item);
          }
        }
        break;
      }
    }
  }
  RNA_enum_item_end(&items, &items_count);
  return items;
}

static const EnumPropertyItem *rna_NodeTreeInterfaceSocket_structure_type_itemf(
    bContext * /*C*/, PointerRNA *ptr, PropertyRNA * /*prop*/, bool *r_free)
{
  const bNodeTree *ntree = reinterpret_cast<const bNodeTree *>(ptr->owner_id);
  const bNodeTreeInterfaceSocket *socket = static_cast<const bNodeTreeInterfaceSocket *>(
      ptr->data);
  const eNodeSocketDatatype socket_type = socket->socket_typeinfo()->type;
  return rna_NodeSocket_structure_type_item_filter(ntree, socket_type, r_free);
}

static const EnumPropertyItem *rna_NodeTreeInterfaceSocket_default_input_itemf(
    bContext * /*C*/, PointerRNA *ptr, PropertyRNA * /*prop*/, bool *r_free)
{
  const bNodeTree *ntree = reinterpret_cast<const bNodeTree *>(ptr->owner_id);
  const bNodeTreeInterfaceSocket *socket = static_cast<const bNodeTreeInterfaceSocket *>(
      ptr->data);
  if (!ntree) {
    return rna_enum_dummy_NULL_items;
  }
  const blender::bke::bNodeSocketType *stype = socket->socket_typeinfo();
  if (!stype) {
    return rna_enum_dummy_NULL_items;
  }

  *r_free = true;
  EnumPropertyItem *items = nullptr;
  int items_count = 0;

  for (const EnumPropertyItem *item = node_default_input_items; item->identifier; item++) {
    if (item->value == NODE_DEFAULT_INPUT_VALUE) {
      RNA_enum_item_add(&items, &items_count, item);
    }
    else if (ntree->type == NTREE_GEOMETRY) {
      if (blender::nodes::socket_type_supports_default_input_type(
              *stype, NodeDefaultInputType(item->value)))
      {
        RNA_enum_item_add(&items, &items_count, item);
      }
    }
  }

  RNA_enum_item_end(&items, &items_count);
  return items;
}

static const EnumPropertyItem *rna_NodeTreeInterfaceSocket_attribute_domain_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  using namespace blender;
  EnumPropertyItem *item_array = nullptr;
  int items_len = 0;

  for (const EnumPropertyItem *item = rna_enum_attribute_domain_items; item->identifier != nullptr;
       item++)
  {
    RNA_enum_item_add(&item_array, &items_len, item);
  }
  RNA_enum_item_end(&item_array, &items_len);

  *r_free = true;
  return item_array;
}

static PointerRNA rna_NodeTreeInterfaceItems_active_get(PointerRNA *ptr)
{
  bNodeTreeInterface *interface = static_cast<bNodeTreeInterface *>(ptr->data);
  PointerRNA ptr_result = RNA_pointer_create_discrete(
      ptr->owner_id, &RNA_NodeTreeInterfaceItem, interface->active_item());
  return ptr_result;
}

static void rna_NodeTreeInterfaceItems_active_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList * /*reports*/)
{
  bNodeTreeInterface *interface = static_cast<bNodeTreeInterface *>(ptr->data);
  bNodeTreeInterfaceItem *item = static_cast<bNodeTreeInterfaceItem *>(value.data);
  interface->active_item_set(item);
}

static bNodeTreeInterfaceSocket *rna_NodeTreeInterfaceItems_new_socket(
    ID *id,
    bNodeTreeInterface *interface,
    Main *bmain,
    ReportList *reports,
    const char *name,
    const char *description,
    int in_out,
    int socket_type_enum,
    bNodeTreeInterfacePanel *parent)
{
  if (parent != nullptr && !interface->find_item(parent->item)) {
    BKE_report(reports, RPT_ERROR_INVALID_INPUT, "Parent is not part of the interface");
    return nullptr;
  }
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  blender::bke::bNodeSocketType *typeinfo = rna_node_socket_type_from_enum(socket_type_enum);
  if (typeinfo == nullptr) {
    BKE_report(reports, RPT_ERROR_INVALID_INPUT, "Unknown socket type");
    return nullptr;
  }

  /* If data type is unsupported try to find a valid type. */
  if (!is_socket_type_supported(ntree->typeinfo, typeinfo)) {
    typeinfo = find_supported_socket_type(ntree->typeinfo);
    if (typeinfo == nullptr) {
      BKE_report(reports, RPT_ERROR, "Could not find supported socket type");
      return nullptr;
    }
  }
  const blender::StringRef socket_type = typeinfo->idname;
  NodeTreeInterfaceSocketFlag flag = NodeTreeInterfaceSocketFlag(in_out);
  bNodeTreeInterfaceSocket *socket = interface->add_socket(
      name, description, socket_type, flag, parent);

  if (socket == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create socket");
  }
  else {
    BKE_main_ensure_invariants(*bmain, ntree->id);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }

  return socket;
}

static bNodeTreeInterfacePanel *rna_NodeTreeInterfaceItems_new_panel(ID *id,
                                                                     bNodeTreeInterface *interface,
                                                                     Main *bmain,
                                                                     ReportList *reports,
                                                                     const char *name,
                                                                     const char *description,
                                                                     bool default_closed)
{
  NodeTreeInterfacePanelFlag flag = NodeTreeInterfacePanelFlag(0);
  SET_FLAG_FROM_TEST(flag, default_closed, NODE_INTERFACE_PANEL_DEFAULT_CLOSED);

  bNodeTreeInterfacePanel *panel = interface->add_panel(
      name ? name : "", description ? description : "", flag, nullptr);

  if (panel == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create panel");
  }
  else {
    bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
    BKE_main_ensure_invariants(*bmain, ntree->id);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }

  return panel;
}

static bNodeTreeInterfaceItem *rna_NodeTreeInterfaceItems_copy_to_parent(
    ID *id,
    bNodeTreeInterface *interface,
    Main *bmain,
    ReportList *reports,
    bNodeTreeInterfaceItem *item,
    bNodeTreeInterfacePanel *parent)
{
  if (parent != nullptr) {
    if (!interface->find_item(parent->item)) {
      BKE_report(reports, RPT_ERROR_INVALID_INPUT, "Parent is not part of the interface");
      return nullptr;
    }
  }

  if (parent == nullptr) {
    parent = &interface->root_panel;
  }
  const int index = parent->items().as_span().first_index_try(item);
  if (!parent->items().index_range().contains(index)) {
    return nullptr;
  }

  bNodeTreeInterfaceItem *item_copy = interface->insert_item_copy(*item, parent, index + 1);

  if (item_copy == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to copy item");
  }
  else {
    bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
    BKE_main_ensure_invariants(*bmain, ntree->id);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }

  return item_copy;
}

static bNodeTreeInterfaceItem *rna_NodeTreeInterfaceItems_copy(ID *id,
                                                               bNodeTreeInterface *interface,
                                                               Main *bmain,
                                                               ReportList *reports,
                                                               bNodeTreeInterfaceItem *item)
{
  /* Copy to same parent as the item. */
  bNodeTreeInterfacePanel *parent = interface->find_item_parent(*item);
  return rna_NodeTreeInterfaceItems_copy_to_parent(id, interface, bmain, reports, item, parent);
}

static void rna_NodeTreeInterfaceItems_remove(ID *id,
                                              bNodeTreeInterface *interface,
                                              Main *bmain,
                                              bNodeTreeInterfaceItem *item,
                                              bool move_content_to_parent)
{
  interface->remove_item(*item, move_content_to_parent);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTreeInterfaceItems_clear(ID *id, bNodeTreeInterface *interface, Main *bmain)
{
  interface->clear_items();

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTreeInterfaceItems_move(ID *id,
                                            bNodeTreeInterface *interface,
                                            Main *bmain,
                                            bNodeTreeInterfaceItem *item,
                                            int to_position)
{
  interface->move_item(*item, to_position);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTreeInterfaceItems_move_to_parent(ID *id,
                                                      bNodeTreeInterface *interface,
                                                      Main *bmain,
                                                      ReportList * /*reports*/,
                                                      bNodeTreeInterfaceItem *item,
                                                      bNodeTreeInterfacePanel *parent,
                                                      int to_position)
{
  interface->move_item_to_parent(*item, parent, to_position);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

/* ******** Node Socket Subtypes ******** */

static const EnumPropertyItem *rna_subtype_filter_itemf(const blender::Set<int> &subtypes,
                                                        bool *r_free)
{
  if (subtypes.is_empty()) {
    return rna_enum_dummy_NULL_items;
  }

  EnumPropertyItem *items = nullptr;
  int items_count = 0;
  for (const EnumPropertyItem *item = rna_enum_property_subtype_items; item->name != nullptr;
       item++)
  {
    if (subtypes.contains(item->value)) {
      RNA_enum_item_add(&items, &items_count, item);
    }
  }

  if (items_count == 0) {
    return rna_enum_dummy_NULL_items;
  }

  RNA_enum_item_end(&items, &items_count);
  *r_free = true;
  return items;
}

static const EnumPropertyItem *rna_NodeTreeInterfaceSocketFloat_subtype_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  return rna_subtype_filter_itemf({PROP_PERCENTAGE,
                                   PROP_FACTOR,
                                   PROP_ANGLE,
                                   PROP_TIME,
                                   PROP_TIME_ABSOLUTE,
                                   PROP_DISTANCE,
                                   PROP_WAVELENGTH,
                                   PROP_COLOR_TEMPERATURE,
                                   PROP_FREQUENCY,
                                   PROP_NONE},
                                  r_free);
}

void rna_NodeTreeInterfaceSocketFloat_default_value_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  bNodeSocketValueFloat *dval = static_cast<bNodeSocketValueFloat *>(socket->socket_data);
  blender::bke::bNodeSocketType *socket_typeinfo = blender::bke::node_socket_type_find(
      socket->socket_type);
  int subtype = socket_typeinfo ? socket_typeinfo->subtype : PROP_NONE;

  if (dval->max < dval->min) {
    dval->max = dval->min;
  }

  *min = (subtype == PROP_UNSIGNED ? 0.0f : -FLT_MAX);
  *max = FLT_MAX;
  *softmin = dval->min;
  *softmax = dval->max;
}

static const EnumPropertyItem *rna_NodeTreeInterfaceSocketInt_subtype_itemf(bContext * /*C*/,
                                                                            PointerRNA * /*ptr*/,
                                                                            PropertyRNA * /*prop*/,
                                                                            bool *r_free)
{
  return rna_subtype_filter_itemf({PROP_PERCENTAGE, PROP_FACTOR, PROP_NONE}, r_free);
}

void rna_NodeTreeInterfaceSocketInt_default_value_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  bNodeSocketValueInt *dval = static_cast<bNodeSocketValueInt *>(socket->socket_data);
  blender::bke::bNodeSocketType *socket_typeinfo = blender::bke::node_socket_type_find(
      socket->socket_type);
  int subtype = socket_typeinfo ? socket_typeinfo->subtype : PROP_NONE;

  if (dval->max < dval->min) {
    dval->max = dval->min;
  }

  *min = (subtype == PROP_UNSIGNED ? 0 : INT_MIN);
  *max = INT_MAX;
  *softmin = dval->min;
  *softmax = dval->max;
}

static const EnumPropertyItem *rna_NodeTreeInterfaceSocketVector_subtype_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  return rna_subtype_filter_itemf({PROP_FACTOR,
                                   PROP_PERCENTAGE,
                                   PROP_TRANSLATION,
                                   PROP_DIRECTION,
                                   PROP_VELOCITY,
                                   PROP_ACCELERATION,
                                   PROP_EULER,
                                   PROP_XYZ,
                                   PROP_NONE},
                                  r_free);
}

void rna_NodeTreeInterfaceSocketVector_default_value_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  bNodeSocketValueVector *dval = static_cast<bNodeSocketValueVector *>(socket->socket_data);

  if (dval->max < dval->min) {
    dval->max = dval->min;
  }

  *min = -FLT_MAX;
  *max = FLT_MAX;
  *softmin = dval->min;
  *softmax = dval->max;
}

static const EnumPropertyItem *rna_NodeTreeInterfaceSocketString_subtype_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  return rna_subtype_filter_itemf({PROP_FILEPATH, PROP_NONE}, r_free);
}

/* If the dimensions of the vector socket changed, we need to update the socket type, since each
 * dimensions value has its own sub-type. */
static void rna_NodeTreeInterfaceSocketVector_dimensions_update(Main *bmain,
                                                                Scene *scene,
                                                                PointerRNA *ptr)
{

  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);

  /* Store a copy of the existing default value since it will be freed when setting the socket type
   * below. */
  const bNodeSocketValueVector default_value = *static_cast<bNodeSocketValueVector *>(
      socket->socket_data);

  const blender::StringRefNull socket_idname = *blender::bke::node_static_socket_type(
      SOCK_VECTOR, default_value.subtype, default_value.dimensions);

  socket->set_socket_type(socket_idname);

  /* Restore existing default value. */
  *static_cast<bNodeSocketValueVector *>(socket->socket_data) = default_value;

  rna_NodeTreeInterfaceItem_update(bmain, scene, ptr);
}

static bool rna_NodeTreeInterfaceSocketMaterial_default_value_poll(PointerRNA * /*ptr*/,
                                                                   PointerRNA value)
{
  /* Do not show grease pencil materials for now. */
  Material *ma = static_cast<Material *>(value.data);
  return ma->gp_style == nullptr;
}

static void rna_NodeTreeInterface_items_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ntree->runtime) {
    return;
  }

  ntree->ensure_interface_cache();
  rna_iterator_array_begin(iter,
                           ptr,
                           const_cast<bNodeTreeInterfaceItem **>(ntree->interface_items().data()),
                           sizeof(bNodeTreeInterfaceItem *),
                           ntree->interface_items().size(),
                           false,
                           nullptr);
}

static int rna_NodeTreeInterface_items_length(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ntree->runtime) {
    return 0;
  }

  ntree->ensure_interface_cache();
  return ntree->interface_items().size();
}

static bool rna_NodeTreeInterface_items_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ntree->runtime) {
    return false;
  }

  ntree->ensure_interface_cache();
  if (!ntree->interface_items().index_range().contains(index)) {
    return false;
  }

  rna_pointer_create_with_ancestors(
      *ptr, &RNA_NodeTreeInterfaceItem, ntree->interface_items()[index], *r_ptr);
  return true;
}

static bool rna_NodeTreeInterface_items_lookup_string(PointerRNA *ptr,
                                                      const char *key,
                                                      PointerRNA *r_ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ntree->runtime) {
    return false;
  }

  ntree->ensure_interface_cache();
  for (bNodeTreeInterfaceItem *item : ntree->interface_items()) {
    switch (NodeTreeInterfaceItemType(item->item_type)) {
      case NODE_INTERFACE_SOCKET: {
        bNodeTreeInterfaceSocket *socket = reinterpret_cast<bNodeTreeInterfaceSocket *>(item);
        if (STREQ(socket->identifier, key)) {
          rna_pointer_create_with_ancestors(*ptr, &RNA_NodeTreeInterfaceSocket, socket, *r_ptr);
          return true;
        }
        break;
      }
      default:
        break;
    }
  }
  for (bNodeTreeInterfaceItem *item : ntree->interface_items()) {
    switch (NodeTreeInterfaceItemType(item->item_type)) {
      case NODE_INTERFACE_SOCKET: {
        bNodeTreeInterfaceSocket *socket = reinterpret_cast<bNodeTreeInterfaceSocket *>(item);
        if (STREQ(socket->name, key)) {
          rna_pointer_create_with_ancestors(*ptr, &RNA_NodeTreeInterfaceSocket, socket, *r_ptr);
          return true;
        }
        break;
      }
      case NODE_INTERFACE_PANEL: {
        bNodeTreeInterfacePanel *panel = reinterpret_cast<bNodeTreeInterfacePanel *>(item);
        if (STREQ(panel->name, key)) {
          rna_pointer_create_with_ancestors(*ptr, &RNA_NodeTreeInterfacePanel, panel, *r_ptr);
          return true;
        }
        break;
      }
    }
  }
  return false;
}

const EnumPropertyItem *RNA_node_tree_interface_socket_menu_itemf(bContext * /*C*/,
                                                                  PointerRNA *ptr,
                                                                  PropertyRNA * /*prop*/,
                                                                  bool *r_free)
{
  const bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  if (!socket) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }
  const bNodeSocketValueMenu *data = static_cast<bNodeSocketValueMenu *>(socket->socket_data);
  if (!data->enum_items) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }
  return RNA_node_enum_definition_itemf(*data->enum_items, r_free);
}

#else

static void rna_def_node_interface_item(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeTreeInterfaceItem", nullptr);
  RNA_def_struct_ui_text(srna, "Node Tree Interface Item", "Item in a node tree interface");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceItem");
  RNA_def_struct_refine_func(srna, "rna_NodeTreeInterfaceItem_refine");
  RNA_def_struct_path_func(srna, "rna_NodeTreeInterfaceItem_path");

  prop = RNA_def_property(srna, "item_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "item_type");
  RNA_def_property_enum_items(prop, rna_enum_node_tree_interface_item_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Item Type", "Type of interface item");

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NodeTreeInterfacePanel");
  RNA_def_property_pointer_funcs(
      prop, "rna_NodeTreeInterfaceItem_parent_get", nullptr, nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop, "Parent", "Panel that contains the item");

  prop = RNA_def_property(srna, "position", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop, "rna_NodeTreeInterfaceItem_position_get", nullptr, nullptr);
  RNA_def_property_range(prop, -1, INT_MAX);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Position", "Position of the item in its parent panel");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop, "rna_NodeTreeInterfaceItem_index_get", nullptr, nullptr);
  RNA_def_property_range(prop, -1, INT_MAX);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Index", "Global index of the item among all items in the interface");
}

static void rna_def_node_interface_socket(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "NodeTreeInterfaceSocket", "NodeTreeInterfaceItem");
  RNA_def_struct_ui_text(srna, "Node Tree Interface Socket", "Declaration of a node socket");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");
  RNA_def_struct_register_funcs(srna,
                                "rna_NodeTreeInterfaceSocket_register",
                                "rna_NodeTreeInterfaceSocket_unregister",
                                nullptr);
  RNA_def_struct_system_idprops_func(srna, "rna_NodeTreeInterfaceSocket_idprops");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Socket name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeTreeInterfaceSocket_identifier_get",
                                "rna_NodeTreeInterfaceSocket_identifier_length",
                                nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Identifier", "Unique identifier for mapping sockets");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "description");
  RNA_def_property_ui_text(prop, "Description", "Socket description");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "socket_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeTreeInterfaceSocket_socket_type_get",
                              "rna_NodeTreeInterfaceSocket_socket_type_set",
                              "rna_NodeTreeInterfaceSocket_socket_type_itemf");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Socket Type", "Type of the socket generated by this interface item");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "in_out", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, node_tree_interface_socket_in_out_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Input/Output Type", "Input or output socket type");

  prop = RNA_def_property(srna, "hide_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_HIDE_VALUE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Hide Value", "Hide the socket input value even when the socket is not connected");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "hide_in_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Hide in Modifier",
                           "Don't show the input value in the geometry nodes modifier interface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "force_non_field", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "flag", NODE_INTERFACE_SOCKET_SINGLE_VALUE_ONLY_LEGACY);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_NodeTreeInterfaceSocket_force_non_field_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Single Value",
      "Only allow single value inputs rather than field.\nDeprecated. Will be remove in 5.0.");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "is_inspect_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_INSPECT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Is Inspect Output",
                           "Take link out of node group to connect to root tree output node");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "is_panel_toggle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_PANEL_TOGGLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Is Panel Toggle",
                           "This socket is meant to be used as the toggle in its panel header");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "layer_selection_field", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_LAYER_SELECTION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Layer Selection", "Take Grease Pencil Layer or Layer Group as selection field");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "menu_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_MENU_EXPANDED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Menu Expanded", "Draw the menu socket as an expanded drop-down menu");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "optional_label", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_OPTIONAL_LABEL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Optional Label",
      "Indicate that the label of this socket is not necessary to understand its meaning. This "
      "may result in the label being skipped in some cases");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "attribute_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocket_attribute_domain_itemf");
  RNA_def_property_ui_text(
      prop,
      "Attribute Domain",
      "Attribute domain used by the geometry nodes modifier to create an attribute output");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "default_attribute_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "default_attribute_name");
  RNA_def_property_ui_text(prop,
                           "Default Attribute",
                           "The attribute name used by default when the node group is used by a "
                           "geometry nodes modifier");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "structure_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_structure_type_items);
  RNA_def_property_ui_text(
      prop,
      "Structure Type",
      "What kind of higher order types are expected to flow through this socket");
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocket_structure_type_itemf");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "default_input", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, node_default_input_items);
  RNA_def_property_ui_text(
      prop,
      "Default Input",
      "Input to use when the socket is unconnected. Requires \"Hide Value\".");
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocket_default_input_itemf");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  /* Registered properties and functions for custom socket types. */
  prop = RNA_def_property(srna, "bl_socket_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "socket_type");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Socket Type Name", "Name of the socket type");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  RNA_def_function_ui_description(func, "Draw properties of the socket interface");
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "init_socket", nullptr);
  RNA_def_function_ui_description(func, "Initialize a node socket instance");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the socket to initialize");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Socket to initialize");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(
      func, "data_path", nullptr, 0, "Data Path", "Path to specialized socket data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "from_socket", nullptr);
  RNA_def_function_ui_description(func, "Setup template parameters from an existing socket");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the original socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Original socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

static void rna_def_node_interface_panel(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeTreeInterfacePanel", "NodeTreeInterfaceItem");
  RNA_def_struct_ui_text(srna, "Node Tree Interface Item", "Declaration of a node panel");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfacePanel");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Panel name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "description");
  RNA_def_property_ui_text(prop, "Description", "Panel description");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "default_closed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_PANEL_DEFAULT_CLOSED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Default Closed", "Panel is closed by default on new nodes");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "interface_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items_array", "items_num");
  RNA_def_property_struct_type(prop, "NodeTreeInterfaceItem");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Items", "Items in the node panel");

  prop = RNA_def_property(srna, "persistent_uid", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "identifier");
  RNA_def_property_ui_text(
      prop, "Persistent Identifier", "Unique identifier for this panel within this node tree");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_node_tree_interface_items_api(StructRNA *srna)
{
  PropertyRNA *prop;
  PropertyRNA *parm;
  FunctionRNA *func;

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NodeTreeInterfaceItem");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_NodeTreeInterfaceItems_active_get",
                                 "rna_NodeTreeInterfaceItems_active_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_ui_text(prop, "Active", "Active item");
  RNA_def_property_update(prop, NC_NODE, nullptr);

  func = RNA_def_function(srna, "new_socket", "rna_NodeTreeInterfaceItems_new_socket");
  RNA_def_function_ui_description(func, "Add a new socket to the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "Name of the socket");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_string(func, "description", nullptr, 0, "Description", "Description of the socket");
  RNA_def_enum(func,
               "in_out",
               node_tree_interface_socket_in_out_items,
               NODE_INTERFACE_SOCKET_INPUT,
               "Input/Output Type",
               "Create an input or output socket");
  parm = RNA_def_enum(func,
                      "socket_type",
                      rna_enum_dummy_DEFAULT_items,
                      0,
                      "Socket Type",
                      "Type of socket generated on nodes");
  /* NOTE: itemf callback works for the function parameter, it does not require a data pointer. */
  RNA_def_property_enum_funcs(
      parm, nullptr, nullptr, "rna_NodeTreeInterfaceSocket_socket_type_itemf");
  RNA_def_pointer(
      func, "parent", "NodeTreeInterfacePanel", "Parent", "Panel to add the socket in");
  /* return value */
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceSocket", "Socket", "New socket");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_panel", "rna_NodeTreeInterfaceItems_new_panel");
  RNA_def_function_ui_description(func, "Add a new panel to the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "Name of the new panel");
  RNA_def_string(func, "description", nullptr, 0, "Description", "Description of the panel");
  RNA_def_boolean(
      func, "default_closed", false, "Default Closed", "Panel is closed by default on new nodes");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfacePanel", "Panel", "New panel");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "copy", "rna_NodeTreeInterfaceItems_copy");
  RNA_def_function_ui_description(func, "Add a copy of an item to the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceItem", "Item", "Item to copy");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  /* return value */
  parm = RNA_def_pointer(
      func, "item_copy", "NodeTreeInterfaceItem", "Item Copy", "Copy of the item");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_NodeTreeInterfaceItems_remove");
  RNA_def_function_ui_description(func, "Remove an item from the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceItem", "Item", "The item to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_boolean(
      func,
      "move_content_to_parent",
      true,
      "Move Content",
      "If the item is a panel, move the contents to the parent instead of deleting it");

  func = RNA_def_function(srna, "clear", "rna_NodeTreeInterfaceItems_clear");
  RNA_def_function_ui_description(func, "Remove all items from the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);

  func = RNA_def_function(srna, "move", "rna_NodeTreeInterfaceItems_move");
  RNA_def_function_ui_description(func, "Move an item to another position");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceItem", "Item", "The item to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "to_position",
                     -1,
                     0,
                     INT_MAX,
                     "To Position",
                     "Target position for the item in its current panel",
                     0,
                     10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "move_to_parent", "rna_NodeTreeInterfaceItems_move_to_parent");
  RNA_def_function_ui_description(func, "Move an item to a new panel and/or position.");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceItem", "Item", "The item to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "parent", "NodeTreeInterfacePanel", "Parent", "New parent of the item");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "to_position",
                     -1,
                     0,
                     INT_MAX,
                     "To Position",
                     "Target position for the item in the new parent panel",
                     0,
                     10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_node_tree_interface(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeTreeInterface", nullptr);
  RNA_def_struct_ui_text(
      srna, "Node Tree Interface", "Declaration of sockets and ui panels of a node group");
  RNA_def_struct_sdna(srna, "bNodeTreeInterface");

  prop = RNA_def_property(srna, "items_tree", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_NodeTreeInterface_items_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_NodeTreeInterface_items_length",
                                    "rna_NodeTreeInterface_items_lookup_int",
                                    "rna_NodeTreeInterface_items_lookup_string",
                                    nullptr);
  RNA_def_property_struct_type(prop, "NodeTreeInterfaceItem");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Items", "Items in the node interface");

  rna_def_node_tree_interface_items_api(srna);
}

void RNA_def_node_tree_interface(BlenderRNA *brna)
{
  rna_def_node_interface_item(brna);
  rna_def_node_interface_socket(brna);
  rna_def_node_interface_panel(brna);
  rna_def_node_tree_interface(brna);

  rna_def_node_socket_interface_subtypes(brna);
}

#endif
