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
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

/* -------------------------------------------------------------------- */
/** \name Node Input Buttons Template
 * \{ */

using blender::nodes::ItemDeclaration;
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
  socket.typeinfo->draw(C, layout, &socket_ptr, node_ptr, text);
}

static void draw_node_input(bContext *C,
                            uiLayout *layout,
                            PointerRNA *node_ptr,
                            StringRefNull identifier)
{
  bNode &node = *static_cast<bNode *>(node_ptr->data);
  bNodeSocket *socket = node.runtime->inputs_by_identifier.lookup(identifier);
  draw_node_input(C, layout, node_ptr, *socket);
}

/* Consume the item range, draw buttons if layout is not null. */
static void handle_node_declaration_items(bContext *C,
                                          Panel *root_panel,
                                          uiLayout *layout,
                                          PointerRNA *node_ptr,
                                          ItemIterator &item_iter,
                                          const ItemIterator item_end)
{
  while (item_iter != item_end) {
    const ItemDeclaration *item_decl = item_iter->get();
    ++item_iter;

    if (const SocketDeclaration *socket_decl = dynamic_cast<const SocketDeclaration *>(item_decl))
    {
      if (layout && socket_decl->in_out == SOCK_IN) {
        draw_node_input(C, layout, node_ptr, socket_decl->identifier);
      }
    }
    else if (const PanelDeclaration *panel_decl = dynamic_cast<const PanelDeclaration *>(
                 item_decl))
    {
      const ItemIterator panel_item_end = item_iter + panel_decl->num_child_decls;
      BLI_assert(panel_item_end <= item_end);

      /* Use a root panel property to toggle open/closed state. */
      const std::string panel_idname = "NodePanel" + std::to_string(panel_decl->identifier);
      LayoutPanelState *state = BKE_panel_layout_panel_state_ensure(
          root_panel, panel_idname.c_str(), panel_decl->default_collapsed);
      PointerRNA state_ptr = RNA_pointer_create(nullptr, &RNA_LayoutPanelState, state);
      uiLayout *panel_layout = uiLayoutPanelProp(
          C, layout, &state_ptr, "is_open", IFACE_(panel_decl->name.c_str()));
      /* Draw panel buttons at the top of each panel section. */
      if (panel_layout && panel_decl->draw_buttons) {
        panel_decl->draw_buttons(panel_layout, C, node_ptr);
      }

      handle_node_declaration_items(
          C, root_panel, panel_layout, node_ptr, item_iter, panel_item_end);
    }
  }
}

}  // namespace blender::ui::nodes

void uiTemplateNodeInputs(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
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
    ItemIterator item_iter = node.declaration()->items.begin();
    const ItemIterator item_end = node.declaration()->items.end();
    Panel *root_panel = uiLayoutGetRootPanel(layout);
    blender::ui::nodes::handle_node_declaration_items(
        C, root_panel, layout, ptr, item_iter, item_end);
  }
  else {
    /* Draw socket values using the flat inputs list. */
    for (bNodeSocket *input : node.runtime->inputs) {
      blender::ui::nodes::draw_node_input(C, layout, ptr, *input);
    }
  }
}

/** \} */
