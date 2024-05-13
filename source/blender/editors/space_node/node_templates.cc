/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cstdlib>
#include <cstring>
#include <optional>

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_interface.hh"
#include "BKE_node_tree_update.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"
#include "NOD_socket_declarations.hh"

#include "../interface/interface_intern.hh" /* XXX bad level */
#include "UI_interface.hh"

#include "ED_node.hh" /* own include */
#include "node_intern.hh"

#include "ED_undo.hh"

#include "WM_api.hh"

using blender::nodes::NodeDeclaration;

namespace blender::ed::space_node {

/************************* Node Socket Manipulation **************************/

/* describes an instance of a node type and a specific socket to link */
struct NodeLinkItem {
  int socket_index = -1;             /* index for linking */
  int socket_type = SOCK_CUSTOM;     /* socket type for compatibility check */
  const char *socket_name = nullptr; /* ui label of the socket */
  const char *node_name = nullptr;   /* ui label of the node */

  /* extra settings */
  bNodeTree *ngroup = nullptr; /* group node tree */
};

/* Compare an existing node to a link item to see if it can be reused.
 * item must be for the same node type!
 * XXX should become a node type callback
 */
static bool node_link_item_compare(bNode *node, NodeLinkItem *item)
{
  if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) {
    return (node->id == (ID *)item->ngroup);
  }
  return true;
}

static void node_link_item_apply(bNodeTree *ntree, bNode *node, NodeLinkItem *item)
{
  if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) {
    node->id = (ID *)item->ngroup;
    BKE_ntree_update_tag_node_property(ntree, node);
  }
  else {
    /* nothing to do for now */
  }

  if (node->id) {
    id_us_plus(node->id);
  }
}

static void node_tag_recursive(bNode *node)
{
  if (!node || (node->flag & NODE_TEST)) {
    return; /* in case of cycles */
  }

  node->flag |= NODE_TEST;

  LISTBASE_FOREACH (bNodeSocket *, input, &node->inputs) {
    if (input->link) {
      node_tag_recursive(input->link->fromnode);
    }
  }
}

static void node_clear_recursive(bNode *node)
{
  if (!node || !(node->flag & NODE_TEST)) {
    return; /* in case of cycles */
  }

  node->flag &= ~NODE_TEST;

  LISTBASE_FOREACH (bNodeSocket *, input, &node->inputs) {
    if (input->link) {
      node_clear_recursive(input->link->fromnode);
    }
  }
}

static void node_remove_linked(Main *bmain, bNodeTree *ntree, bNode *rem_node)
{
  bNode *node, *next;

  if (!rem_node) {
    return;
  }

  /* tag linked nodes to be removed */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->flag &= ~NODE_TEST;
  }

  node_tag_recursive(rem_node);

  /* clear tags on nodes that are still used by other nodes */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (!(node->flag & NODE_TEST)) {
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        if (sock->link && sock->link->fromnode != rem_node) {
          node_clear_recursive(sock->link->fromnode);
        }
      }
    }
  }

  /* remove nodes */
  for (node = (bNode *)ntree->nodes.first; node; node = next) {
    next = node->next;

    if (node->flag & NODE_TEST) {
      bke::nodeRemoveNode(bmain, ntree, node, true);
    }
  }
}

/* disconnect socket from the node it is connected to */
static void node_socket_disconnect(Main *bmain,
                                   bNodeTree *ntree,
                                   bNode *node_to,
                                   bNodeSocket *sock_to)
{
  if (!sock_to->link) {
    return;
  }

  bke::nodeRemLink(ntree, sock_to->link);
  sock_to->flag |= SOCK_COLLAPSED;

  BKE_ntree_update_tag_node_property(ntree, node_to);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

/* remove all nodes connected to this socket, if they aren't connected to other nodes */
static void node_socket_remove(Main *bmain, bNodeTree *ntree, bNode *node_to, bNodeSocket *sock_to)
{
  if (!sock_to->link) {
    return;
  }

  node_remove_linked(bmain, ntree, sock_to->link->fromnode);
  sock_to->flag |= SOCK_COLLAPSED;

  BKE_ntree_update_tag_node_property(ntree, node_to);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

/* add new node connected to this socket, or replace an existing one */
static void node_socket_add_replace(const bContext *C,
                                    bNodeTree *ntree,
                                    bNode *node_to,
                                    bNodeSocket *sock_to,
                                    int type,
                                    NodeLinkItem *item)
{
  Main *bmain = CTX_data_main(C);
  bNode *node_from;
  bNodeSocket *sock_from_tmp;
  bNode *node_prev = nullptr;

  /* unlink existing node */
  if (sock_to->link) {
    node_prev = sock_to->link->fromnode;
    bke::nodeRemLink(ntree, sock_to->link);
  }

  /* find existing node that we can use */
  for (node_from = (bNode *)ntree->nodes.first; node_from; node_from = node_from->next) {
    if (node_from->type == type) {
      break;
    }
  }

  if (node_from) {
    if (node_from->inputs.first || node_from->typeinfo->draw_buttons ||
        node_from->typeinfo->draw_buttons_ex)
    {
      node_from = nullptr;
    }
  }

  if (node_prev && node_prev->type == type && node_link_item_compare(node_prev, item)) {
    /* keep the previous node if it's the same type */
    node_from = node_prev;
  }
  else if (!node_from) {
    node_from = bke::nodeAddStaticNode(C, ntree, type);
    if (node_prev != nullptr) {
      /* If we're replacing existing node, use its location. */
      node_from->locx = node_prev->locx;
      node_from->locy = node_prev->locy;
      node_from->offsetx = node_prev->offsetx;
      node_from->offsety = node_prev->offsety;
    }
    else {
      sock_from_tmp = (bNodeSocket *)BLI_findlink(&node_from->outputs, item->socket_index);
      bke::nodePositionRelative(node_from, node_to, sock_from_tmp, sock_to);
    }

    node_link_item_apply(ntree, node_from, item);
    ED_node_tree_propagate_change(C, bmain, ntree);
  }

  bke::nodeSetActive(ntree, node_from);

  /* add link */
  sock_from_tmp = (bNodeSocket *)BLI_findlink(&node_from->outputs, item->socket_index);
  bke::nodeAddLink(ntree, node_from, sock_from_tmp, node_to, sock_to);
  sock_to->flag &= ~SOCK_COLLAPSED;

  /* copy input sockets from previous node */
  if (node_prev && node_from != node_prev) {
    LISTBASE_FOREACH (bNodeSocket *, sock_prev, &node_prev->inputs) {
      LISTBASE_FOREACH (bNodeSocket *, sock_from, &node_from->inputs) {
        if (bke::nodeCountSocketLinks(ntree, sock_from) >= bke::nodeSocketLinkLimit(sock_from)) {
          continue;
        }

        if (STREQ(sock_prev->identifier, sock_from->identifier) &&
            sock_prev->type == sock_from->type)
        {
          bNodeLink *link = sock_prev->link;

          if (link && link->fromnode) {
            bke::nodeAddLink(ntree, link->fromnode, link->fromsock, node_from, sock_from);
            bke::nodeRemLink(ntree, link);
          }

          node_socket_copy_default_value(sock_from, sock_prev);
        }
      }
    }

    /* also preserve mapping for texture nodes */
    if (node_from->typeinfo->nclass == NODE_CLASS_TEXTURE &&
        node_prev->typeinfo->nclass == NODE_CLASS_TEXTURE &&
        /* White noise texture node does not have NodeTexBase. */
        node_from->storage != nullptr && node_prev->storage != nullptr)
    {
      memcpy(node_from->storage, node_prev->storage, sizeof(NodeTexBase));
    }

    /* remove node */
    node_remove_linked(bmain, ntree, node_prev);
  }

  BKE_ntree_update_tag_node_property(ntree, node_from);
  BKE_ntree_update_tag_node_property(ntree, node_to);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

/****************************** Node Link Menu *******************************/

// #define UI_NODE_LINK_ADD        0
#define UI_NODE_LINK_DISCONNECT -1
#define UI_NODE_LINK_REMOVE -2

struct NodeLinkArg {
  Main *bmain;
  Scene *scene;
  bNodeTree *ntree;
  bNode *node;
  bNodeSocket *sock;

  bke::bNodeType *node_type;
  NodeLinkItem item;

  uiLayout *layout;
};

static Vector<NodeLinkItem> ui_node_link_items(NodeLinkArg *arg,
                                               int in_out,
                                               std::optional<NodeDeclaration> &r_node_decl)
{
  Vector<NodeLinkItem> items;

  /* XXX this should become a callback for node types! */
  if (arg->node_type->type == NODE_GROUP) {
    bNodeTree *ngroup;

    for (ngroup = (bNodeTree *)arg->bmain->nodetrees.first; ngroup;
         ngroup = (bNodeTree *)ngroup->id.next)
    {
      const char *disabled_hint;
      if ((ngroup->type != arg->ntree->type) ||
          !bke::nodeGroupPoll(arg->ntree, ngroup, &disabled_hint))
      {
        continue;
      }
    }

    for (ngroup = (bNodeTree *)arg->bmain->nodetrees.first; ngroup;
         ngroup = (bNodeTree *)ngroup->id.next)
    {
      const char *disabled_hint;
      if ((ngroup->type != arg->ntree->type) ||
          !bke::nodeGroupPoll(arg->ntree, ngroup, &disabled_hint))
      {
        continue;
      }

      ngroup->ensure_interface_cache();
      Span<bNodeTreeInterfaceSocket *> iosockets = (in_out == SOCK_IN ?
                                                        ngroup->interface_inputs() :
                                                        ngroup->interface_outputs());
      for (const int index : iosockets.index_range()) {
        bNodeTreeInterfaceSocket *iosock = iosockets[index];
        NodeLinkItem item;
        item.socket_index = index;
        /* NOTE: int stemp->type is not fully reliable, not used for node group
         * interface sockets. use the typeinfo->type instead.
         */
        const bke::bNodeSocketType *typeinfo = iosock->socket_typeinfo();
        item.socket_type = typeinfo->type;
        item.socket_name = iosock->name;
        item.node_name = ngroup->id.name + 2;
        item.ngroup = ngroup;

        items.append(item);
      }
    }
  }
  else if (arg->node_type->declare != nullptr) {
    using namespace blender;
    using namespace blender::nodes;

    r_node_decl.emplace(NodeDeclaration());
    blender::nodes::build_node_declaration(*arg->node_type, *r_node_decl, nullptr, nullptr);
    Span<SocketDeclaration *> socket_decls = (in_out == SOCK_IN) ? r_node_decl->inputs :
                                                                   r_node_decl->outputs;
    int index = 0;
    for (const SocketDeclaration *socket_decl_ptr : socket_decls) {
      const SocketDeclaration &socket_decl = *socket_decl_ptr;
      NodeLinkItem item;
      item.socket_index = index++;
      item.socket_type = socket_decl.socket_type;
      item.socket_name = socket_decl.name.c_str();
      item.node_name = arg->node_type->ui_name;
      items.append(item);
    }
  }
  else {
    bke::bNodeSocketTemplate *socket_templates = (in_out == SOCK_IN ? arg->node_type->inputs :
                                                                      arg->node_type->outputs);
    bke::bNodeSocketTemplate *stemp;
    int i;

    i = 0;
    for (stemp = socket_templates; stemp && stemp->type != -1; stemp++, i++) {
      NodeLinkItem item;
      item.socket_index = i;
      item.socket_type = stemp->type;
      item.socket_name = stemp->name;
      item.node_name = arg->node_type->ui_name;
      items.append(item);
    }
  }

  return items;
}

static void ui_node_link(bContext *C, void *arg_p, void *event_p)
{
  NodeLinkArg *arg = (NodeLinkArg *)arg_p;
  Main *bmain = arg->bmain;
  bNode *node_to = arg->node;
  bNodeSocket *sock_to = arg->sock;
  bNodeTree *ntree = arg->ntree;
  int event = POINTER_AS_INT(event_p);

  if (event == UI_NODE_LINK_DISCONNECT) {
    node_socket_disconnect(bmain, ntree, node_to, sock_to);
  }
  else if (event == UI_NODE_LINK_REMOVE) {
    node_socket_remove(bmain, ntree, node_to, sock_to);
  }
  else {
    node_socket_add_replace(C, ntree, node_to, sock_to, arg->node_type->type, &arg->item);
  }

  ED_undo_push(C, "Node input modify");
}

static void ui_node_sock_name(const bNodeTree *ntree,
                              bNodeSocket *sock,
                              char name[UI_MAX_NAME_STR])
{
  if (sock->link && sock->link->fromnode) {
    bNode *node = sock->link->fromnode;
    char node_name[UI_MAX_NAME_STR];

    bke::nodeLabel(ntree, node, node_name, sizeof(node_name));

    if (BLI_listbase_is_empty(&node->inputs) && node->outputs.first != node->outputs.last) {
      BLI_snprintf(
          name, UI_MAX_NAME_STR, "%s | %s", IFACE_(node_name), IFACE_(sock->link->fromsock->name));
    }
    else {
      BLI_strncpy_utf8(name, IFACE_(node_name), UI_MAX_NAME_STR);
    }
  }
  else if (sock->type == SOCK_SHADER) {
    BLI_strncpy_utf8(name, IFACE_("None"), UI_MAX_NAME_STR);
  }
  else {
    BLI_strncpy_utf8(name, IFACE_("Default"), UI_MAX_NAME_STR);
  }
}

static int ui_compatible_sockets(int typeA, int typeB)
{
  return (typeA == typeB);
}

static int ui_node_item_name_compare(const void *a, const void *b)
{
  const bke::bNodeType *type_a = *(const bke::bNodeType **)a;
  const bke::bNodeType *type_b = *(const bke::bNodeType **)b;
  return BLI_strcasecmp_natural(type_a->ui_name, type_b->ui_name);
}

static bool ui_node_item_special_poll(const bNodeTree * /*ntree*/, const bke::bNodeType *ntype)
{
  if (STREQ(ntype->idname, "ShaderNodeUVAlongStroke")) {
    /* TODO(sergey): Currently we don't have Freestyle nodes edited from
     * the buttons context, so can ignore its nodes completely.
     *
     * However, we might want to do some extra checks here later.
     */
    return false;
  }
  return true;
}

static void ui_node_menu_column(NodeLinkArg *arg, int nclass, const char *cname)
{
  bNodeTree *ntree = arg->ntree;
  bNodeSocket *sock = arg->sock;
  uiLayout *layout = arg->layout;
  uiLayout *column = nullptr;
  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but;
  NodeLinkArg *argN;
  int first = 1;

  /* generate array of node types sorted by UI name */
  blender::Vector<bke::bNodeType *> sorted_ntypes;

  NODE_TYPES_BEGIN (ntype) {
    const char *disabled_hint;
    if (!(ntype->poll && ntype->poll(ntype, ntree, &disabled_hint))) {
      continue;
    }

    if (ntype->nclass != nclass) {
      continue;
    }

    if (!ui_node_item_special_poll(ntree, ntype)) {
      continue;
    }

    sorted_ntypes.append(ntype);
  }
  NODE_TYPES_END;

  qsort(sorted_ntypes.data(),
        sorted_ntypes.size(),
        sizeof(bke::bNodeType *),
        ui_node_item_name_compare);

  /* generate UI */
  for (int j = 0; j < sorted_ntypes.size(); j++) {
    bke::bNodeType *ntype = sorted_ntypes[j];
    char name[UI_MAX_NAME_STR];
    const char *cur_node_name = nullptr;
    int num = 0;
    int icon = ICON_NONE;

    arg->node_type = ntype;

    std::optional<blender::nodes::NodeDeclaration> node_decl;
    Vector<NodeLinkItem> items = ui_node_link_items(arg, SOCK_OUT, node_decl);

    for (const NodeLinkItem &item : items) {
      if (ui_compatible_sockets(item.socket_type, sock->type)) {
        num++;
      }
    }

    for (const NodeLinkItem &item : items) {
      if (!ui_compatible_sockets(item.socket_type, sock->type)) {
        continue;
      }

      if (first) {
        column = uiLayoutColumn(layout, false);
        UI_block_layout_set_current(block, column);

        uiItemL(column, IFACE_(cname), ICON_NODE);
        but = (uiBut *)block->buttons.last;

        first = 0;
      }

      if (num > 1) {
        if (!cur_node_name || !STREQ(cur_node_name, item.node_name)) {
          cur_node_name = item.node_name;
          /* XXX Do not use uiItemL here,
           * it would add an empty icon as we are in a menu! */
          uiDefBut(block,
                   UI_BTYPE_LABEL,
                   0,
                   IFACE_(cur_node_name),
                   0,
                   0,
                   UI_UNIT_X * 4,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   "");
        }

        SNPRINTF(name, "%s", IFACE_(item.socket_name));
        icon = ICON_BLANK1;
      }
      else {
        STRNCPY_UTF8(name, IFACE_(item.node_name));
        icon = ICON_NONE;
      }

      but = uiDefIconTextBut(block,
                             UI_BTYPE_BUT,
                             0,
                             icon,
                             name,
                             0,
                             0,
                             UI_UNIT_X * 4,
                             UI_UNIT_Y,
                             nullptr,
                             0.0,
                             0.0,
                             TIP_("Add node to input"));

      argN = (NodeLinkArg *)MEM_dupallocN(arg);
      argN->item = item;
      UI_but_funcN_set(but, ui_node_link, argN, nullptr);
    }
  }
}

static void node_menu_column_foreach_cb(void *calldata, int nclass, const char *name)
{
  NodeLinkArg *arg = (NodeLinkArg *)calldata;

  if (!ELEM(nclass, NODE_CLASS_GROUP, NODE_CLASS_LAYOUT)) {
    ui_node_menu_column(arg, nclass, name);
  }
}

static void ui_template_node_link_menu(bContext *C, uiLayout *layout, void *but_p)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but = (uiBut *)but_p;
  uiLayout *split, *column;
  NodeLinkArg *arg = (NodeLinkArg *)but->func_argN;
  bNodeSocket *sock = arg->sock;
  bke::bNodeTreeType *ntreetype = arg->ntree->typeinfo;

  UI_block_layout_set_current(block, layout);
  split = uiLayoutSplit(layout, 0.0f, false);

  arg->bmain = bmain;
  arg->scene = scene;
  arg->layout = split;

  if (ntreetype && ntreetype->foreach_nodeclass) {
    ntreetype->foreach_nodeclass(arg, node_menu_column_foreach_cb);
  }

  column = uiLayoutColumn(split, false);
  UI_block_layout_set_current(block, column);

  if (sock->link) {
    uiItemL(column, IFACE_("Link"), ICON_NONE);
    but = (uiBut *)block->buttons.last;
    but->drawflag = UI_BUT_TEXT_LEFT;

    but = uiDefBut(block,
                   UI_BTYPE_BUT,
                   0,
                   IFACE_("Remove"),
                   0,
                   0,
                   UI_UNIT_X * 4,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   TIP_("Remove nodes connected to the input"));
    UI_but_funcN_set(but, ui_node_link, MEM_dupallocN(arg), POINTER_FROM_INT(UI_NODE_LINK_REMOVE));

    but = uiDefBut(block,
                   UI_BTYPE_BUT,
                   0,
                   IFACE_("Disconnect"),
                   0,
                   0,
                   UI_UNIT_X * 4,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   TIP_("Disconnect nodes connected to the input"));
    UI_but_funcN_set(
        but, ui_node_link, MEM_dupallocN(arg), POINTER_FROM_INT(UI_NODE_LINK_DISCONNECT));
  }

  ui_node_menu_column(arg, NODE_CLASS_GROUP, N_("Group"));
}

}  // namespace blender::ed::space_node

void uiTemplateNodeLink(
    uiLayout *layout, bContext *C, bNodeTree *ntree, bNode *node, bNodeSocket *input)
{
  using namespace blender::ed::space_node;

  uiBlock *block = uiLayoutGetBlock(layout);
  NodeLinkArg *arg;
  uiBut *but;
  float socket_col[4];

  arg = MEM_new<NodeLinkArg>("NodeLinkArg");
  arg->ntree = ntree;
  arg->node = node;
  arg->sock = input;

  PointerRNA node_ptr = RNA_pointer_create(&ntree->id, &RNA_Node, node);
  node_socket_color_get(*C, *ntree, node_ptr, *input, socket_col);

  UI_block_layout_set_current(block, layout);

  if (input->link || input->type == SOCK_SHADER || (input->flag & SOCK_HIDE_VALUE)) {
    char name[UI_MAX_NAME_STR];
    ui_node_sock_name(ntree, input, name);
    but = uiDefMenuBut(
        block, ui_template_node_link_menu, nullptr, name, 0, 0, UI_UNIT_X * 4, UI_UNIT_Y, "");
  }
  else {
    but = uiDefIconMenuBut(
        block, ui_template_node_link_menu, nullptr, ICON_NONE, 0, 0, UI_UNIT_X, UI_UNIT_Y, "");
  }

  UI_but_type_set_menu_from_pulldown(but);
  UI_but_node_link_set(but, input, socket_col);
  UI_but_drawflag_enable(but, UI_BUT_ICON_LEFT);

  but->poin = (char *)but;
  but->func_argN = arg;

  if (input->link && input->link->fromnode) {
    if (input->link->fromnode->flag & NODE_ACTIVE_TEXTURE) {
      but->flag |= UI_BUT_NODE_ACTIVE;
    }
  }
}

namespace blender::ed::space_node {

/**************************** Node Tree Layout *******************************/

static void ui_node_draw_input(uiLayout &layout,
                               bContext &C,
                               bNodeTree &ntree,
                               bNode &node,
                               bNodeSocket &input,
                               int depth,
                               const char *panel_label);

static void node_panel_toggle_button_cb(bContext *C, void *panel_state_argv, void *ntree_argv)
{
  Main *bmain = CTX_data_main(C);
  bNodePanelState *panel_state = static_cast<bNodePanelState *>(panel_state_argv);
  bNodeTree *ntree = static_cast<bNodeTree *>(ntree_argv);

  panel_state->flag ^= NODE_PANEL_COLLAPSED;

  ED_node_tree_propagate_change(C, bmain, ntree);

  /* Make sure panel state updates from the Properties Editor, too. */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);
}

static void ui_node_draw_panel(uiLayout &layout,
                               bContext &C,
                               bNodeTree &ntree,
                               const nodes::PanelDeclaration &panel_decl,
                               bNodePanelState &panel_state,
                               PointerRNA nodeptr)
{
  uiLayout *row = uiLayoutRow(&layout, true);
  uiLayoutSetPropDecorate(row, false);

  /* Panel header with collapse icon */
  uiBlock *block = uiLayoutGetBlock(row);
  UI_block_emboss_set(block, UI_EMBOSS_NONE);
  uiBut *but = uiDefIconTextBut(block,
                                UI_BTYPE_BUT,
                                0,
                                panel_state.is_collapsed() ? ICON_RIGHTARROW : ICON_DOWNARROW_HLT,
                                IFACE_(panel_decl.name.c_str()),
                                0,
                                0,
                                UI_UNIT_X * 4,
                                UI_UNIT_Y,
                                nullptr,
                                0.0,
                                0.0,
                                "");
  UI_but_drawflag_enable(but, UI_BUT_TEXT_LEFT | UI_BUT_NO_TOOLTIP);
  UI_but_func_set(but, node_panel_toggle_button_cb, &panel_state, &ntree);
  UI_block_emboss_set(block, UI_EMBOSS);

  /* Panel buttons. */
  if (!panel_state.is_collapsed() && panel_decl.draw_buttons) {
    uiLayoutSetPropSep(&layout, true);
    panel_decl.draw_buttons(&layout, &C, &nodeptr);
  }
}

static void ui_node_draw_node(
    uiLayout &layout, bContext &C, bNodeTree &ntree, bNode &node, int depth)
{
  PointerRNA nodeptr = RNA_pointer_create(&ntree.id, &RNA_Node, &node);

  if (node.typeinfo->draw_buttons) {
    if (node.type != NODE_GROUP) {
      uiLayoutSetPropSep(&layout, true);
      node.typeinfo->draw_buttons(&layout, &C, &nodeptr);
    }
  }

  if (node.declaration() && node.declaration()->use_custom_socket_order) {
    /* Node with panels. */
    namespace nodes = blender::nodes;
    using ItemDeclIterator = blender::Span<nodes::ItemDeclarationPtr>::iterator;
    using SocketIterator = blender::Span<bNodeSocket *>::iterator;
    using PanelStateIterator = blender::MutableSpan<bNodePanelState>::iterator;

    ItemDeclIterator item_decl = node.declaration()->items.begin();
    SocketIterator input = node.input_sockets().begin();
    PanelStateIterator panel_state = node.panel_states().begin();
    const ItemDeclIterator item_decl_end = node.declaration()->items.end();

    bool panel_collapsed = false;
    const char *panel_label = nullptr;

    for (; item_decl != item_decl_end; ++item_decl) {
      if (const nodes::SocketDeclaration *socket_decl =
              dynamic_cast<const nodes::SocketDeclaration *>(item_decl->get()))
      {
        if (socket_decl->in_out == SOCK_IN) {
          if (!panel_collapsed) {
            ui_node_draw_input(layout, C, ntree, node, **input, depth + 1, panel_label);
          }
          ++input;
        }
      }
      else if (const nodes::PanelDeclaration *panel_decl =
                   dynamic_cast<const nodes::PanelDeclaration *>(item_decl->get()))
      {
        panel_collapsed = panel_state->is_collapsed();
        panel_label = panel_decl->name.c_str();
        ui_node_draw_panel(layout, C, ntree, *panel_decl, *panel_state, nodeptr);
        ++panel_state;
      }
    }
  }
  else {
    /* Node without panels. */
    LISTBASE_FOREACH (bNodeSocket *, input, &node.inputs) {
      ui_node_draw_input(layout, C, ntree, node, *input, depth + 1, nullptr);
    }
  }
}

static void ui_node_draw_input(uiLayout &layout,
                               bContext &C,
                               bNodeTree &ntree,
                               bNode &node,
                               bNodeSocket &input,
                               int depth,
                               const char *panel_label)
{
  uiBlock *block = uiLayoutGetBlock(&layout);
  uiLayout *row = nullptr;
  bool dependency_loop;

  if (input.flag & SOCK_UNAVAIL) {
    return;
  }

  /* to avoid eternal loops on cyclic dependencies */
  node.flag |= NODE_TEST;
  bNode *lnode = (input.link) ? input.link->fromnode : nullptr;

  dependency_loop = (lnode && (lnode->flag & NODE_TEST));
  if (dependency_loop) {
    lnode = nullptr;
  }

  /* socket RNA pointer */
  PointerRNA inputptr = RNA_pointer_create(&ntree.id, &RNA_NodeSocket, &input);
  PointerRNA nodeptr = RNA_pointer_create(&ntree.id, &RNA_Node, &node);

  row = uiLayoutRow(&layout, true);
  /* Decorations are added manually here. */
  uiLayoutSetPropDecorate(row, false);

  uiPropertySplitWrapper split_wrapper = uiItemPropertySplitWrapperCreate(row);
  /* Empty decorator item for alignment. */
  bool add_dummy_decorator = false;

  {
    uiLayout *sub = uiLayoutRow(split_wrapper.label_column, true);

    if (depth > 0) {
      UI_block_emboss_set(block, UI_EMBOSS_NONE);

      if (lnode &&
          (lnode->inputs.first || (lnode->typeinfo->draw_buttons && lnode->type != NODE_GROUP)))
      {
        int icon = (input.flag & SOCK_COLLAPSED) ? ICON_RIGHTARROW : ICON_DOWNARROW_HLT;
        uiItemR(sub, &inputptr, "show_expanded", UI_ITEM_R_ICON_ONLY, "", icon);
      }

      UI_block_emboss_set(block, UI_EMBOSS);
    }

    sub = uiLayoutRow(sub, true);
    uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_RIGHT);
    uiItemL(sub, node_socket_get_label(&input, panel_label), ICON_NONE);
  }

  if (dependency_loop) {
    uiItemL(row, RPT_("Dependency Loop"), ICON_ERROR);
    add_dummy_decorator = true;
  }
  else if (lnode) {
    /* input linked to a node */
    uiTemplateNodeLink(row, &C, &ntree, &node, &input);
    add_dummy_decorator = true;

    if (depth == 0 || !(input.flag & SOCK_COLLAPSED)) {
      if (depth == 0) {
        uiItemS(&layout);
      }

      ui_node_draw_node(layout, C, ntree, *lnode, depth);
    }
  }
  else {
    uiLayout *sub = uiLayoutRow(row, true);

    uiTemplateNodeLink(sub, &C, &ntree, &node, &input);

    if (input.flag & SOCK_HIDE_VALUE) {
      add_dummy_decorator = true;
    }
    /* input not linked, show value */
    else {
      switch (input.type) {
        case SOCK_VECTOR:
          uiItemS(sub);
          sub = uiLayoutColumn(sub, true);
          ATTR_FALLTHROUGH;
        case SOCK_FLOAT:
        case SOCK_INT:
        case SOCK_ROTATION:
        case SOCK_BOOLEAN:
        case SOCK_RGBA:
          uiItemR(sub, &inputptr, "default_value", UI_ITEM_NONE, "", ICON_NONE);
          uiItemDecoratorR(
              split_wrapper.decorate_column, &inputptr, "default_value", RNA_NO_INDEX);
          break;
        case SOCK_STRING: {
          const bNodeTree *node_tree = (const bNodeTree *)nodeptr.owner_id;
          SpaceNode *snode = CTX_wm_space_node(&C);
          if (node_tree->type == NTREE_GEOMETRY && snode != nullptr) {
            /* Only add the attribute search in the node editor, in other places there is not
             * enough context. */
            node_geometry_add_attribute_search_button(C, node, inputptr, *sub);
          }
          else {
            uiItemR(sub, &inputptr, "default_value", UI_ITEM_NONE, "", ICON_NONE);
          }
          uiItemDecoratorR(
              split_wrapper.decorate_column, &inputptr, "default_value", RNA_NO_INDEX);
          break;
        }
        case SOCK_MENU:
          uiItemL(sub, RPT_("Unsupported Menu Socket"), ICON_NONE);
          break;
        case SOCK_CUSTOM:
          input.typeinfo->draw(&C, sub, &inputptr, &nodeptr, input.name);
          break;
        default:
          add_dummy_decorator = true;
      }
    }
  }

  if (add_dummy_decorator) {
    uiItemDecoratorR(split_wrapper.decorate_column, nullptr, nullptr, 0);
  }

  node_socket_add_tooltip(ntree, input, *row);

  /* clear */
  node.flag &= ~NODE_TEST;
}

}  // namespace blender::ed::space_node

void uiTemplateNodeView(
    uiLayout *layout, bContext *C, bNodeTree *ntree, bNode *node, bNodeSocket *input)
{
  using namespace blender::ed::space_node;

  if (!ntree) {
    return;
  }
  ntree->ensure_topology_cache();

  /* clear for cycle check */
  LISTBASE_FOREACH (bNode *, tnode, &ntree->nodes) {
    tnode->flag &= ~NODE_TEST;
  }

  if (input) {
    ui_node_draw_input(*layout, *C, *ntree, *node, *input, 0, nullptr);
  }
  else {
    ui_node_draw_node(*layout, *C, *ntree, *node, 0);
  }
}
