/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_vector.hh"

#include "BKE_context.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "NOD_node_declaration.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

/* -------------------------------------------------------------------- */
/** \name Node Input Buttons Template
 * \{ */

using blender::nodes::ItemDeclaration;
using blender::nodes::LayoutDeclaration;
using blender::nodes::NodeDeclaration;
using blender::nodes::PanelDeclaration;
using blender::nodes::SocketDeclaration;

using ItemIterator = blender::Vector<blender::nodes::ItemDeclarationPtr>::const_iterator;

namespace blender::ui::nodes {

static void draw_node_input(bContext *C,
                            uiLayout *layout,
                            PointerRNA *node_ptr,
                            bNodeSocket &socket)
{
  BLI_assert(socket.typeinfo != nullptr);
  /* Ignore disabled sockets and linked sockets and sockets without a `draw` callback. */
  if (!socket.is_available()) {
    return;
  }
  if ((socket.flag & (SOCK_IS_LINKED | SOCK_HIDE_VALUE)) != 0) {
    return;
  }
  if (socket.typeinfo->draw == nullptr) {
    return;
  }
  if (ELEM(socket.type, SOCK_GEOMETRY, SOCK_MATRIX, SOCK_SHADER)) {
    return;
  }
  const bNode &node = *static_cast<bNode *>(node_ptr->data);
  if (node.is_reroute()) {
    return;
  }

  PointerRNA socket_ptr = RNA_pointer_create(node_ptr->owner_id, &RNA_NodeSocket, &socket);
  const char *text = IFACE_(bke::nodeSocketLabel(&socket));
  uiLayout *row = uiLayoutRow(layout, true);
  socket.typeinfo->draw(C, row, &socket_ptr, node_ptr, text);
}

static void draw_node_inputs_recursive(bContext *C,
                                       uiLayout *layout,
                                       bNode &node,
                                       PointerRNA *node_ptr,
                                       const blender::nodes::PanelDeclaration &panel_decl)
{
  /* Use a root panel property to toggle open/closed state. */
  /* TODO: Use flag on the panel state instead which is better for dynamic panel amounts. */
  const std::string panel_idname = "NodePanel" + std::to_string(panel_decl.identifier);
  Panel *root_panel = uiLayoutGetRootPanel(layout);
  LayoutPanelState *state = BKE_panel_layout_panel_state_ensure(
      root_panel, panel_idname.c_str(), panel_decl.default_collapsed);
  PointerRNA state_ptr = RNA_pointer_create(nullptr, &RNA_LayoutPanelState, state);
  uiLayout *panel_layout = uiLayoutPanelProp(
      C, layout, &state_ptr, "is_open", IFACE_(panel_decl.name.c_str()));
  if (!(state->flag & LAYOUT_PANEL_STATE_FLAG_OPEN)) {
    return;
  }
  for (const ItemDeclaration *item_decl : panel_decl.items) {
    if (const auto *socket_decl = dynamic_cast<const SocketDeclaration *>(item_decl)) {
      if (socket_decl->in_out == SOCK_IN) {
        draw_node_input(C, panel_layout, node_ptr, node.socket_by_decl(*socket_decl));
      }
    }
    else if (const auto *sub_panel_decl = dynamic_cast<const PanelDeclaration *>(item_decl)) {
      draw_node_inputs_recursive(C, panel_layout, node, node_ptr, *sub_panel_decl);
    }
    else if (const auto *layout_decl = dynamic_cast<const LayoutDeclaration *>(item_decl)) {
      if (!layout_decl->is_default) {
        layout_decl->draw(panel_layout, C, node_ptr);
      }
    }
  }
}

}  // namespace blender::ui::nodes

void uiTemplateNodeInputs(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  using namespace blender::nodes;
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);

  tree.ensure_topology_cache();

  BLI_assert(node.typeinfo != nullptr);
  /* Draw top-level node buttons. */
  if (node.typeinfo->draw_buttons_ex) {
    node.typeinfo->draw_buttons_ex(layout, C, ptr);
  }
  else if (node.typeinfo->draw_buttons) {
    node.typeinfo->draw_buttons(layout, C, ptr);
  }

  if (node.declaration()) {
    /* Draw socket inputs and panel buttons in the order of declaration panels. */
    const NodeDeclaration &node_decl = *node.declaration();
    for (const ItemDeclaration *item_decl : node_decl.root_items) {
      if (const auto *panel_decl = dynamic_cast<const PanelDeclaration *>(item_decl)) {
        blender::ui::nodes::draw_node_inputs_recursive(C, layout, node, ptr, *panel_decl);
      }
      else if (const auto *socket_decl = dynamic_cast<const SocketDeclaration *>(item_decl)) {
        if (socket_decl->in_out == SOCK_IN) {
          blender::ui::nodes::draw_node_input(C, layout, ptr, node.socket_by_decl(*socket_decl));
        }
      }
      else if (const auto *layout_decl = dynamic_cast<const LayoutDeclaration *>(item_decl)) {
        if (!layout_decl->is_default) {
          layout_decl->draw(layout, C, ptr);
        }
      }
    }
  }
  else {
    /* Draw socket values using the flat inputs list. */
    for (bNodeSocket *input : node.runtime->inputs) {
      blender::ui::nodes::draw_node_input(C, layout, ptr, *input);
    }
  }
}

/** \} */
