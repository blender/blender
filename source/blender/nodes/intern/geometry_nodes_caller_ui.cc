/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>
#include <sstream>

#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_screen.hh"

#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "DNA_modifier_types.h"
#include "DNA_node_tree_interface_types.h"

#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"

#include "MOD_nodes.hh"
#include "NOD_geometry.hh"
#include "NOD_geometry_nodes_caller_ui.hh"
#include "NOD_geometry_nodes_log.hh"
#include "NOD_socket_usage_inference.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "intern/MOD_ui_common.hh"

namespace blender::nodes {

namespace geo_log = geo_eval_log;

namespace {
struct PanelOpenProperty {
  PointerRNA ptr;
  StringRefNull name;
};

struct SearchInfo {
  geo_log::GeoTreeLog *tree_log = nullptr;
  bNodeTree *tree = nullptr;
  IDProperty *properties = nullptr;
};

struct ModifierSearchData {
  uint32_t object_session_uid;
  char modifier_name[MAX_NAME];
};

struct OperatorSearchData {
  /** Can store this data directly, because it's more persistent than for the modifier. */
  SearchInfo info;
};

struct SocketSearchData {
  std::variant<ModifierSearchData, OperatorSearchData> search_data;
  char socket_identifier[MAX_NAME];
  bool is_output;

  SearchInfo info(const bContext &C) const;
};
/* This class must not have a destructor, since it is used by buttons and freed with #MEM_freeN. */
BLI_STATIC_ASSERT(std::is_trivially_destructible_v<SocketSearchData>, "");

struct DrawGroupInputsContext {
  const bContext &C;
  bNodeTree *tree;
  geo_log::GeoTreeLog *tree_log;
  IDProperty *properties;
  PointerRNA *properties_ptr;
  PointerRNA *bmain_ptr;
  Array<nodes::socket_usage_inference::SocketUsage> input_usages;
  Array<nodes::socket_usage_inference::SocketUsage> output_usages;
  bool use_name_for_ids = false;
  std::function<PanelOpenProperty(const bNodeTreeInterfacePanel &)> panel_open_property_fn;
  std::function<SocketSearchData(const bNodeTreeInterfaceSocket &)> socket_search_data_fn;
  std::function<void(uiLayout &, int icon, const bNodeTreeInterfaceSocket &)>
      draw_attribute_toggle_fn;

  bool input_is_visible(const bNodeTreeInterfaceSocket &socket) const
  {
    return this->input_usages[this->tree->interface_input_index(socket)].is_visible;
  }

  bool input_is_active(const bNodeTreeInterfaceSocket &socket) const
  {
    return this->input_usages[this->tree->interface_input_index(socket)].is_used;
  }
};
}  // namespace

static geo_log::GeoTreeLog *get_root_tree_log(const NodesModifierData &nmd)
{
  if (!nmd.runtime->eval_log) {
    return nullptr;
  }
  bke::ModifierComputeContext compute_context{nullptr, nmd};
  return &nmd.runtime->eval_log->get_tree_log(compute_context.hash());
}

static NodesModifierData *get_modifier_data(Main &bmain,
                                            const wmWindowManager &wm,
                                            const ModifierSearchData &data)
{
  if (ED_screen_animation_playing(&wm)) {
    /* Work around an issue where the attribute search exec function has stale pointers when data
     * is reallocated when evaluating the node tree, causing a crash. This would be solved by
     * allowing the UI search data to own arbitrary memory rather than just referencing it. */
    return nullptr;
  }

  const Object *object = (Object *)BKE_libblock_find_session_uid(
      &bmain, ID_OB, data.object_session_uid);
  if (object == nullptr) {
    return nullptr;
  }
  ModifierData *md = BKE_modifiers_findby_name(object, data.modifier_name);
  if (md == nullptr) {
    return nullptr;
  }
  BLI_assert(md->type == eModifierType_Nodes);
  return reinterpret_cast<NodesModifierData *>(md);
}

SearchInfo SocketSearchData::info(const bContext &C) const
{
  if (const auto *modifier_search_data = std::get_if<ModifierSearchData>(&this->search_data)) {
    const NodesModifierData *nmd = get_modifier_data(
        *CTX_data_main(&C), *CTX_wm_manager(&C), *modifier_search_data);
    if (nmd == nullptr) {
      return {};
    }
    if (nmd->node_group == nullptr) {
      return {};
    }
    geo_log::GeoTreeLog *tree_log = get_root_tree_log(*nmd);
    return {tree_log, nmd->node_group, nmd->settings.properties};
  }
  if (const auto *operator_search_data = std::get_if<OperatorSearchData>(&this->search_data)) {
    return operator_search_data->info;
  }
  return {};
}

static void layer_name_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  const SocketSearchData &data = *static_cast<SocketSearchData *>(arg);
  const SearchInfo info = data.info(*C);
  if (!info.tree || !info.tree_log) {
    return;
  }
  info.tree_log->ensure_layer_names();
  info.tree->ensure_topology_cache();

  Vector<const bNodeSocket *> sockets_to_check;
  for (const bNode *node : info.tree->group_input_nodes()) {
    for (const bNodeSocket *socket : node->output_sockets()) {
      if (socket->type == SOCK_GEOMETRY) {
        sockets_to_check.append(socket);
      }
    }
  }

  Set<StringRef> names;
  Vector<const std::string *> layer_names;
  for (const bNodeSocket *socket : sockets_to_check) {
    const geo_log::ValueLog *value_log = info.tree_log->find_socket_value_log(*socket);
    if (value_log == nullptr) {
      continue;
    }
    if (const auto *geo_log = dynamic_cast<const geo_log::GeometryInfoLog *>(value_log)) {
      if (const std::optional<geo_log::GeometryInfoLog::GreasePencilInfo> &grease_pencil_info =
              geo_log->grease_pencil_info)
      {
        for (const std::string &name : grease_pencil_info->layer_names) {
          if (names.add(name)) {
            layer_names.append(&name);
          }
        }
      }
    }
  }
  BLI_assert(items);
  ui::grease_pencil_layer_search_add_items(str, layer_names.as_span(), *items, is_first);
}

static void layer_name_search_exec_fn(bContext *C, void *data_v, void *item_v)
{
  const SocketSearchData &data = *static_cast<SocketSearchData *>(data_v);
  const std::string *item = static_cast<std::string *>(item_v);
  if (!item) {
    return;
  }
  const SearchInfo info = data.info(*C);
  if (!info.properties) {
    return;
  }

  IDProperty &name_property = *IDP_GetPropertyFromGroup(info.properties, data.socket_identifier);
  IDP_AssignString(&name_property, item->c_str());

  ED_undo_push(C, "Assign Layer Name");
}

static void add_layer_name_search_button(DrawGroupInputsContext &ctx,
                                         uiLayout *layout,
                                         const bNodeTreeInterfaceSocket &socket)
{
  const std::string rna_path = fmt::format("[\"{}\"]", BLI_str_escape(socket.identifier));
  if (!ctx.tree_log) {
    layout->prop(ctx.properties_ptr, rna_path, UI_ITEM_NONE, "", ICON_NONE);
    return;
  }

  layout->use_property_decorate_set(false);

  uiLayout *split = &layout->split(0.4f, false);
  uiLayout *name_row = &split->row(false);
  name_row->alignment_set(ui::LayoutAlign::Right);

  name_row->label(socket.name ? IFACE_(socket.name) : "", ICON_NONE);
  uiLayout *prop_row = &split->row(true);

  uiBlock *block = prop_row->block();
  uiBut *but = uiDefIconTextButR(block,
                                 ButType::SearchMenu,
                                 0,
                                 ICON_OUTLINER_DATA_GP_LAYER,
                                 "",
                                 0,
                                 0,
                                 10 * UI_UNIT_X, /* Dummy value, replaced by layout system. */
                                 UI_UNIT_Y,
                                 ctx.properties_ptr,
                                 rna_path,
                                 0,
                                 StringRef(socket.description));
  UI_but_placeholder_set(but, IFACE_("Layer"));
  layout->label("", ICON_BLANK1);

  const Object *object = ed::object::context_object(&ctx.C);
  BLI_assert(object != nullptr);
  if (object == nullptr) {
    return;
  }

  /* Using a custom free function make the search not work currently. So make sure this data can be
   * freed with MEM_freeN. */
  SocketSearchData *data = static_cast<SocketSearchData *>(
      MEM_mallocN(sizeof(SocketSearchData), __func__));
  *data = ctx.socket_search_data_fn(socket);
  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         layer_name_search_update_fn,
                         data,
                         true,
                         nullptr,
                         layer_name_search_exec_fn,
                         nullptr);
}

static void attribute_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  SocketSearchData &data = *static_cast<SocketSearchData *>(arg);
  const SearchInfo info = data.info(*C);
  if (!info.tree || !info.tree_log) {
    return;
  }
  info.tree_log->ensure_existing_attributes();
  info.tree->ensure_topology_cache();

  Vector<const bNodeSocket *> sockets_to_check;
  if (data.is_output) {
    for (const bNode *node : info.tree->nodes_by_type("NodeGroupOutput")) {
      for (const bNodeSocket *socket : node->input_sockets()) {
        if (socket->type == SOCK_GEOMETRY) {
          sockets_to_check.append(socket);
        }
      }
    }
  }
  else {
    for (const bNode *node : info.tree->group_input_nodes()) {
      for (const bNodeSocket *socket : node->output_sockets()) {
        if (socket->type == SOCK_GEOMETRY) {
          sockets_to_check.append(socket);
        }
      }
    }
  }
  Set<StringRef> names;
  Vector<const geo_log::GeometryAttributeInfo *> attributes;
  for (const bNodeSocket *socket : sockets_to_check) {
    const geo_log::ValueLog *value_log = info.tree_log->find_socket_value_log(*socket);
    if (value_log == nullptr) {
      continue;
    }
    if (const auto *geo_log = dynamic_cast<const geo_log::GeometryInfoLog *>(value_log)) {
      for (const geo_log::GeometryAttributeInfo &attribute : geo_log->attributes) {
        if (names.add(attribute.name)) {
          attributes.append(&attribute);
        }
      }
    }
  }
  ui::attribute_search_add_items(str, data.is_output, attributes.as_span(), items, is_first);
}

static void attribute_search_exec_fn(bContext *C, void *data_v, void *item_v)
{
  if (item_v == nullptr) {
    return;
  }
  SocketSearchData &data = *static_cast<SocketSearchData *>(data_v);
  const auto &item = *static_cast<const geo_log::GeometryAttributeInfo *>(item_v);
  const SearchInfo info = data.info(*C);
  if (!info.properties) {
    return;
  }

  const std::string attribute_prop_name = data.socket_identifier +
                                          nodes::input_attribute_name_suffix;
  IDProperty &name_property = *IDP_GetPropertyFromGroup(info.properties, attribute_prop_name);
  IDP_AssignString(&name_property, item.name.c_str());

  ED_undo_push(C, "Assign Attribute Name");
}

static void add_attribute_search_button(DrawGroupInputsContext &ctx,
                                        uiLayout *layout,
                                        const StringRefNull rna_path_attribute_name,
                                        const bNodeTreeInterfaceSocket &socket)
{
  if (!ctx.tree_log) {
    layout->prop(ctx.properties_ptr, rna_path_attribute_name, UI_ITEM_NONE, "", ICON_NONE);
    return;
  }

  uiBlock *block = layout->block();
  uiBut *but = uiDefIconTextButR(block,
                                 ButType::SearchMenu,
                                 0,
                                 ICON_NONE,
                                 "",
                                 0,
                                 0,
                                 10 * UI_UNIT_X, /* Dummy value, replaced by layout system. */
                                 UI_UNIT_Y,
                                 ctx.properties_ptr,
                                 rna_path_attribute_name,
                                 0,
                                 StringRef(socket.description));

  const Object *object = ed::object::context_object(&ctx.C);
  BLI_assert(object != nullptr);
  if (object == nullptr) {
    return;
  }

  /* Using a custom free function make the search not work currently. So make sure this data can be
   * freed with MEM_freeN. */
  SocketSearchData *data = static_cast<SocketSearchData *>(
      MEM_mallocN(sizeof(SocketSearchData), __func__));
  *data = ctx.socket_search_data_fn(socket);
  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         attribute_search_update_fn,
                         data,
                         true,
                         nullptr,
                         attribute_search_exec_fn,
                         nullptr);

  std::string attribute_name = RNA_string_get(ctx.properties_ptr, rna_path_attribute_name.c_str());
  const bool access_allowed = bke::allow_procedural_attribute_access(attribute_name);
  if (!access_allowed) {
    UI_but_flag_enable(but, UI_BUT_REDALERT);
  }
}

static void add_attribute_search_or_value_buttons(
    DrawGroupInputsContext &ctx,
    uiLayout *layout,
    const StringRefNull rna_path,
    const bNodeTreeInterfaceSocket &socket,
    const std::optional<StringRefNull> use_name = std::nullopt)
{
  const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
  const std::string rna_path_attribute_name = fmt::format(
      "[\"{}{}\"]", BLI_str_escape(socket.identifier), nodes::input_attribute_name_suffix);

  /* We're handling this manually in this case. */
  layout->use_property_decorate_set(false);

  uiLayout *split = &layout->split(0.4f, false);
  uiLayout *name_row = &split->row(false);
  name_row->alignment_set(ui::LayoutAlign::Right);

  uiLayout *prop_row = nullptr;

  const std::optional<StringRef> attribute_name = nodes::input_attribute_name_get(ctx.properties,
                                                                                  socket);
  const StringRefNull socket_name = use_name.has_value() ?
                                        (*use_name) :
                                        (socket.name ? IFACE_(socket.name) : "");
  if (type == SOCK_BOOLEAN && !attribute_name) {
    name_row->label("", ICON_NONE);
    prop_row = &split->row(true);
  }
  else {
    prop_row = &layout->row(true);
  }

  if (type == SOCK_BOOLEAN) {
    prop_row->use_property_split_set(false);
    prop_row->alignment_set(ui::LayoutAlign::Expand);
  }

  if (attribute_name) {
    name_row->label(IFACE_(socket_name), ICON_NONE);
    prop_row = &split->row(true);
    add_attribute_search_button(ctx, prop_row, rna_path_attribute_name, socket);
    layout->label("", ICON_BLANK1);
  }
  else {
    const char *name = IFACE_(socket_name.c_str());
    prop_row->prop(ctx.properties_ptr, rna_path, UI_ITEM_NONE, name, ICON_NONE);
    layout->decorator(ctx.properties_ptr, rna_path.c_str(), -1);
  }

  ctx.draw_attribute_toggle_fn(*prop_row, ICON_SPREADSHEET, socket);
}

static NodesModifierPanel *find_panel_by_id(NodesModifierData &nmd, const int id)
{
  for (const int i : IndexRange(nmd.panels_num)) {
    if (nmd.panels[i].id == id) {
      return &nmd.panels[i];
    }
  }
  return nullptr;
}

/* Drawing the properties manually with #uiLayout::prop instead of #uiDefAutoButsRNA allows using
 * the node socket identifier for the property names, since they are unique, but also having
 * the correct label displayed in the UI. */
static void draw_property_for_socket(DrawGroupInputsContext &ctx,
                                     uiLayout *layout,
                                     const bNodeTreeInterfaceSocket &socket,
                                     const std::optional<StringRef> parent_name = std::nullopt)
{
  const StringRefNull identifier = socket.identifier;
  /* The property should be created in #MOD_nodes_update_interface with the correct type. */
  IDProperty *property = IDP_GetPropertyFromGroup_null(ctx.properties, identifier);

  /* IDProperties can be removed with python, so there could be a situation where
   * there isn't a property for a socket or it doesn't have the correct type. */
  if (property == nullptr ||
      !nodes::id_property_type_matches_socket(socket, *property, ctx.use_name_for_ids))
  {
    return;
  }

  const int input_index = ctx.tree->interface_input_index(socket);
  if (!ctx.input_is_visible(socket)) {
    /* The input is not used currently, but it would be used if any menu input is changed.
     * By convention, the input is hidden in this case instead of just grayed out. */
    return;
  }

  uiLayout *row = &layout->row(true);
  row->use_property_decorate_set(true);
  row->active_set(ctx.input_is_active(socket));

  const std::string rna_path = fmt::format("[\"{}\"]", BLI_str_escape(identifier.c_str()));

  /* Use #uiLayout::prop_search to draw pointer properties because #uiLayout::prop would not have
   * enough information about what type of ID to select for editing the values. This is because
   * pointer IDProperties contain no information about their type. */
  const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
  std::string name = socket.name ? IFACE_(socket.name) : "";

  /* If the property has a prefix that's the same string as the name of the panel it's in, remove
   * the prefix so it appears less verbose. */
  if (parent_name.has_value()) {
    const StringRef prefix_to_remove = *parent_name;
    const int prefix_size = prefix_to_remove.size();
    const int pos = name.find(prefix_to_remove);
    if (pos == 0 && name.size() > prefix_size && name[prefix_size] == ' ') {
      name = name.substr(prefix_size + 1);
    }
  }

  switch (type) {
    case SOCK_OBJECT: {
      row->prop_search(
          ctx.properties_ptr, rna_path, ctx.bmain_ptr, "objects", name, ICON_OBJECT_DATA);
      break;
    }
    case SOCK_COLLECTION: {
      row->prop_search(ctx.properties_ptr,
                       rna_path,
                       ctx.bmain_ptr,
                       "collections",
                       name,
                       ICON_OUTLINER_COLLECTION);
      break;
    }
    case SOCK_MATERIAL: {
      row->prop_search(
          ctx.properties_ptr, rna_path, ctx.bmain_ptr, "materials", name, ICON_MATERIAL);
      break;
    }
    case SOCK_TEXTURE: {
      row->prop_search(
          ctx.properties_ptr, rna_path, ctx.bmain_ptr, "textures", name, ICON_TEXTURE);
      break;
    }
    case SOCK_IMAGE: {
      PropertyRNA *prop = RNA_struct_find_property(ctx.properties_ptr, rna_path.c_str());
      if (prop && RNA_property_type(prop) == PROP_POINTER) {
        uiTemplateID(row,
                     &ctx.C,
                     ctx.properties_ptr,
                     rna_path,
                     "image.new",
                     "image.open",
                     nullptr,
                     UI_TEMPLATE_ID_FILTER_ALL,
                     false,
                     name);
      }
      else {
        /* #uiTemplateID only supports pointer properties currently. Node tools store data-block
         * pointers in strings currently. */
        row->prop_search(ctx.properties_ptr, rna_path, ctx.bmain_ptr, "images", name, ICON_IMAGE);
      }
      break;
    }
    case SOCK_MENU: {
      if (socket.flag & NODE_INTERFACE_SOCKET_MENU_EXPANDED) {
        /* Use a single space when the name is empty to work around a bug with expanded enums. Also
         * see #ui_item_enum_expand_exec. */
        row->prop(ctx.properties_ptr,
                  rna_path,
                  UI_ITEM_R_EXPAND,
                  StringRef(name).is_empty() ? " " : name,
                  ICON_NONE);
      }
      else {
        row->prop(ctx.properties_ptr, rna_path, UI_ITEM_NONE, name, ICON_NONE);
      }
      break;
    }
    case SOCK_BOOLEAN: {
      if (is_layer_selection_field(socket)) {
        add_layer_name_search_button(ctx, row, socket);
        /* Adds a spacing at the end of the row. */
        row->label("", ICON_BLANK1);
        break;
      }
      ATTR_FALLTHROUGH;
    }
    default: {
      if (nodes::input_has_attribute_toggle(*ctx.tree, input_index)) {
        add_attribute_search_or_value_buttons(ctx, row, rna_path, socket, name);
      }
      else {
        row->prop(ctx.properties_ptr, rna_path, UI_ITEM_NONE, name, ICON_NONE);
      }
    }
  }
  if (!nodes::input_has_attribute_toggle(*ctx.tree, input_index)) {
    row->label("", ICON_BLANK1);
  }
}

static bool interface_panel_has_socket(DrawGroupInputsContext &ctx,
                                       const bNodeTreeInterfacePanel &interface_panel)
{
  for (const bNodeTreeInterfaceItem *item : interface_panel.items()) {
    if (item->item_type == NODE_INTERFACE_SOCKET) {
      const bNodeTreeInterfaceSocket &socket = *reinterpret_cast<const bNodeTreeInterfaceSocket *>(
          item);
      if (socket.flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER) {
        continue;
      }
      if (socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
        if (ctx.input_is_visible(socket)) {
          return true;
        }
      }
    }
    else if (item->item_type == NODE_INTERFACE_PANEL) {
      if (interface_panel_has_socket(ctx,
                                     *reinterpret_cast<const bNodeTreeInterfacePanel *>(item)))
      {
        return true;
      }
    }
  }
  return false;
}

static bool interface_panel_affects_output(DrawGroupInputsContext &ctx,
                                           const bNodeTreeInterfacePanel &panel)
{
  for (const bNodeTreeInterfaceItem *item : panel.items()) {
    if (item->item_type == NODE_INTERFACE_SOCKET) {
      const auto &socket = *reinterpret_cast<const bNodeTreeInterfaceSocket *>(item);
      if (socket.flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER) {
        continue;
      }
      if (!(socket.flag & NODE_INTERFACE_SOCKET_INPUT)) {
        continue;
      }
      if (ctx.input_is_active(socket)) {
        return true;
      }
    }
    else if (item->item_type == NODE_INTERFACE_PANEL) {
      const auto &sub_interface_panel = *reinterpret_cast<const bNodeTreeInterfacePanel *>(item);
      if (interface_panel_affects_output(ctx, sub_interface_panel)) {
        return true;
      }
    }
  }
  return false;
}

static void draw_interface_panel_content(
    DrawGroupInputsContext &ctx,
    uiLayout *layout,
    const bNodeTreeInterfacePanel &interface_panel,
    const bool skip_first = false,
    const std::optional<StringRef> parent_name = std::nullopt);

static void draw_interface_panel_as_panel(DrawGroupInputsContext &ctx,
                                          uiLayout &layout,
                                          const bNodeTreeInterfacePanel &interface_panel)
{
  if (!interface_panel_has_socket(ctx, interface_panel)) {
    return;
  }
  PanelOpenProperty open_property = ctx.panel_open_property_fn(interface_panel);
  PanelLayout panel_layout;
  bool skip_first = false;
  /* Check if the panel should have a toggle in the header. */
  const bNodeTreeInterfaceSocket *toggle_socket = interface_panel.header_toggle_socket();
  const StringRef panel_name = interface_panel.name;
  if (toggle_socket && !(toggle_socket->flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER)) {
    const StringRefNull identifier = toggle_socket->identifier;
    IDProperty *property = IDP_GetPropertyFromGroup_null(ctx.properties, identifier);
    /* IDProperties can be removed with python, so there could be a situation where
     * there isn't a property for a socket or it doesn't have the correct type. */
    if (property == nullptr ||
        !nodes::id_property_type_matches_socket(*toggle_socket, *property, ctx.use_name_for_ids))
    {
      return;
    }
    const std::string rna_path = fmt::format("[\"{}\"]", BLI_str_escape(identifier.c_str()));
    panel_layout = layout.panel_prop_with_bool_header(&ctx.C,
                                                      &open_property.ptr,
                                                      open_property.name,
                                                      ctx.properties_ptr,
                                                      rna_path,
                                                      IFACE_(panel_name));
    skip_first = true;
  }
  else {
    panel_layout = layout.panel_prop(&ctx.C, &open_property.ptr, open_property.name);
    panel_layout.header->label(IFACE_(panel_name), ICON_NONE);
  }
  if (!interface_panel_affects_output(ctx, interface_panel)) {
    panel_layout.header->active_set(false);
  }
  uiLayoutSetTooltipFunc(
      panel_layout.header,
      [](bContext * /*C*/, void *panel_arg, const StringRef /*tip*/) -> std::string {
        const auto *panel = static_cast<bNodeTreeInterfacePanel *>(panel_arg);
        return StringRef(panel->description);
      },
      const_cast<bNodeTreeInterfacePanel *>(&interface_panel),
      nullptr,
      nullptr);
  if (panel_layout.body) {
    draw_interface_panel_content(ctx, panel_layout.body, interface_panel, skip_first, panel_name);
  }
}

static void draw_interface_panel_content(DrawGroupInputsContext &ctx,
                                         uiLayout *layout,
                                         const bNodeTreeInterfacePanel &interface_panel,
                                         const bool skip_first,
                                         const std::optional<StringRef> parent_name)
{
  for (const bNodeTreeInterfaceItem *item : interface_panel.items().drop_front(skip_first ? 1 : 0))
  {
    switch (NodeTreeInterfaceItemType(item->item_type)) {
      case NODE_INTERFACE_PANEL: {
        const auto &sub_interface_panel = *reinterpret_cast<const bNodeTreeInterfacePanel *>(item);
        draw_interface_panel_as_panel(ctx, *layout, sub_interface_panel);
        break;
      }
      case NODE_INTERFACE_SOCKET: {
        const auto &interface_socket = *reinterpret_cast<const bNodeTreeInterfaceSocket *>(item);
        if (interface_socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
          if (!(interface_socket.flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER)) {
            draw_property_for_socket(ctx, layout, interface_socket, parent_name);
          }
        }
        break;
      }
    }
  }
}

static std::string get_node_warning_panel_name(const int num_errors,
                                               const int num_warnings,
                                               const int num_infos)
{
  fmt::memory_buffer buffer;
  fmt::appender buf = fmt::appender(buffer);
  if (num_errors > 0) {
    fmt::format_to(buf, "{} ({})", IFACE_("Errors"), num_errors);
  }
  if (num_warnings > 0) {
    if (num_errors > 0) {
      fmt::format_to(buf, ", ");
    }
    fmt::format_to(buf, "{} ({})", IFACE_("Warnings"), num_warnings);
  }
  if (num_infos > 0) {
    if (num_errors > 0 || num_warnings > 0) {
      fmt::format_to(buf, ", ");
    }
    fmt::format_to(buf, "{} ({})", IFACE_("Info"), num_infos);
  }
  return std::string(buffer.data(), buffer.size());
}

static void draw_warnings(const bContext *C,
                          const NodesModifierData &nmd,
                          uiLayout *layout,
                          PointerRNA *md_ptr)
{
  if (G.is_rendering) {
    /* Avoid accessing this data while baking in a separate thread. */
    return;
  }
  using namespace geo_log;
  GeoTreeLog *tree_log = get_root_tree_log(nmd);
  if (!tree_log) {
    return;
  }
  tree_log->ensure_node_warnings(*CTX_data_main(C));
  const int warnings_num = tree_log->all_warnings.size();
  if (warnings_num == 0) {
    return;
  }
  Map<NodeWarningType, int> count_by_type;
  for (const NodeWarning &warning : tree_log->all_warnings) {
    count_by_type.lookup_or_add(warning.type, 0)++;
  }
  const int num_errors = count_by_type.lookup_default(NodeWarningType::Error, 0);
  const int num_warnings = count_by_type.lookup_default(NodeWarningType::Warning, 0);
  const int num_infos = count_by_type.lookup_default(NodeWarningType::Info, 0);
  const std::string panel_name = get_node_warning_panel_name(num_errors, num_warnings, num_infos);
  PanelLayout panel = layout->panel_prop(C, md_ptr, "open_warnings_panel");
  panel.header->label(panel_name.c_str(), ICON_NONE);
  if (!panel.body) {
    return;
  }
  Vector<const NodeWarning *> warnings(tree_log->all_warnings.size());
  for (const int i : warnings.index_range()) {
    warnings[i] = &tree_log->all_warnings[i];
  }
  std::sort(warnings.begin(), warnings.end(), [](const NodeWarning *a, const NodeWarning *b) {
    const int severity_a = node_warning_type_severity(a->type);
    const int severity_b = node_warning_type_severity(b->type);
    if (severity_a > severity_b) {
      return true;
    }
    if (severity_a < severity_b) {
      return false;
    }
    return BLI_strcasecmp_natural(a->message.c_str(), b->message.c_str()) < 0;
  });

  uiLayout *col = &panel.body->column(false);
  for (const NodeWarning *warning : warnings) {
    const int icon = node_warning_type_icon(warning->type);
    col->label(RPT_(warning->message), icon);
  }
}

static bool has_output_attribute(const bNodeTree *tree)
{
  if (!tree) {
    return false;
  }
  for (const bNodeTreeInterfaceSocket *interface_socket : tree->interface_outputs()) {
    const bke::bNodeSocketType *typeinfo = interface_socket->socket_typeinfo();
    const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (nodes::socket_type_has_attribute_toggle(type)) {
      return true;
    }
  }
  return false;
}

static void draw_property_for_output_socket(DrawGroupInputsContext &ctx,
                                            uiLayout *layout,
                                            const bNodeTreeInterfaceSocket &socket)
{
  const std::string rna_path_attribute_name = fmt::format(
      "[\"{}{}\"]", BLI_str_escape(socket.identifier), nodes::input_attribute_name_suffix);

  uiLayout *split = &layout->split(0.4f, false);
  uiLayout *name_row = &split->row(false);
  name_row->alignment_set(ui::LayoutAlign::Right);
  name_row->label(socket.name ? socket.name : "", ICON_NONE);

  uiLayout *row = &split->row(true);
  add_attribute_search_button(ctx, row, rna_path_attribute_name, socket);
}

static void draw_output_attributes_panel(DrawGroupInputsContext &ctx, uiLayout *layout)
{
  if (!ctx.tree || !ctx.properties) {
    return;
  }
  const Span<const bNodeTreeInterfaceSocket *> interface_outputs = ctx.tree->interface_outputs();
  for (const int i : interface_outputs.index_range()) {
    const bNodeTreeInterfaceSocket &socket = *interface_outputs[i];
    const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
    const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (!ctx.output_usages[i].is_visible) {
      continue;
    }
    if (nodes::socket_type_has_attribute_toggle(type)) {
      draw_property_for_output_socket(ctx, layout, socket);
    }
  }
}

static void draw_bake_panel(uiLayout *layout, PointerRNA *modifier_ptr)
{
  uiLayout *col = &layout->column(false);
  col->use_property_split_set(true);
  col->use_property_decorate_set(false);
  col->prop(modifier_ptr, "bake_target", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(modifier_ptr, "bake_directory", UI_ITEM_NONE, IFACE_("Bake Path"), ICON_NONE);
}

static void draw_named_attributes_panel(uiLayout *layout, NodesModifierData &nmd)
{
  if (G.is_rendering) {
    /* Avoid accessing this data while baking in a separate thread. */
    return;
  }
  geo_log::GeoTreeLog *tree_log = get_root_tree_log(nmd);
  if (tree_log == nullptr) {
    return;
  }

  tree_log->ensure_used_named_attributes();
  const Map<StringRefNull, geo_log::NamedAttributeUsage> &usage_by_attribute =
      tree_log->used_named_attributes;

  if (usage_by_attribute.is_empty()) {
    layout->label(RPT_("No named attributes used"), ICON_INFO);
    return;
  }

  struct NameWithUsage {
    StringRefNull name;
    geo_log::NamedAttributeUsage usage;
  };

  Vector<NameWithUsage> sorted_used_attribute;
  for (auto &&item : usage_by_attribute.items()) {
    sorted_used_attribute.append({item.key, item.value});
  }
  std::sort(sorted_used_attribute.begin(),
            sorted_used_attribute.end(),
            [](const NameWithUsage &a, const NameWithUsage &b) {
              return BLI_strcasecmp_natural(a.name.c_str(), b.name.c_str()) < 0;
            });

  for (const NameWithUsage &attribute : sorted_used_attribute) {
    const StringRef attribute_name = attribute.name;
    const geo_log::NamedAttributeUsage usage = attribute.usage;

    /* #uiLayoutRowWithHeading doesn't seem to work in this case. */
    uiLayout *split = &layout->split(0.4f, false);

    std::stringstream ss;
    Vector<std::string> usages;
    if (flag_is_set(usage, geo_log::NamedAttributeUsage::Read)) {
      usages.append(IFACE_("Read"));
    }
    if (flag_is_set(usage, geo_log::NamedAttributeUsage::Write)) {
      usages.append(IFACE_("Write"));
    }
    if (flag_is_set(usage, geo_log::NamedAttributeUsage::Remove)) {
      usages.append(CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove"));
    }
    for (const int i : usages.index_range()) {
      ss << usages[i];
      if (i < usages.size() - 1) {
        ss << ", ";
      }
    }

    uiLayout *row = &split->row(false);
    row->alignment_set(ui::LayoutAlign::Right);
    row->active_set(false);
    row->label(ss.str(), ICON_NONE);

    row = &split->row(false);
    row->label(attribute_name, ICON_NONE);
  }
}

static void draw_manage_panel(const bContext *C,
                              uiLayout *layout,
                              PointerRNA *modifier_ptr,
                              NodesModifierData &nmd)
{
  if (uiLayout *panel_layout = layout->panel_prop(
          C, modifier_ptr, "open_bake_panel", IFACE_("Bake")))
  {
    draw_bake_panel(panel_layout, modifier_ptr);
  }
  if (uiLayout *panel_layout = layout->panel_prop(
          C, modifier_ptr, "open_named_attributes_panel", IFACE_("Named Attributes")))
  {
    draw_named_attributes_panel(panel_layout, nmd);
  }
}

void draw_geometry_nodes_modifier_ui(const bContext &C, PointerRNA *modifier_ptr, uiLayout &layout)
{
  Main *bmain = CTX_data_main(&C);
  PointerRNA bmain_ptr = RNA_main_pointer_create(bmain);
  NodesModifierData &nmd = *modifier_ptr->data_as<NodesModifierData>();
  Object &object = *reinterpret_cast<Object *>(modifier_ptr->owner_id);

  DrawGroupInputsContext ctx{C,
                             nmd.node_group,
                             get_root_tree_log(nmd),
                             nmd.settings.properties,
                             modifier_ptr,
                             &bmain_ptr};

  ctx.panel_open_property_fn = [&](const bNodeTreeInterfacePanel &io_panel) -> PanelOpenProperty {
    NodesModifierPanel *panel = find_panel_by_id(nmd, io_panel.identifier);
    PointerRNA panel_ptr = RNA_pointer_create_discrete(
        modifier_ptr->owner_id, &RNA_NodesModifierPanel, panel);
    return {panel_ptr, "is_open"};
  };
  ctx.socket_search_data_fn = [&](const bNodeTreeInterfaceSocket &io_socket) -> SocketSearchData {
    SocketSearchData data{};
    ModifierSearchData &modifier_search_data = data.search_data.emplace<ModifierSearchData>();
    modifier_search_data.object_session_uid = object.id.session_uid;
    STRNCPY_UTF8(modifier_search_data.modifier_name, nmd.modifier.name);
    STRNCPY_UTF8(data.socket_identifier, io_socket.identifier);
    data.is_output = io_socket.flag & NODE_INTERFACE_SOCKET_OUTPUT;
    return data;
  };
  ctx.draw_attribute_toggle_fn =
      [&](uiLayout &layout, const int icon, const bNodeTreeInterfaceSocket &io_socket) {
        PointerRNA props = layout.op("object.geometry_nodes_input_attribute_toggle",
                                     "",
                                     icon,
                                     wm::OpCallContext::InvokeDefault,
                                     UI_ITEM_NONE);
        RNA_string_set(&props, "modifier_name", nmd.modifier.name);
        RNA_string_set(&props, "input_name", io_socket.identifier);
      };

  layout.use_property_split_set(true);
  /* Decorators are added manually for supported properties because the
   * attribute/value toggle requires a manually built layout anyway. */
  layout.use_property_decorate_set(false);

  if (!(nmd.flag & NODES_MODIFIER_HIDE_DATABLOCK_SELECTOR)) {
    const char *newop = (nmd.node_group == nullptr) ? "node.new_geometry_node_group_assign" :
                                                      "object.geometry_node_tree_copy_assign";
    uiTemplateID(&layout, &C, modifier_ptr, "node_group", newop, nullptr, nullptr);
  }

  if (nmd.node_group != nullptr && nmd.settings.properties != nullptr) {
    nmd.runtime->usage_cache.ensure(nmd);
    ctx.input_usages = nmd.runtime->usage_cache.inputs;
    ctx.output_usages = nmd.runtime->usage_cache.outputs;
    draw_interface_panel_content(ctx, &layout, nmd.node_group->tree_interface.root_panel);
  }

  modifier_error_message_draw(&layout, modifier_ptr);

  draw_warnings(&C, nmd, &layout, modifier_ptr);

  if (has_output_attribute(nmd.node_group)) {
    if (uiLayout *panel_layout = layout.panel_prop(
            &C, modifier_ptr, "open_output_attributes_panel", IFACE_("Output Attributes")))
    {
      draw_output_attributes_panel(ctx, panel_layout);
    }
  }

  if ((nmd.flag & NODES_MODIFIER_HIDE_MANAGE_PANEL) == 0) {
    if (uiLayout *panel_layout = layout.panel_prop(
            &C, modifier_ptr, "open_manage_panel", IFACE_("Manage")))
    {
      draw_manage_panel(&C, panel_layout, modifier_ptr, nmd);
    }
  }
}

void draw_geometry_nodes_operator_redo_ui(const bContext &C,
                                          wmOperator &op,
                                          bNodeTree &tree,
                                          geo_eval_log::GeoTreeLog *tree_log)
{
  uiLayout &layout = *op.layout;
  Main &bmain = *CTX_data_main(&C);
  PointerRNA bmain_ptr = RNA_main_pointer_create(&bmain);

  DrawGroupInputsContext ctx{C, &tree, tree_log, op.properties, op.ptr, &bmain_ptr};
  ctx.panel_open_property_fn = [&](const bNodeTreeInterfacePanel &io_panel) -> PanelOpenProperty {
    Panel *root_panel = layout.root_panel();
    LayoutPanelState *state = BKE_panel_layout_panel_state_ensure(
        root_panel,
        "node_operator_panel_" + std::to_string(io_panel.identifier),
        io_panel.flag & NODE_INTERFACE_PANEL_DEFAULT_CLOSED);
    PointerRNA state_ptr = RNA_pointer_create_discrete(nullptr, &RNA_LayoutPanelState, state);
    return {state_ptr, "is_open"};
  };
  ctx.socket_search_data_fn = [&](const bNodeTreeInterfaceSocket &io_socket) -> SocketSearchData {
    SocketSearchData data{};
    OperatorSearchData &operator_search_data = data.search_data.emplace<OperatorSearchData>();
    operator_search_data.info.tree = &tree;
    operator_search_data.info.tree_log = tree_log;
    operator_search_data.info.properties = op.properties;
    STRNCPY_UTF8(data.socket_identifier, io_socket.identifier);
    data.is_output = io_socket.flag & NODE_INTERFACE_SOCKET_OUTPUT;
    return data;
  };
  ctx.draw_attribute_toggle_fn =
      [&](uiLayout &layout, const int icon, const bNodeTreeInterfaceSocket &io_socket) {
        const std::string prop_name = fmt::format(
            "[\"{}{}\"]", BLI_str_escape(io_socket.identifier), nodes::input_use_attribute_suffix);
        layout.prop(op.ptr, prop_name, UI_ITEM_R_ICON_ONLY, "", icon);
      };
  ctx.use_name_for_ids = true;

  layout.use_property_split_set(true);
  /* Decorators are added manually for supported properties because the
   * attribute/value toggle requires a manually built layout anyway. */
  layout.use_property_decorate_set(false);

  tree.ensure_interface_cache();
  ctx.input_usages.reinitialize(tree.interface_inputs().size());
  ctx.output_usages.reinitialize(tree.interface_outputs().size());
  nodes::socket_usage_inference::infer_group_interface_usage(
      tree, ctx.properties, ctx.input_usages, ctx.output_usages);
  draw_interface_panel_content(ctx, &layout, tree.tree_interface.root_panel);
}

}  // namespace blender::nodes
