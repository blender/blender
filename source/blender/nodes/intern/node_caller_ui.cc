/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_tree_interface_types.h"

#include "NOD_caller_ui.hh"
#include "NOD_socket_usage_inference.hh"

#include "RNA_access.hh"
#include "RNA_types.hh"

#include "BLT_translation.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

namespace blender::nodes {

static bool interface_panel_has_socket(
    const bNodeTreeInterfacePanel &interface_panel,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_visible)
{
  for (const bNodeTreeInterfaceItem *item : interface_panel.items()) {
    if (item->item_type == NodeTreeInterfaceItemType::Socket) {
      const bNodeTreeInterfaceSocket &socket = *reinterpret_cast<const bNodeTreeInterfaceSocket *>(
          item);
      if (socket.flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER) {
        continue;
      }
      if (socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
        if (fn_input_is_visible(socket)) {
          return true;
        }
      }
    }
    else if (item->item_type == NodeTreeInterfaceItemType::Panel) {
      const auto &panel_item = *reinterpret_cast<const bNodeTreeInterfacePanel *>(item);
      if (interface_panel_has_socket(panel_item, fn_input_is_visible)) {
        return true;
      }
    }
  }
  return false;
}

static bool interface_panel_affects_output(
    const bNodeTreeInterfacePanel &panel,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_active)
{
  for (const bNodeTreeInterfaceItem *item : panel.items()) {
    if (item->item_type == NodeTreeInterfaceItemType::Socket) {
      const auto &socket = *reinterpret_cast<const bNodeTreeInterfaceSocket *>(item);
      if (socket.flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER) {
        continue;
      }
      if (!(socket.flag & NODE_INTERFACE_SOCKET_INPUT)) {
        continue;
      }
      if (fn_input_is_active(socket)) {
        return true;
      }
    }
    else if (item->item_type == NodeTreeInterfaceItemType::Panel) {
      const auto &sub_interface_panel = *reinterpret_cast<const bNodeTreeInterfacePanel *>(item);
      if (interface_panel_affects_output(sub_interface_panel, fn_input_is_active)) {
        return true;
      }
    }
  }
  return false;
}

void draw_interface_panel_as_panel(
    const bContext &C,
    ui::Layout &layout,
    PointerRNA *properties_ptr,
    const bNodeTreeInterfacePanel &interface_panel,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_visible,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_active,
    FunctionRef<void(ui::Layout &,
                     const bNodeTreeInterfaceSocket &,
                     PointerRNA *,
                     const std::optional<StringRef>)> fn_draw_property_for_socket)
{
  if (!interface_panel_has_socket(interface_panel, fn_input_is_visible)) {
    return;
  }
  PointerRNA panels_ptr = RNA_pointer_get(properties_ptr, "panels");
  const std::string panel_open_name = fmt::format("open_{}", interface_panel.identifier);
  ui::PanelLayout panel_layout;
  bool skip_first = false;
  /* Check if the panel should have a toggle in the header. */
  const bNodeTreeInterfaceSocket *toggle_socket = interface_panel.header_toggle_socket();
  const StringRef panel_name = interface_panel.name;
  if (toggle_socket && !(toggle_socket->flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER)) {
    PointerRNA inputs_ptr = RNA_pointer_get(properties_ptr, "inputs");
    PointerRNA toggle_ptr = RNA_pointer_get(&inputs_ptr, toggle_socket->identifier);
    panel_layout = layout.panel_prop_with_bool_header(
        &C, &panels_ptr, panel_open_name, &toggle_ptr, "value", IFACE_(panel_name));
    skip_first = true;
  }
  else {
    panel_layout = layout.panel_prop(&C, &panels_ptr, panel_open_name);
    panel_layout.header->label(IFACE_(panel_name), ICON_NONE);
  }
  if (!interface_panel_affects_output(interface_panel, fn_input_is_active)) {
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
    draw_interface_panel_content(C,
                                 *panel_layout.body,
                                 properties_ptr,
                                 interface_panel,
                                 fn_input_is_visible,
                                 fn_input_is_active,
                                 fn_draw_property_for_socket,
                                 skip_first,
                                 panel_name);
  }
}

void draw_interface_panel_content(
    const bContext &C,
    ui::Layout &layout,
    PointerRNA *properties_ptr,
    const bNodeTreeInterfacePanel &interface_panel,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_visible,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_active,
    FunctionRef<void(ui::Layout &,
                     const bNodeTreeInterfaceSocket &,
                     PointerRNA *,
                     const std::optional<StringRef>)> fn_draw_property_for_socket,
    const bool skip_first,
    const std::optional<StringRef> parent_name)
{
  for (const bNodeTreeInterfaceItem *item : interface_panel.items().drop_front(skip_first ? 1 : 0))
  {
    switch (item->item_type) {
      case NodeTreeInterfaceItemType::Panel: {
        const auto &sub_interface_panel = *reinterpret_cast<const bNodeTreeInterfacePanel *>(item);
        draw_interface_panel_as_panel(C,
                                      layout,
                                      properties_ptr,
                                      sub_interface_panel,
                                      fn_input_is_visible,
                                      fn_input_is_active,
                                      fn_draw_property_for_socket);
        break;
      }
      case NodeTreeInterfaceItemType::Socket: {
        const auto &interface_socket = *reinterpret_cast<const bNodeTreeInterfaceSocket *>(item);
        if (interface_socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
          if (!(interface_socket.flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER)) {
            PointerRNA inputs_ptr = RNA_pointer_get(properties_ptr, "inputs");
            PointerRNA socket_props_ptr = RNA_pointer_get(&inputs_ptr,
                                                          interface_socket.identifier);
            fn_draw_property_for_socket(layout, interface_socket, &socket_props_ptr, parent_name);
          }
        }
        break;
      }
    }
  }
}

}  // namespace blender::nodes
