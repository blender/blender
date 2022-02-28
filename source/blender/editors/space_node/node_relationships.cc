/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spnode
 */

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_node_types.h"

#include "BLI_easing.h"

#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_tree_update.h"
#include "BKE_screen.h"

#include "ED_node.h" /* own include */
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_spreadsheet.h"
#include "ED_util.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_state.h"

#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "BLT_translation.h"

#include "NOD_node_declaration.hh"
#include "NOD_node_tree_ref.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "node_intern.hh" /* own include */

using namespace blender::nodes::node_tree_ref_types;

/* -------------------------------------------------------------------- */
/** \name Add Node
 * \{ */

struct bNodeListItem {
  struct bNodeListItem *next, *prev;
  struct bNode *node;
};

struct NodeInsertOfsData {
  bNodeTree *ntree;
  bNode *insert;      /* inserted node */
  bNode *prev, *next; /* prev/next node in the chain */
  bNode *insert_parent;

  wmTimer *anim_timer;

  float offset_x; /* offset to apply to node chain */
};

static void clear_picking_highlight(ListBase *links)
{
  LISTBASE_FOREACH (bNodeLink *, link, links) {
    link->flag &= ~NODE_LINK_TEMP_HIGHLIGHT;
  }
}

namespace blender::ed::space_node {

static bNodeLink *create_drag_link(bNode &node, bNodeSocket &sock)
{
  bNodeLink *oplink = MEM_cnew<bNodeLink>(__func__);
  if (sock.in_out == SOCK_OUT) {
    oplink->fromnode = &node;
    oplink->fromsock = &sock;
  }
  else {
    oplink->tonode = &node;
    oplink->tosock = &sock;
  }
  oplink->flag |= NODE_LINK_VALID;
  oplink->flag |= NODE_LINK_DRAGGED;
  return oplink;
}

static void pick_link(
    wmOperator &op, bNodeLinkDrag &nldrag, SpaceNode &snode, bNode *node, bNodeLink &link_to_pick)
{
  clear_picking_highlight(&snode.edittree->links);
  RNA_boolean_set(op.ptr, "has_link_picked", true);

  bNodeLink *link = create_drag_link(*link_to_pick.fromnode, *link_to_pick.fromsock);

  nldrag.links.append(link);
  nodeRemLink(snode.edittree, &link_to_pick);

  BLI_assert(nldrag.last_node_hovered_while_dragging_a_link != nullptr);

  sort_multi_input_socket_links(
      snode, *nldrag.last_node_hovered_while_dragging_a_link, nullptr, nullptr);

  /* Send changed event to original link->tonode. */
  if (node) {
    BKE_ntree_update_tag_node_property(snode.edittree, node);
  }
}

static void pick_input_link_by_link_intersect(const bContext &C,
                                              wmOperator &op,
                                              bNodeLinkDrag &nldrag,
                                              const float2 &cursor)
{
  SpaceNode *snode = CTX_wm_space_node(&C);
  const ARegion *region = CTX_wm_region(&C);
  const View2D *v2d = &region->v2d;

  float drag_start[2];
  RNA_float_get_array(op.ptr, "drag_start", drag_start);
  bNode *node;
  bNodeSocket *socket;
  node_find_indicated_socket(*snode, &node, &socket, drag_start, SOCK_IN);

  /* Distance to test overlapping of cursor on link. */
  const float cursor_link_touch_distance = 12.5f * UI_DPI_FAC;

  const int resolution = NODE_LINK_RESOL;

  bNodeLink *link_to_pick = nullptr;
  clear_picking_highlight(&snode->edittree->links);
  LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
    if (link->tosock == socket) {
      /* Test if the cursor is near a link. */
      float vec[4][2];
      node_link_bezier_handles(v2d, snode, *link, vec);

      float data[NODE_LINK_RESOL * 2 + 2];
      BKE_curve_forward_diff_bezier(
          vec[0][0], vec[1][0], vec[2][0], vec[3][0], data, resolution, sizeof(float[2]));
      BKE_curve_forward_diff_bezier(
          vec[0][1], vec[1][1], vec[2][1], vec[3][1], data + 1, resolution, sizeof(float[2]));

      for (int i = 0; i < resolution * 2; i += 2) {
        float *l1 = &data[i];
        float *l2 = &data[i + 2];
        float distance = dist_squared_to_line_segment_v2(cursor, l1, l2);
        if (distance < cursor_link_touch_distance) {
          link_to_pick = link;
          nldrag.last_picked_multi_input_socket_link = link_to_pick;
        }
      }
    }
  }

  /* If no linked was picked in this call, try using the one picked in the previous call.
   * Not essential for the basic behavior, but can make interaction feel a bit better if
   * the mouse moves to the right and loses the "selection." */
  if (!link_to_pick) {
    bNodeLink *last_picked_link = nldrag.last_picked_multi_input_socket_link;
    if (last_picked_link) {
      link_to_pick = last_picked_link;
    }
  }

  if (link_to_pick) {
    /* Highlight is set here and cleared in the next iteration or if the operation finishes. */
    link_to_pick->flag |= NODE_LINK_TEMP_HIGHLIGHT;
    ED_area_tag_redraw(CTX_wm_area(&C));

    if (!node_find_indicated_socket(*snode, &node, &socket, cursor, SOCK_IN)) {
      pick_link(op, nldrag, *snode, node, *link_to_pick);
    }
  }
}

static bool socket_is_available(bNodeTree *UNUSED(ntree), bNodeSocket *sock, const bool allow_used)
{
  if (nodeSocketIsHidden(sock)) {
    return false;
  }

  if (!allow_used && (sock->flag & SOCK_IN_USE)) {
    /* Multi input sockets are available (even if used). */
    if (!(sock->flag & SOCK_MULTI_INPUT)) {
      return false;
    }
  }

  return true;
}

static bNodeSocket *best_socket_output(bNodeTree *ntree,
                                       bNode *node,
                                       bNodeSocket *sock_target,
                                       const bool allow_multiple)
{
  /* first look for selected output */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    if (!socket_is_available(ntree, sock, allow_multiple)) {
      continue;
    }

    if (sock->flag & SELECT) {
      return sock;
    }
  }

  /* try to find a socket with a matching name */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    if (!socket_is_available(ntree, sock, allow_multiple)) {
      continue;
    }

    /* check for same types */
    if (sock->type == sock_target->type) {
      if (STREQ(sock->name, sock_target->name)) {
        return sock;
      }
    }
  }

  /* otherwise settle for the first available socket of the right type */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    if (!socket_is_available(ntree, sock, allow_multiple)) {
      continue;
    }

    /* check for same types */
    if (sock->type == sock_target->type) {
      return sock;
    }
  }

  /* Always allow linking to an reroute node. The socket type of the reroute sockets might change
   * after the link has been created. */
  if (node->type == NODE_REROUTE) {
    return (bNodeSocket *)node->outputs.first;
  }

  return nullptr;
}

/* this is a bit complicated, but designed to prioritize finding
 * sockets of higher types, such as image, first */
static bNodeSocket *best_socket_input(bNodeTree *ntree, bNode *node, int num, int replace)
{
  int maxtype = 0;
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    maxtype = max_ii(sock->type, maxtype);
  }

  /* find sockets of higher 'types' first (i.e. image) */
  int a = 0;
  for (int socktype = maxtype; socktype >= 0; socktype--) {
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      if (!socket_is_available(ntree, sock, replace)) {
        a++;
        continue;
      }

      if (sock->type == socktype) {
        /* increment to make sure we don't keep finding
         * the same socket on every attempt running this function */
        a++;
        if (a > num) {
          return sock;
        }
      }
    }
  }

  return nullptr;
}

static bool snode_autoconnect_input(SpaceNode &snode,
                                    bNode *node_fr,
                                    bNodeSocket *sock_fr,
                                    bNode *node_to,
                                    bNodeSocket *sock_to,
                                    int replace)
{
  bNodeTree *ntree = snode.edittree;

  /* then we can connect */
  if (replace) {
    nodeRemSocketLinks(ntree, sock_to);
  }

  nodeAddLink(ntree, node_fr, sock_fr, node_to, sock_to);
  return true;
}

struct LinkAndPosition {
  bNodeLink *link;
  float2 multi_socket_position;
};

void sort_multi_input_socket_links(SpaceNode &snode,
                                   bNode &node,
                                   bNodeLink *drag_link,
                                   const float2 *cursor)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (!(socket->flag & SOCK_MULTI_INPUT)) {
      continue;
    }
    Vector<LinkAndPosition, 8> links;

    LISTBASE_FOREACH (bNodeLink *, link, &snode.edittree->links) {
      if (link->tosock == socket) {
        links.append(
            {link,
             node_link_calculate_multi_input_position({link->tosock->locx, link->tosock->locy},
                                                      link->multi_input_socket_index,
                                                      link->tosock->total_inputs)});
      }
    }

    if (drag_link) {
      LinkAndPosition link_and_position{};
      link_and_position.link = drag_link;
      if (cursor) {
        link_and_position.multi_socket_position = *cursor;
      }
      links.append(link_and_position);
    }

    std::sort(links.begin(), links.end(), [](const LinkAndPosition a, const LinkAndPosition b) {
      return a.multi_socket_position.y < b.multi_socket_position.y;
    });

    for (const int i : links.index_range()) {
      links[i].link->multi_input_socket_index = i;
    }
  }
}

static void snode_autoconnect(SpaceNode &snode, const bool allow_multiple, const bool replace)
{
  bNodeTree *ntree = snode.edittree;
  Vector<bNode *> sorted_nodes;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & NODE_SELECT) {
      sorted_nodes.append(node);
    }
  }

  /* Sort nodes left to right. */
  std::sort(sorted_nodes.begin(), sorted_nodes.end(), [](const bNode *a, const bNode *b) {
    return a->locx < b->locx;
  });

  int numlinks = 0;
  for (const int i : sorted_nodes.as_mutable_span().drop_back(1).index_range()) {
    bool has_selected_inputs = false;

    bNode *node_fr = sorted_nodes[i];
    bNode *node_to = sorted_nodes[i + 1];
    /* corner case: input/output node aligned the wrong way around (T47729) */
    if (BLI_listbase_is_empty(&node_to->inputs) || BLI_listbase_is_empty(&node_fr->outputs)) {
      SWAP(bNode *, node_fr, node_to);
    }

    /* if there are selected sockets, connect those */
    LISTBASE_FOREACH (bNodeSocket *, sock_to, &node_to->inputs) {
      if (sock_to->flag & SELECT) {
        has_selected_inputs = true;

        if (!socket_is_available(ntree, sock_to, replace)) {
          continue;
        }

        /* check for an appropriate output socket to connect from */
        bNodeSocket *sock_fr = best_socket_output(ntree, node_fr, sock_to, allow_multiple);
        if (!sock_fr) {
          continue;
        }

        if (snode_autoconnect_input(snode, node_fr, sock_fr, node_to, sock_to, replace)) {
          numlinks++;
        }
      }
    }

    if (!has_selected_inputs) {
      /* no selected inputs, connect by finding suitable match */
      int num_inputs = BLI_listbase_count(&node_to->inputs);

      for (int i = 0; i < num_inputs; i++) {

        /* find the best guess input socket */
        bNodeSocket *sock_to = best_socket_input(ntree, node_to, i, replace);
        if (!sock_to) {
          continue;
        }

        /* check for an appropriate output socket to connect from */
        bNodeSocket *sock_fr = best_socket_output(ntree, node_fr, sock_to, allow_multiple);
        if (!sock_fr) {
          continue;
        }

        if (snode_autoconnect_input(snode, node_fr, sock_fr, node_to, sock_to, replace)) {
          numlinks++;
          break;
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Link Viewer Operator
 * \{ */

namespace viewer_linking {

/* Depending on the node tree type, different socket types are supported by viewer nodes. */
static bool socket_can_be_viewed(const OutputSocketRef &socket)
{
  if (nodeSocketIsHidden(socket.bsocket())) {
    return false;
  }
  if (socket.idname() == "NodeSocketVirtual") {
    return false;
  }
  if (socket.tree().btree()->type != NTREE_GEOMETRY) {
    return true;
  }
  return ELEM(socket.typeinfo()->type,
              SOCK_GEOMETRY,
              SOCK_FLOAT,
              SOCK_VECTOR,
              SOCK_INT,
              SOCK_BOOLEAN,
              SOCK_RGBA);
}

static CustomDataType socket_type_to_custom_data_type(const eNodeSocketDatatype socket_type)
{
  switch (socket_type) {
    case SOCK_FLOAT:
      return CD_PROP_FLOAT;
    case SOCK_INT:
      return CD_PROP_INT32;
    case SOCK_VECTOR:
      return CD_PROP_FLOAT3;
    case SOCK_BOOLEAN:
      return CD_PROP_BOOL;
    case SOCK_RGBA:
      return CD_PROP_COLOR;
    default:
      /* Fallback. */
      return CD_AUTO_FROM_NAME;
  }
}

/**
 * Find the socket to link to in a viewer node.
 */
static bNodeSocket *node_link_viewer_get_socket(bNodeTree &ntree,
                                                bNode &viewer_node,
                                                bNodeSocket &src_socket)
{
  if (viewer_node.type != GEO_NODE_VIEWER) {
    /* In viewer nodes in the compositor, only the first input should be linked to. */
    return (bNodeSocket *)viewer_node.inputs.first;
  }
  /* For the geometry nodes viewer, find the socket with the correct type. */
  LISTBASE_FOREACH (bNodeSocket *, viewer_socket, &viewer_node.inputs) {
    if (viewer_socket->type == src_socket.type) {
      if (viewer_socket->type == SOCK_GEOMETRY) {
        return viewer_socket;
      }
      NodeGeometryViewer *storage = (NodeGeometryViewer *)viewer_node.storage;
      const CustomDataType data_type = socket_type_to_custom_data_type(
          (eNodeSocketDatatype)src_socket.type);
      BLI_assert(data_type != CD_AUTO_FROM_NAME);
      storage->data_type = data_type;
      viewer_node.typeinfo->updatefunc(&ntree, &viewer_node);
      return viewer_socket;
    }
  }
  return nullptr;
}

static bool is_viewer_node(const NodeRef &node)
{
  return ELEM(node.bnode()->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER, GEO_NODE_VIEWER);
}

static Vector<const NodeRef *> find_viewer_nodes(const NodeTreeRef &tree)
{
  Vector<const NodeRef *> viewer_nodes;
  for (const NodeRef *node : tree.nodes()) {
    if (is_viewer_node(*node)) {
      viewer_nodes.append(node);
    }
  }
  return viewer_nodes;
}

static bool is_viewer_socket_in_viewer(const InputSocketRef &socket)
{
  const NodeRef &node = socket.node();
  BLI_assert(is_viewer_node(node));
  if (node.typeinfo()->type == GEO_NODE_VIEWER) {
    return true;
  }
  return socket.index() == 0;
}

static bool is_linked_to_viewer(const OutputSocketRef &socket, const NodeRef &viewer_node)
{
  for (const InputSocketRef *target_socket : socket.directly_linked_sockets()) {
    if (&target_socket->node() != &viewer_node) {
      continue;
    }
    if (!target_socket->is_available()) {
      continue;
    }
    if (is_viewer_socket_in_viewer(*target_socket)) {
      return true;
    }
  }
  return false;
}

static int get_default_viewer_type(const bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  return ED_node_is_compositor(snode) ? CMP_NODE_VIEWER : GEO_NODE_VIEWER;
}

static void remove_links_to_unavailable_viewer_sockets(bNodeTree &btree, bNode &viewer_node)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &btree.links) {
    if (link->tonode == &viewer_node) {
      if (link->tosock->flag & SOCK_UNAVAIL) {
        nodeRemLink(&btree, link);
      }
    }
  }
}

static const NodeRef *get_existing_viewer(const NodeTreeRef &tree)
{
  Vector<const NodeRef *> viewer_nodes = find_viewer_nodes(tree);

  /* Check if there is already an active viewer node that should be used. */
  for (const NodeRef *viewer_node : viewer_nodes) {
    if (viewer_node->bnode()->flag & NODE_DO_OUTPUT) {
      return viewer_node;
    }
  }

  /* If no active but non-active viewers exist, make one active. */
  if (!viewer_nodes.is_empty()) {
    viewer_nodes[0]->bnode()->flag |= NODE_DO_OUTPUT;
    return viewer_nodes[0];
  }
  return nullptr;
}

static const OutputSocketRef *find_output_socket_to_be_viewed(const NodeRef *active_viewer_node,
                                                              const NodeRef &node_to_view)
{
  /* Check if any of the output sockets is selected, which is the case when the user just clicked
   * on the socket. */
  for (const OutputSocketRef *output_socket : node_to_view.outputs()) {
    if (output_socket->bsocket()->flag & SELECT) {
      return output_socket;
    }
  }

  const OutputSocketRef *last_socket_linked_to_viewer = nullptr;
  if (active_viewer_node != nullptr) {
    for (const OutputSocketRef *output_socket : node_to_view.outputs()) {
      if (!socket_can_be_viewed(*output_socket)) {
        continue;
      }
      if (is_linked_to_viewer(*output_socket, *active_viewer_node)) {
        last_socket_linked_to_viewer = output_socket;
      }
    }
  }
  if (last_socket_linked_to_viewer == nullptr) {
    /* If no output is connected to a viewer, use the first output that can be viewed. */
    for (const OutputSocketRef *output_socket : node_to_view.outputs()) {
      if (socket_can_be_viewed(*output_socket)) {
        return output_socket;
      }
    }
  }
  else {
    /* Pick the next socket to be linked to the viewer. */
    const int tot_outputs = node_to_view.outputs().size();
    for (const int offset : IndexRange(1, tot_outputs - 1)) {
      const int index = (last_socket_linked_to_viewer->index() + offset) % tot_outputs;
      const OutputSocketRef &output_socket = node_to_view.output(index);
      if (!socket_can_be_viewed(output_socket)) {
        continue;
      }
      if (is_linked_to_viewer(output_socket, *active_viewer_node)) {
        continue;
      }
      return &output_socket;
    }
  }
  return nullptr;
}

static int link_socket_to_viewer(const bContext &C,
                                 bNode *viewer_bnode,
                                 bNode &bnode_to_view,
                                 bNodeSocket &bsocket_to_view)
{
  SpaceNode &snode = *CTX_wm_space_node(&C);
  bNodeTree &btree = *snode.edittree;

  if (viewer_bnode == nullptr) {
    /* Create a new viewer node if none exists. */
    const int viewer_type = get_default_viewer_type(&C);
    viewer_bnode = node_add_node(
        C, nullptr, viewer_type, bsocket_to_view.locx + 100, bsocket_to_view.locy);
    if (viewer_bnode == nullptr) {
      return OPERATOR_CANCELLED;
    }
  }

  bNodeSocket *viewer_bsocket = node_link_viewer_get_socket(btree, *viewer_bnode, bsocket_to_view);
  if (viewer_bsocket == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bNodeLink *link_to_change = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &btree.links) {
    if (link->tosock == viewer_bsocket) {
      link_to_change = link;
      break;
    }
  }

  if (link_to_change == nullptr) {
    nodeAddLink(&btree, &bnode_to_view, &bsocket_to_view, viewer_bnode, viewer_bsocket);
  }
  else {
    link_to_change->fromnode = &bnode_to_view;
    link_to_change->fromsock = &bsocket_to_view;
    BKE_ntree_update_tag_link_changed(&btree);
  }

  remove_links_to_unavailable_viewer_sockets(btree, *viewer_bnode);

  if (btree.type == NTREE_GEOMETRY) {
    ED_spreadsheet_context_paths_set_geometry_node(CTX_data_main(&C), &snode, viewer_bnode);
  }

  ED_node_tree_propagate_change(&C, CTX_data_main(&C), &btree);
  return OPERATOR_FINISHED;
}

static int node_link_viewer(const bContext &C, bNode &bnode_to_view)
{
  SpaceNode &snode = *CTX_wm_space_node(&C);
  bNodeTree *btree = snode.edittree;

  const NodeTreeRef tree{btree};
  const NodeRef &node_to_view = *tree.find_node(bnode_to_view);
  const NodeRef *active_viewer_node = get_existing_viewer(tree);

  const OutputSocketRef *socket_to_view = find_output_socket_to_be_viewed(active_viewer_node,
                                                                          node_to_view);
  if (socket_to_view == nullptr) {
    return OPERATOR_FINISHED;
  }

  bNodeSocket &bsocket_to_view = *socket_to_view->bsocket();
  bNode *viewer_bnode = active_viewer_node ? active_viewer_node->bnode() : nullptr;
  return link_socket_to_viewer(C, viewer_bnode, bnode_to_view, bsocket_to_view);
}

}  // namespace viewer_linking

static int node_active_link_viewer_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNode *node = nodeGetActive(snode.edittree);

  if (!node) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  if (viewer_linking::node_link_viewer(*C, *node) == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  ED_node_tree_propagate_change(C, CTX_data_main(C), snode.edittree);

  return OPERATOR_FINISHED;
}

static bool node_active_link_viewer_poll(bContext *C)
{
  if (!ED_operator_node_editable(C)) {
    return false;
  }
  SpaceNode *snode = CTX_wm_space_node(C);
  return ED_node_is_compositor(snode) || ED_node_is_geometry(snode);
}

void NODE_OT_link_viewer(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Link to Viewer Node";
  ot->description = "Link to viewer node";
  ot->idname = "NODE_OT_link_viewer";

  /* api callbacks */
  ot->exec = node_active_link_viewer_exec;
  ot->poll = node_active_link_viewer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Link Operator
 * \{ */

/**
 * Check if any of the dragged links are connected to a socket on the side that they are dragged
 * from.
 */
static bool dragged_links_are_detached(const bNodeLinkDrag &nldrag)
{
  if (nldrag.in_out == SOCK_OUT) {
    for (const bNodeLink *link : nldrag.links) {
      if (link->tonode && link->tosock) {
        return false;
      }
    }
  }
  else {
    for (const bNodeLink *link : nldrag.links) {
      if (link->fromnode && link->fromsock) {
        return false;
      }
    }
  }
  return true;
}

static bool should_create_drag_link_search_menu(const bNodeTree &node_tree,
                                                const bNodeLinkDrag &nldrag)
{
  /* Custom node trees aren't supported yet. */
  if (node_tree.type == NTREE_CUSTOM) {
    return false;
  }
  /* Only create the search menu when the drag has not already connected the links to a socket. */
  if (!dragged_links_are_detached(nldrag)) {
    return false;
  }
  /* Don't create the search menu if the drag is disconnecting a link from an input node. */
  if (nldrag.start_socket->in_out == SOCK_IN && nldrag.start_link_count > 0) {
    return false;
  }
  /* Don't allow a drag from the "new socket" of a group input node. Handling these
   * properly in node callbacks increases the complexity too much for now. */
  if (ELEM(nldrag.start_node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
    if (nldrag.start_socket->type == SOCK_CUSTOM) {
      return false;
    }
  }
  return true;
}

static void draw_draglink_tooltip(const bContext *UNUSED(C), ARegion *UNUSED(region), void *arg)
{
  bNodeLinkDrag *nldrag = static_cast<bNodeLinkDrag *>(arg);

  const uchar text_col[4] = {255, 255, 255, 255};
  const int padding = 4 * UI_DPI_FAC;
  const float x = nldrag->in_out == SOCK_IN ? nldrag->cursor[0] - 3.3f * padding :
                                              nldrag->cursor[0];
  const float y = nldrag->cursor[1] - 2.0f * UI_DPI_FAC;

  UI_icon_draw_ex(x, y, ICON_ADD, U.inv_dpi_fac, 1.0f, 0.0f, text_col, false);
}

static void draw_draglink_tooltip_activate(const ARegion &region, bNodeLinkDrag &nldrag)
{
  if (nldrag.draw_handle == nullptr) {
    nldrag.draw_handle = ED_region_draw_cb_activate(
        region.type, draw_draglink_tooltip, &nldrag, REGION_DRAW_POST_PIXEL);
  }
}

static void draw_draglink_tooltip_deactivate(const ARegion &region, bNodeLinkDrag &nldrag)
{
  if (nldrag.draw_handle) {
    ED_region_draw_cb_exit(region.type, nldrag.draw_handle);
    nldrag.draw_handle = nullptr;
  }
}

static void node_link_update_header(bContext *C, bNodeLinkDrag *UNUSED(nldrag))
{
  char header[UI_MAX_DRAW_STR];

  BLI_strncpy(header, TIP_("LMB: drag node link, RMB: cancel"), sizeof(header));
  ED_workspace_status_text(C, header);
}

static int node_count_links(const bNodeTree &ntree, const bNodeSocket &socket)
{
  int count = 0;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    if (ELEM(&socket, link->fromsock, link->tosock)) {
      count++;
    }
  }
  return count;
}

static void node_remove_extra_links(SpaceNode &snode, bNodeLink &link)
{
  bNodeTree &ntree = *snode.edittree;
  bNodeSocket &from = *link.fromsock;
  bNodeSocket &to = *link.tosock;
  int to_count = node_count_links(ntree, to);
  int from_count = node_count_links(ntree, from);
  int to_link_limit = nodeSocketLinkLimit(&to);
  int from_link_limit = nodeSocketLinkLimit(&from);

  LISTBASE_FOREACH_MUTABLE (bNodeLink *, tlink, &ntree.links) {
    if (tlink == &link) {
      continue;
    }

    if (tlink && tlink->fromsock == &from) {
      if (from_count > from_link_limit) {
        nodeRemLink(&ntree, tlink);
        tlink = nullptr;
        from_count--;
      }
    }

    if (tlink && tlink->tosock == &to) {
      if (to_count > to_link_limit) {
        nodeRemLink(&ntree, tlink);
        tlink = nullptr;
        to_count--;
      }
      else if (tlink->fromsock == &from) {
        /* Also remove link if it comes from the same output. */
        nodeRemLink(&ntree, tlink);
        tlink = nullptr;
        to_count--;
        from_count--;
      }
    }
  }
}

static void node_link_exit(bContext &C, wmOperator &op, const bool apply_links)
{
  Main *bmain = CTX_data_main(&C);
  ARegion &region = *CTX_wm_region(&C);
  SpaceNode &snode = *CTX_wm_space_node(&C);
  bNodeTree &ntree = *snode.edittree;
  bNodeLinkDrag *nldrag = (bNodeLinkDrag *)op.customdata;

  /* avoid updates while applying links */
  ntree.is_updating = true;
  for (bNodeLink *link : nldrag->links) {
    link->flag &= ~NODE_LINK_DRAGGED;

    if (apply_links && link->tosock && link->fromsock) {
      /* before actually adding the link,
       * let nodes perform special link insertion handling
       */
      if (link->fromnode->typeinfo->insert_link) {
        link->fromnode->typeinfo->insert_link(&ntree, link->fromnode, link);
      }
      if (link->tonode->typeinfo->insert_link) {
        link->tonode->typeinfo->insert_link(&ntree, link->tonode, link);
      }

      /* add link to the node tree */
      BLI_addtail(&ntree.links, link);
      BKE_ntree_update_tag_link_added(&ntree, link);

      /* we might need to remove a link */
      node_remove_extra_links(snode, *link);
    }
    else {
      nodeRemLink(&ntree, link);
    }
  }
  ntree.is_updating = false;

  ED_node_tree_propagate_change(&C, bmain, &ntree);

  /* Ensure draglink tooltip is disabled. */
  draw_draglink_tooltip_deactivate(*CTX_wm_region(&C), *nldrag);

  ED_workspace_status_text(&C, nullptr);
  ED_region_tag_redraw(&region);
  clear_picking_highlight(&snode.edittree->links);

  snode.runtime->linkdrag.reset();
}

static void node_link_find_socket(bContext &C, wmOperator &op, const float2 &cursor)
{
  SpaceNode &snode = *CTX_wm_space_node(&C);
  bNodeLinkDrag *nldrag = (bNodeLinkDrag *)op.customdata;

  if (nldrag->in_out == SOCK_OUT) {
    bNode *tnode;
    bNodeSocket *tsock = nullptr;
    if (node_find_indicated_socket(snode, &tnode, &tsock, cursor, SOCK_IN)) {
      for (bNodeLink *link : nldrag->links) {
        /* skip if socket is on the same node as the fromsock */
        if (tnode && link->fromnode == tnode) {
          continue;
        }

        /* Skip if tsock is already linked with this output. */
        bNodeLink *existing_link_connected_to_fromsock = nullptr;
        LISTBASE_FOREACH (bNodeLink *, existing_link, &snode.edittree->links) {
          if (existing_link->fromsock == link->fromsock && existing_link->tosock == tsock) {
            existing_link_connected_to_fromsock = existing_link;
            break;
          }
        }

        /* attach links to the socket */
        link->tonode = tnode;
        link->tosock = tsock;
        nldrag->last_node_hovered_while_dragging_a_link = tnode;
        if (existing_link_connected_to_fromsock) {
          link->multi_input_socket_index =
              existing_link_connected_to_fromsock->multi_input_socket_index;
          continue;
        }
        if (link->tosock && link->tosock->flag & SOCK_MULTI_INPUT) {
          sort_multi_input_socket_links(snode, *tnode, link, &cursor);
        }
      }
    }
    else {
      for (bNodeLink *link : nldrag->links) {
        if (nldrag->last_node_hovered_while_dragging_a_link) {
          sort_multi_input_socket_links(
              snode, *nldrag->last_node_hovered_while_dragging_a_link, nullptr, &cursor);
        }
        link->tonode = nullptr;
        link->tosock = nullptr;
      }
    }
  }
  else {
    bNode *tnode;
    bNodeSocket *tsock = nullptr;
    if (node_find_indicated_socket(snode, &tnode, &tsock, cursor, SOCK_OUT)) {
      for (bNodeLink *link : nldrag->links) {
        /* skip if this is already the target socket */
        if (link->fromsock == tsock) {
          continue;
        }
        /* skip if socket is on the same node as the fromsock */
        if (tnode && link->tonode == tnode) {
          continue;
        }

        /* attach links to the socket */
        link->fromnode = tnode;
        link->fromsock = tsock;
      }
    }
    else {
      for (bNodeLink *link : nldrag->links) {
        link->fromnode = nullptr;
        link->fromsock = nullptr;
      }
    }
  }
}

/* Loop that adds a node-link, called by function below. */
/* in_out = starting socket */
static int node_link_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  bNodeLinkDrag *nldrag = (bNodeLinkDrag *)op->customdata;
  SpaceNode &snode = *CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);

  UI_view2d_edge_pan_apply_event(C, &nldrag->pan_data, event);

  float2 cursor;
  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &cursor.x, &cursor.y);
  nldrag->cursor[0] = event->mval[0];
  nldrag->cursor[1] = event->mval[1];

  switch (event->type) {
    case MOUSEMOVE:
      if (nldrag->from_multi_input_socket && !RNA_boolean_get(op->ptr, "has_link_picked")) {
        pick_input_link_by_link_intersect(*C, *op, *nldrag, cursor);
      }
      else {
        node_link_find_socket(*C, *op, cursor);

        node_link_update_header(C, nldrag);
        ED_region_tag_redraw(region);
      }

      if (should_create_drag_link_search_menu(*snode.edittree, *nldrag)) {
        draw_draglink_tooltip_activate(*region, *nldrag);
      }
      else {
        draw_draglink_tooltip_deactivate(*region, *nldrag);
      }
      break;
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        /* Add a search menu for compatible sockets if the drag released on empty space. */
        if (should_create_drag_link_search_menu(*snode.edittree, *nldrag)) {
          bNodeLink &link = *nldrag->links.first();
          if (nldrag->in_out == SOCK_OUT) {
            invoke_node_link_drag_add_menu(*C, *link.fromnode, *link.fromsock, cursor);
          }
          else {
            invoke_node_link_drag_add_menu(*C, *link.tonode, *link.tosock, cursor);
          }
        }

        /* Finish link. */
        node_link_exit(*C, *op, true);
        return OPERATOR_FINISHED;
      }
      break;
    case RIGHTMOUSE:
    case MIDDLEMOUSE: {
      if (event->val == KM_RELEASE) {
        node_link_exit(*C, *op, true);
        return OPERATOR_FINISHED;
      }
      break;
    }
    case EVT_ESCKEY: {
      node_link_exit(*C, *op, true);
      return OPERATOR_FINISHED;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static std::unique_ptr<bNodeLinkDrag> node_link_init(SpaceNode &snode,
                                                     float2 cursor,
                                                     const bool detach)
{
  /* output indicated? */
  bNode *node;
  bNodeSocket *sock;
  if (node_find_indicated_socket(snode, &node, &sock, cursor, SOCK_OUT)) {
    std::unique_ptr<bNodeLinkDrag> nldrag = std::make_unique<bNodeLinkDrag>();
    nldrag->start_node = node;
    nldrag->start_socket = sock;
    nldrag->start_link_count = nodeCountSocketLinks(snode.edittree, sock);
    int link_limit = nodeSocketLinkLimit(sock);
    if (nldrag->start_link_count > 0 && (nldrag->start_link_count >= link_limit || detach)) {
      /* dragged links are fixed on input side */
      nldrag->in_out = SOCK_IN;
      /* detach current links and store them in the operator data */
      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &snode.edittree->links) {
        if (link->fromsock == sock) {
          bNodeLink *oplink = MEM_cnew<bNodeLink>("drag link op link");
          *oplink = *link;
          oplink->next = oplink->prev = nullptr;
          oplink->flag |= NODE_LINK_VALID;
          oplink->flag |= NODE_LINK_DRAGGED;

          nldrag->links.append(oplink);
          nodeRemLink(snode.edittree, link);
        }
      }
    }
    else {
      /* dragged links are fixed on output side */
      nldrag->in_out = SOCK_OUT;
      /* create a new link */
      nldrag->links.append(create_drag_link(*node, *sock));
    }
    return nldrag;
  }

  /* or an input? */
  if (node_find_indicated_socket(snode, &node, &sock, cursor, SOCK_IN)) {
    std::unique_ptr<bNodeLinkDrag> nldrag = std::make_unique<bNodeLinkDrag>();
    nldrag->last_node_hovered_while_dragging_a_link = node;
    nldrag->start_node = node;
    nldrag->start_socket = sock;

    nldrag->start_link_count = nodeCountSocketLinks(snode.edittree, sock);
    if (nldrag->start_link_count > 0) {
      /* dragged links are fixed on output side */
      nldrag->in_out = SOCK_OUT;
      /* detach current links and store them in the operator data */
      bNodeLink *link_to_pick;
      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &snode.edittree->links) {
        if (link->tosock == sock) {
          if (sock->flag & SOCK_MULTI_INPUT) {
            nldrag->from_multi_input_socket = true;
          }
          link_to_pick = link;
        }
      }

      if (link_to_pick != nullptr && !nldrag->from_multi_input_socket) {
        bNodeLink *oplink = MEM_cnew<bNodeLink>("drag link op link");
        *oplink = *link_to_pick;
        oplink->next = oplink->prev = nullptr;
        oplink->flag |= NODE_LINK_VALID;
        oplink->flag |= NODE_LINK_DRAGGED;

        nldrag->links.append(oplink);
        nodeRemLink(snode.edittree, link_to_pick);

        /* send changed event to original link->tonode */
        if (node) {
          BKE_ntree_update_tag_node_property(snode.edittree, node);
        }
      }
    }
    else {
      /* dragged links are fixed on input side */
      nldrag->in_out = SOCK_IN;
      /* create a new link */
      nldrag->links.append(create_drag_link(*node, *sock));
    }
    return nldrag;
  }

  return {};
}

static int node_link_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  ARegion &region = *CTX_wm_region(C);

  bool detach = RNA_boolean_get(op->ptr, "detach");

  float2 cursor;
  UI_view2d_region_to_view(&region.v2d, event->mval[0], event->mval[1], &cursor[0], &cursor[1]);
  RNA_float_set_array(op->ptr, "drag_start", cursor);
  RNA_boolean_set(op->ptr, "has_link_picked", false);

  ED_preview_kill_jobs(CTX_wm_manager(C), &bmain);

  std::unique_ptr<bNodeLinkDrag> nldrag = node_link_init(snode, cursor, detach);

  if (nldrag) {
    UI_view2d_edge_pan_operator_init(C, &nldrag->pan_data, op);

    /* Add "+" icon when the link is dragged in empty space. */
    if (should_create_drag_link_search_menu(*snode.edittree, *nldrag)) {
      draw_draglink_tooltip_activate(*CTX_wm_region(C), *nldrag);
    }
    snode.runtime->linkdrag = std::move(nldrag);
    op->customdata = snode.runtime->linkdrag.get();

    /* add modal handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

static void node_link_cancel(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeLinkDrag *nldrag = (bNodeLinkDrag *)op->customdata;

  UI_view2d_edge_pan_cancel(C, &nldrag->pan_data);

  snode->runtime->linkdrag.reset();

  clear_picking_highlight(&snode->edittree->links);
}

void NODE_OT_link(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Link Nodes";
  ot->idname = "NODE_OT_link";
  ot->description = "Use the mouse to create a link between two nodes";

  /* api callbacks */
  ot->invoke = node_link_invoke;
  ot->modal = node_link_modal;
  //  ot->exec = node_link_exec;
  ot->poll = ED_operator_node_editable;
  ot->cancel = node_link_cancel;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  PropertyRNA *prop;

  RNA_def_boolean(ot->srna, "detach", false, "Detach", "Detach and redirect existing links");
  prop = RNA_def_boolean(
      ot->srna,
      "has_link_picked",
      false,
      "Has Link Picked",
      "The operation has placed a link. Only used for multi-input sockets, where the "
      "link is picked later");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_float_array(ot->srna,
                      "drag_start",
                      2,
                      nullptr,
                      -UI_PRECISION_FLOAT_MAX,
                      UI_PRECISION_FLOAT_MAX,
                      "Drag Start",
                      "The position of the mouse cursor at the start of the operation",
                      -UI_PRECISION_FLOAT_MAX,
                      UI_PRECISION_FLOAT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  UI_view2d_edge_pan_operator_properties_ex(ot,
                                            NODE_EDGE_PAN_INSIDE_PAD,
                                            NODE_EDGE_PAN_OUTSIDE_PAD,
                                            NODE_EDGE_PAN_SPEED_RAMP,
                                            NODE_EDGE_PAN_MAX_SPEED,
                                            NODE_EDGE_PAN_DELAY,
                                            NODE_EDGE_PAN_ZOOM_INFLUENCE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Link Operator
 * \{ */

/* makes a link between selected output and input sockets */
static int node_make_link_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  const bool replace = RNA_boolean_get(op->ptr, "replace");

  ED_preview_kill_jobs(CTX_wm_manager(C), &bmain);

  snode_autoconnect(snode, true, replace);

  /* deselect sockets after linking */
  node_deselect_all_input_sockets(snode, false);
  node_deselect_all_output_sockets(snode, false);

  ED_node_tree_propagate_change(C, &bmain, snode.edittree);

  return OPERATOR_FINISHED;
}

void NODE_OT_link_make(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Links";
  ot->description = "Makes a link between selected output in input sockets";
  ot->idname = "NODE_OT_link_make";

  /* callbacks */
  ot->exec = node_make_link_exec;
  /* XXX we need a special poll which checks that there are selected input/output sockets. */
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "replace", false, "Replace", "Replace socket connections with the new links");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Link Intersect
 * \{ */

static bool node_links_intersect(bNodeLink &link, const float mcoords[][2], int tot)
{
  float coord_array[NODE_LINK_RESOL + 1][2];

  if (node_link_bezier_points(nullptr, nullptr, link, coord_array, NODE_LINK_RESOL)) {
    for (int i = 0; i < tot - 1; i++) {
      for (int b = 0; b < NODE_LINK_RESOL; b++) {
        if (isect_seg_seg_v2(mcoords[i], mcoords[i + 1], coord_array[b], coord_array[b + 1]) > 0) {
          return true;
        }
      }
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cut Link Operator
 * \{ */

static int cut_links_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  ARegion &region = *CTX_wm_region(C);

  int i = 0;
  float mcoords[256][2];
  RNA_BEGIN (op->ptr, itemptr, "path") {
    float loc[2];

    RNA_float_get_array(&itemptr, "loc", loc);
    UI_view2d_region_to_view(
        &region.v2d, (int)loc[0], (int)loc[1], &mcoords[i][0], &mcoords[i][1]);
    i++;
    if (i >= 256) {
      break;
    }
  }
  RNA_END;

  if (i > 1) {
    bool found = false;

    ED_preview_kill_jobs(CTX_wm_manager(C), &bmain);

    LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &snode.edittree->links) {
      if (node_link_is_hidden_or_dimmed(region.v2d, *link)) {
        continue;
      }

      if (node_links_intersect(*link, mcoords, i)) {

        if (found == false) {
          /* TODO(sergey): Why did we kill jobs twice? */
          ED_preview_kill_jobs(CTX_wm_manager(C), &bmain);
          found = true;
        }

        bNode *to_node = link->tonode;
        nodeRemLink(snode.edittree, link);
        sort_multi_input_socket_links(snode, *to_node, nullptr, nullptr);
      }
    }

    ED_node_tree_propagate_change(C, CTX_data_main(C), snode.edittree);
    if (found) {
      return OPERATOR_FINISHED;
    }

    return OPERATOR_CANCELLED;
  }

  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

void NODE_OT_links_cut(wmOperatorType *ot)
{
  ot->name = "Cut Links";
  ot->idname = "NODE_OT_links_cut";
  ot->description = "Use the mouse to cut (remove) some links";

  ot->invoke = WM_gesture_lines_invoke;
  ot->modal = WM_gesture_lines_modal;
  ot->exec = cut_links_exec;
  ot->cancel = WM_gesture_lines_cancel;

  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
  RNA_def_property_flag(prop, (PropertyFlag)(PROP_HIDDEN | PROP_SKIP_SAVE));

  /* internal */
  RNA_def_int(ot->srna, "cursor", WM_CURSOR_KNIFE, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mute Links Operator
 * \{ */

static int mute_links_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  ARegion &region = *CTX_wm_region(C);

  int i = 0;
  float mcoords[256][2];
  RNA_BEGIN (op->ptr, itemptr, "path") {
    float loc[2];

    RNA_float_get_array(&itemptr, "loc", loc);
    UI_view2d_region_to_view(
        &region.v2d, (int)loc[0], (int)loc[1], &mcoords[i][0], &mcoords[i][1]);
    i++;
    if (i >= 256) {
      break;
    }
  }
  RNA_END;

  if (i > 1) {
    ED_preview_kill_jobs(CTX_wm_manager(C), &bmain);

    /* Count intersected links and clear test flag. */
    int tot = 0;
    LISTBASE_FOREACH (bNodeLink *, link, &snode.edittree->links) {
      if (node_link_is_hidden_or_dimmed(region.v2d, *link)) {
        continue;
      }
      link->flag &= ~NODE_LINK_TEST;
      if (node_links_intersect(*link, mcoords, i)) {
        tot++;
      }
    }
    if (tot == 0) {
      return OPERATOR_CANCELLED;
    }

    /* Mute links. */
    LISTBASE_FOREACH (bNodeLink *, link, &snode.edittree->links) {
      if (node_link_is_hidden_or_dimmed(region.v2d, *link) || (link->flag & NODE_LINK_TEST)) {
        continue;
      }

      if (node_links_intersect(*link, mcoords, i)) {
        nodeMuteLinkToggle(snode.edittree, link);
      }
    }

    /* Clear remaining test flags. */
    LISTBASE_FOREACH (bNodeLink *, link, &snode.edittree->links) {
      if (node_link_is_hidden_or_dimmed(region.v2d, *link)) {
        continue;
      }
      link->flag &= ~NODE_LINK_TEST;
    }

    ED_node_tree_propagate_change(C, CTX_data_main(C), snode.edittree);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

void NODE_OT_links_mute(wmOperatorType *ot)
{
  ot->name = "Mute Links";
  ot->idname = "NODE_OT_links_mute";
  ot->description = "Use the mouse to mute links";

  ot->invoke = WM_gesture_lines_invoke;
  ot->modal = WM_gesture_lines_modal;
  ot->exec = mute_links_exec;
  ot->cancel = WM_gesture_lines_cancel;

  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
  RNA_def_property_flag(prop, (PropertyFlag)(PROP_HIDDEN | PROP_SKIP_SAVE));

  /* internal */
  RNA_def_int(ot->srna, "cursor", WM_CURSOR_MUTE, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Detach Links Operator
 * \{ */

static int detach_links_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->flag & SELECT) {
      nodeInternalRelink(&ntree, node);
    }
  }

  ED_node_tree_propagate_change(C, CTX_data_main(C), &ntree);
  return OPERATOR_FINISHED;
}

void NODE_OT_links_detach(wmOperatorType *ot)
{
  ot->name = "Detach Links";
  ot->idname = "NODE_OT_links_detach";
  ot->description =
      "Remove all links to selected nodes, and try to connect neighbor nodes together";

  ot->exec = detach_links_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Parent Operator
 * \{ */

static int node_parent_set_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;
  bNode *frame = nodeGetActive(&ntree);
  if (!frame || frame->type != NODE_FRAME) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node == frame) {
      continue;
    }
    if (node->flag & NODE_SELECT) {
      nodeDetachNode(node);
      nodeAttachNode(node, frame);
    }
  }

  node_sort(ntree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_parent_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Parent";
  ot->description = "Attach selected nodes";
  ot->idname = "NODE_OT_parent_set";

  /* api callbacks */
  ot->exec = node_parent_set_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Join Nodes Operator
 * \{ */

/* tags for depth-first search */
#define NODE_JOIN_DONE 1
#define NODE_JOIN_IS_DESCENDANT 2

static void node_join_attach_recursive(bNode *node, bNode *frame)
{
  node->done |= NODE_JOIN_DONE;

  if (node == frame) {
    node->done |= NODE_JOIN_IS_DESCENDANT;
  }
  else if (node->parent) {
    /* call recursively */
    if (!(node->parent->done & NODE_JOIN_DONE)) {
      node_join_attach_recursive(node->parent, frame);
    }

    /* in any case: if the parent is a descendant, so is the child */
    if (node->parent->done & NODE_JOIN_IS_DESCENDANT) {
      node->done |= NODE_JOIN_IS_DESCENDANT;
    }
    else if (node->flag & NODE_TEST) {
      /* if parent is not an descendant of the frame, reattach the node */
      nodeDetachNode(node);
      nodeAttachNode(node, frame);
      node->done |= NODE_JOIN_IS_DESCENDANT;
    }
  }
  else if (node->flag & NODE_TEST) {
    nodeAttachNode(node, frame);
    node->done |= NODE_JOIN_IS_DESCENDANT;
  }
}

static int node_join_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;

  /* XXX save selection: node_add_node call below sets the new frame as single
   * active+selected node */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->flag & NODE_SELECT) {
      node->flag |= NODE_TEST;
    }
    else {
      node->flag &= ~NODE_TEST;
    }
  }

  bNode *frame = node_add_node(*C, nullptr, NODE_FRAME, 0.0f, 0.0f);

  /* reset tags */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    node->done = 0;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (!(node->done & NODE_JOIN_DONE)) {
      node_join_attach_recursive(node, frame);
    }
  }

  /* restore selection */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->flag & NODE_TEST) {
      node->flag |= NODE_SELECT;
    }
  }

  node_sort(ntree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_join(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Join Nodes";
  ot->description = "Attach selected nodes to a new common frame";
  ot->idname = "NODE_OT_join";

  /* api callbacks */
  ot->exec = node_join_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attach Operator
 * \{ */

static bNode *node_find_frame_to_attach(ARegion &region,
                                        const bNodeTree &ntree,
                                        const int mouse_xy[2])
{
  /* convert mouse coordinates to v2d space */
  float cursor[2];
  UI_view2d_region_to_view(&region.v2d, UNPACK2(mouse_xy), &cursor[0], &cursor[1]);

  LISTBASE_FOREACH_BACKWARD (bNode *, frame, &ntree.nodes) {
    /* skip selected, those are the nodes we want to attach */
    if ((frame->type != NODE_FRAME) || (frame->flag & NODE_SELECT)) {
      continue;
    }
    if (BLI_rctf_isect_pt_v(&frame->totr, cursor)) {
      return frame;
    }
  }

  return nullptr;
}

static int node_attach_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  ARegion &region = *CTX_wm_region(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;
  bNode *frame = node_find_frame_to_attach(region, ntree, event->mval);

  if (frame) {
    LISTBASE_FOREACH_BACKWARD (bNode *, node, &ntree.nodes) {
      if (node->flag & NODE_SELECT) {
        if (node->parent == nullptr) {
          /* disallow moving a parent into its child */
          if (nodeAttachNodeCheck(frame, node) == false) {
            /* attach all unparented nodes */
            nodeAttachNode(node, frame);
          }
        }
        else {
          /* attach nodes which share parent with the frame */
          bNode *parent;
          for (parent = frame->parent; parent; parent = parent->parent) {
            if (parent == node->parent) {
              break;
            }
          }

          if (parent) {
            /* disallow moving a parent into its child */
            if (nodeAttachNodeCheck(frame, node) == false) {
              nodeDetachNode(node);
              nodeAttachNode(node, frame);
            }
          }
        }
      }
    }
  }

  node_sort(ntree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_attach(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Attach Nodes";
  ot->description = "Attach active node to a frame";
  ot->idname = "NODE_OT_attach";

  /* api callbacks */

  ot->invoke = node_attach_invoke;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Detach Operator
 * \{ */

/* tags for depth-first search */
#define NODE_DETACH_DONE 1
#define NODE_DETACH_IS_DESCENDANT 2

static void node_detach_recursive(bNode *node)
{
  node->done |= NODE_DETACH_DONE;

  if (node->parent) {
    /* call recursively */
    if (!(node->parent->done & NODE_DETACH_DONE)) {
      node_detach_recursive(node->parent);
    }

    /* in any case: if the parent is a descendant, so is the child */
    if (node->parent->done & NODE_DETACH_IS_DESCENDANT) {
      node->done |= NODE_DETACH_IS_DESCENDANT;
    }
    else if (node->flag & NODE_SELECT) {
      /* if parent is not a descendant of a selected node, detach */
      nodeDetachNode(node);
      node->done |= NODE_DETACH_IS_DESCENDANT;
    }
  }
  else if (node->flag & NODE_SELECT) {
    node->done |= NODE_DETACH_IS_DESCENDANT;
  }
}

/* detach the root nodes in the current selection */
static int node_detach_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;

  /* reset tags */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    node->done = 0;
  }
  /* detach nodes recursively
   * relative order is preserved here!
   */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (!(node->done & NODE_DETACH_DONE)) {
      node_detach_recursive(node);
    }
  }

  node_sort(ntree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_detach(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Detach Nodes";
  ot->description = "Detach selected nodes from parents";
  ot->idname = "NODE_OT_detach";

  /* api callbacks */
  ot->exec = node_detach_exec;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Automatic Node Insert on Dragging
 * \{ */

/* prevent duplicate testing code below */
static bool ed_node_link_conditions(ScrArea *area,
                                    bool test,
                                    SpaceNode **r_snode,
                                    bNode **r_select)
{
  SpaceNode *snode = area ? (SpaceNode *)area->spacedata.first : nullptr;

  *r_snode = snode;
  *r_select = nullptr;

  /* no unlucky accidents */
  if (area == nullptr || area->spacetype != SPACE_NODE) {
    return false;
  }

  if (!test) {
    /* no need to look for a node */
    return true;
  }

  bNode *node;
  bNode *select = nullptr;
  for (node = (bNode *)snode->edittree->nodes.first; node; node = node->next) {
    if (node->flag & SELECT) {
      if (select) {
        break;
      }
      select = node;
    }
  }
  /* only one selected */
  if (node || select == nullptr) {
    return false;
  }

  /* correct node */
  if (BLI_listbase_is_empty(&select->inputs) || BLI_listbase_is_empty(&select->outputs)) {
    return false;
  }

  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);

  /* test node for links */
  LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
    if (node_link_is_hidden_or_dimmed(region->v2d, *link)) {
      continue;
    }

    if (link->tonode == select || link->fromnode == select) {
      return false;
    }
  }

  *r_select = select;
  return true;
}

}  // namespace blender::ed::space_node

void ED_node_link_intersect_test(ScrArea *area, int test)
{
  using namespace blender::ed::space_node;

  bNode *select;
  SpaceNode *snode;
  if (!ed_node_link_conditions(area, test, &snode, &select)) {
    return;
  }

  /* clear flags */
  LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
    link->flag &= ~NODE_LINKFLAG_HILITE;
  }

  if (test == 0) {
    return;
  }

  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);

  /* find link to select/highlight */
  bNodeLink *selink = nullptr;
  float dist_best = FLT_MAX;
  LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
    float coord_array[NODE_LINK_RESOL + 1][2];

    if (node_link_is_hidden_or_dimmed(region->v2d, *link)) {
      continue;
    }

    if (node_link_bezier_points(nullptr, nullptr, *link, coord_array, NODE_LINK_RESOL)) {
      float dist = FLT_MAX;

      /* loop over link coords to find shortest dist to
       * upper left node edge of a intersected line segment */
      for (int i = 0; i < NODE_LINK_RESOL; i++) {
        /* Check if the node rectangle intersects the line from this point to next one. */
        if (BLI_rctf_isect_segment(&select->totr, coord_array[i], coord_array[i + 1])) {
          /* store the shortest distance to the upper left edge
           * of all intersections found so far */
          const float node_xy[] = {select->totr.xmin, select->totr.ymax};

          /* to be precise coord_array should be clipped by select->totr,
           * but not done since there's no real noticeable difference */
          dist = min_ff(
              dist_squared_to_line_segment_v2(node_xy, coord_array[i], coord_array[i + 1]), dist);
        }
      }

      /* we want the link with the shortest distance to node center */
      if (dist < dist_best) {
        dist_best = dist;
        selink = link;
      }
    }
  }

  if (selink) {
    selink->flag |= NODE_LINKFLAG_HILITE;
  }
}

namespace blender::ed::space_node {

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Insert Offset Operator
 * \{ */

static int get_main_socket_priority(const bNodeSocket *socket)
{
  switch ((eNodeSocketDatatype)socket->type) {
    case __SOCK_MESH:
      return -1;
    case SOCK_CUSTOM:
      return 0;
    case SOCK_BOOLEAN:
      return 1;
    case SOCK_INT:
      return 2;
    case SOCK_FLOAT:
      return 3;
    case SOCK_VECTOR:
      return 4;
    case SOCK_RGBA:
      return 5;
    case SOCK_STRING:
    case SOCK_SHADER:
    case SOCK_OBJECT:
    case SOCK_IMAGE:
    case SOCK_GEOMETRY:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_MATERIAL:
      return 6;
  }
  return -1;
}

/** Get the "main" socket based on the node declaration or an heuristic. */
static bNodeSocket *get_main_socket(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out)
{
  ListBase *sockets = (in_out == SOCK_IN) ? &node.inputs : &node.outputs;

  /* Try to get the main socket based on the socket declaration. */
  nodeDeclarationEnsure(&ntree, &node);
  const nodes::NodeDeclaration *node_decl = node.declaration;
  if (node_decl != nullptr) {
    Span<nodes::SocketDeclarationPtr> socket_decls = (in_out == SOCK_IN) ? node_decl->inputs() :
                                                                           node_decl->outputs();
    int index;
    LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, sockets, index) {
      const nodes::SocketDeclaration &socket_decl = *socket_decls[index];
      if (nodeSocketIsHidden(socket)) {
        continue;
      }
      if (socket_decl.is_default_link_socket()) {
        return socket;
      }
    }
  }

  /* find priority range */
  int maxpriority = -1;
  LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
    if (sock->flag & SOCK_UNAVAIL) {
      continue;
    }
    maxpriority = max_ii(get_main_socket_priority(sock), maxpriority);
  }

  /* try all priorities, starting from 'highest' */
  for (int priority = maxpriority; priority >= 0; priority--) {
    LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
      if (!nodeSocketIsHidden(sock) && priority == get_main_socket_priority(sock)) {
        return sock;
      }
    }
  }

  /* no visible sockets, unhide first of highest priority */
  for (int priority = maxpriority; priority >= 0; priority--) {
    LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
      if (sock->flag & SOCK_UNAVAIL) {
        continue;
      }
      if (priority == get_main_socket_priority(sock)) {
        sock->flag &= ~SOCK_HIDDEN;
        return sock;
      }
    }
  }

  return nullptr;
}

static bool node_parents_offset_flag_enable_cb(bNode *parent, void *UNUSED(userdata))
{
  /* NODE_TEST is used to flag nodes that shouldn't be offset (again) */
  parent->flag |= NODE_TEST;

  return true;
}

static void node_offset_apply(bNode &node, const float offset_x)
{
  /* NODE_TEST is used to flag nodes that shouldn't be offset (again) */
  if ((node.flag & NODE_TEST) == 0) {
    node.anim_init_locx = node.locx;
    node.anim_ofsx = (offset_x / UI_DPI_FAC);
    node.flag |= NODE_TEST;
  }
}

static void node_parent_offset_apply(NodeInsertOfsData *data, bNode *parent, const float offset_x)
{
  node_offset_apply(*parent, offset_x);

  /* Flag all children as offset to prevent them from being offset
   * separately (they've already moved with the parent). */
  LISTBASE_FOREACH (bNode *, node, &data->ntree->nodes) {
    if (nodeIsChildOf(parent, node)) {
      /* NODE_TEST is used to flag nodes that shouldn't be offset (again) */
      node->flag |= NODE_TEST;
    }
  }
}

#define NODE_INSOFS_ANIM_DURATION 0.25f

/**
 * Callback that applies #NodeInsertOfsData.offset_x to a node or its parent, similar
 * to node_link_insert_offset_output_chain_cb below, but with slightly different logic
 */
static bool node_link_insert_offset_frame_chain_cb(bNode *fromnode,
                                                   bNode *tonode,
                                                   void *userdata,
                                                   const bool reversed)
{
  NodeInsertOfsData *data = (NodeInsertOfsData *)userdata;
  bNode *ofs_node = reversed ? fromnode : tonode;

  if (ofs_node->parent && ofs_node->parent != data->insert_parent) {
    node_offset_apply(*ofs_node->parent, data->offset_x);
  }
  else {
    node_offset_apply(*ofs_node, data->offset_x);
  }

  return true;
}

/**
 * Applies #NodeInsertOfsData.offset_x to all children of \a parent.
 */
static void node_link_insert_offset_frame_chains(const bNodeTree *ntree,
                                                 const bNode *parent,
                                                 NodeInsertOfsData *data,
                                                 const bool reversed)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (nodeIsChildOf(parent, node)) {
      nodeChainIter(ntree, node, node_link_insert_offset_frame_chain_cb, data, reversed);
    }
  }
}

/**
 * Callback that applies NodeInsertOfsData.offset_x to a node or its parent,
 * considering the logic needed for offsetting nodes after link insert
 */
static bool node_link_insert_offset_chain_cb(bNode *fromnode,
                                             bNode *tonode,
                                             void *userdata,
                                             const bool reversed)
{
  NodeInsertOfsData *data = (NodeInsertOfsData *)userdata;
  bNode *ofs_node = reversed ? fromnode : tonode;

  if (data->insert_parent) {
    if (ofs_node->parent && (ofs_node->parent->flag & NODE_TEST) == 0) {
      node_parent_offset_apply(data, ofs_node->parent, data->offset_x);
      node_link_insert_offset_frame_chains(data->ntree, ofs_node->parent, data, reversed);
    }
    else {
      node_offset_apply(*ofs_node, data->offset_x);
    }

    if (nodeIsChildOf(data->insert_parent, ofs_node) == false) {
      data->insert_parent = nullptr;
    }
  }
  else if (ofs_node->parent) {
    bNode *node = nodeFindRootParent(ofs_node);
    node_offset_apply(*node, data->offset_x);
  }
  else {
    node_offset_apply(*ofs_node, data->offset_x);
  }

  return true;
}

static void node_link_insert_offset_ntree(NodeInsertOfsData *iofsd,
                                          ARegion *region,
                                          const int mouse_xy[2],
                                          const bool right_alignment)
{
  bNodeTree *ntree = iofsd->ntree;
  bNode &insert = *iofsd->insert;
  bNode *prev = iofsd->prev, *next = iofsd->next;
  bNode *init_parent = insert.parent; /* store old insert.parent for restoring later */

  const float min_margin = U.node_margin * UI_DPI_FAC;
  const float width = NODE_WIDTH(insert);
  const bool needs_alignment = (next->totr.xmin - prev->totr.xmax) < (width + (min_margin * 2.0f));

  float margin = width;

  /* NODE_TEST will be used later, so disable for all nodes */
  ntreeNodeFlagSet(ntree, NODE_TEST, false);

  /* `insert.totr` isn't updated yet,
   * so `totr_insert` is used to get the correct world-space coords. */
  rctf totr_insert;
  node_to_updated_rect(insert, totr_insert);

  /* frame attachment wasn't handled yet
   * so we search the frame that the node will be attached to later */
  insert.parent = node_find_frame_to_attach(*region, *ntree, mouse_xy);

  /* this makes sure nodes are also correctly offset when inserting a node on top of a frame
   * without actually making it a part of the frame (because mouse isn't intersecting it)
   * - logic here is similar to node_find_frame_to_attach */
  if (!insert.parent ||
      (prev->parent && (prev->parent == next->parent) && (prev->parent != insert.parent))) {
    bNode *frame;
    rctf totr_frame;

    /* check nodes front to back */
    for (frame = (bNode *)ntree->nodes.last; frame; frame = frame->prev) {
      /* skip selected, those are the nodes we want to attach */
      if ((frame->type != NODE_FRAME) || (frame->flag & NODE_SELECT)) {
        continue;
      }

      /* for some reason frame y coords aren't correct yet */
      node_to_updated_rect(*frame, totr_frame);

      if (BLI_rctf_isect_x(&totr_frame, totr_insert.xmin) &&
          BLI_rctf_isect_x(&totr_frame, totr_insert.xmax)) {
        if (BLI_rctf_isect_y(&totr_frame, totr_insert.ymin) ||
            BLI_rctf_isect_y(&totr_frame, totr_insert.ymax)) {
          /* frame isn't insert.parent actually, but this is needed to make offsetting
           * nodes work correctly for above checked cases (it is restored later) */
          insert.parent = frame;
          break;
        }
      }
    }
  }

  /* *** ensure offset at the left (or right for right_alignment case) of insert_node *** */

  float dist = right_alignment ? totr_insert.xmin - prev->totr.xmax :
                                 next->totr.xmin - totr_insert.xmax;
  /* distance between insert_node and prev is smaller than min margin */
  if (dist < min_margin) {
    const float addval = (min_margin - dist) * (right_alignment ? 1.0f : -1.0f);

    node_offset_apply(insert, addval);

    totr_insert.xmin += addval;
    totr_insert.xmax += addval;
    margin += min_margin;
  }

  /* *** ensure offset at the right (or left for right_alignment case) of insert_node *** */

  dist = right_alignment ? next->totr.xmin - totr_insert.xmax : totr_insert.xmin - prev->totr.xmax;
  /* distance between insert_node and next is smaller than min margin */
  if (dist < min_margin) {
    const float addval = (min_margin - dist) * (right_alignment ? 1.0f : -1.0f);
    if (needs_alignment) {
      bNode *offs_node = right_alignment ? next : prev;
      if (!offs_node->parent || offs_node->parent == insert.parent ||
          nodeIsChildOf(offs_node->parent, &insert)) {
        node_offset_apply(*offs_node, addval);
      }
      else if (!insert.parent && offs_node->parent) {
        node_offset_apply(*nodeFindRootParent(offs_node), addval);
      }
      margin = addval;
    }
    /* enough room is available, but we want to ensure the min margin at the right */
    else {
      /* offset inserted node so that min margin is kept at the right */
      node_offset_apply(insert, -addval);
    }
  }

  if (needs_alignment) {
    iofsd->insert_parent = insert.parent;
    iofsd->offset_x = margin;

    /* flag all parents of insert as offset to prevent them from being offset */
    nodeParentsIter(&insert, node_parents_offset_flag_enable_cb, nullptr);
    /* iterate over entire chain and apply offsets */
    nodeChainIter(ntree,
                  right_alignment ? next : prev,
                  node_link_insert_offset_chain_cb,
                  iofsd,
                  !right_alignment);
  }

  insert.parent = init_parent;
}

/**
 * Modal handler for insert offset animation
 */
static int node_insert_offset_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  NodeInsertOfsData *iofsd = snode->runtime->iofsd;
  bool redraw = false;

  if (!snode || event->type != TIMER || iofsd == nullptr ||
      iofsd->anim_timer != event->customdata) {
    return OPERATOR_PASS_THROUGH;
  }

  const float duration = (float)iofsd->anim_timer->duration;

  /* handle animation - do this before possibly aborting due to duration, since
   * main thread might be so busy that node hasn't reached final position yet */
  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if (UNLIKELY(node->anim_ofsx)) {
      const float endval = node->anim_init_locx + node->anim_ofsx;
      if (IS_EQF(node->locx, endval) == false) {
        node->locx = BLI_easing_cubic_ease_in_out(
            duration, node->anim_init_locx, node->anim_ofsx, NODE_INSOFS_ANIM_DURATION);
        if (node->anim_ofsx < 0) {
          CLAMP_MIN(node->locx, endval);
        }
        else {
          CLAMP_MAX(node->locx, endval);
        }
        redraw = true;
      }
    }
  }
  if (redraw) {
    ED_region_tag_redraw(CTX_wm_region(C));
  }

  /* end timer + free insert offset data */
  if (duration > NODE_INSOFS_ANIM_DURATION) {
    WM_event_remove_timer(CTX_wm_manager(C), nullptr, iofsd->anim_timer);

    LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
      node->anim_init_locx = node->anim_ofsx = 0.0f;
    }

    snode->runtime->iofsd = nullptr;
    MEM_freeN(iofsd);

    return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
  }

  return OPERATOR_RUNNING_MODAL;
}

#undef NODE_INSOFS_ANIM_DURATION

static int node_insert_offset_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  NodeInsertOfsData *iofsd = snode->runtime->iofsd;

  if (!iofsd || !iofsd->insert) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert((snode->flag & SNODE_SKIP_INSOFFSET) == 0);

  iofsd->ntree = snode->edittree;
  iofsd->anim_timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.02);

  node_link_insert_offset_ntree(
      iofsd, CTX_wm_region(C), event->mval, (snode->insert_ofs_dir == SNODE_INSERTOFS_DIR_RIGHT));

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void NODE_OT_insert_offset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Offset";
  ot->description = "Automatically offset nodes on insertion";
  ot->idname = "NODE_OT_insert_offset";

  /* callbacks */
  ot->invoke = node_insert_offset_invoke;
  ot->modal = node_insert_offset_modal;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;
}

/** \} */

}  // namespace blender::ed::space_node

/* -------------------------------------------------------------------- */
/** \name Note Link Insert
 * \{ */

void ED_node_link_insert(Main *bmain, ScrArea *area)
{
  using namespace blender::ed::space_node;

  bNode *node_to_insert;
  SpaceNode *snode;
  if (!ed_node_link_conditions(area, true, &snode, &node_to_insert)) {
    return;
  }

  /* Find link to insert on. */
  bNodeTree &ntree = *snode->edittree;
  bNodeLink *old_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    if (link->flag & NODE_LINKFLAG_HILITE) {
      old_link = link;
      break;
    }
  }
  if (old_link == nullptr) {
    return;
  }

  old_link->flag &= ~NODE_LINKFLAG_HILITE;

  bNodeSocket *best_input = get_main_socket(ntree, *node_to_insert, SOCK_IN);
  bNodeSocket *best_output = get_main_socket(ntree, *node_to_insert, SOCK_OUT);

  if (node_to_insert->type != NODE_REROUTE) {
    /* Ignore main sockets when the types don't match. */
    if (best_input != nullptr && ntree.typeinfo->validate_link != nullptr &&
        !ntree.typeinfo->validate_link(static_cast<eNodeSocketDatatype>(old_link->fromsock->type),
                                       static_cast<eNodeSocketDatatype>(best_input->type))) {
      best_input = nullptr;
    }
    if (best_output != nullptr && ntree.typeinfo->validate_link != nullptr &&
        !ntree.typeinfo->validate_link(static_cast<eNodeSocketDatatype>(best_output->type),
                                       static_cast<eNodeSocketDatatype>(old_link->tosock->type))) {
      best_output = nullptr;
    }
  }

  bNode *from_node = old_link->fromnode;
  bNodeSocket *from_socket = old_link->fromsock;
  bNode *to_node = old_link->tonode;

  if (best_output != nullptr) {
    /* Relink the "start" of the existing link to the newly inserted node. */
    old_link->fromnode = node_to_insert;
    old_link->fromsock = best_output;
    BKE_ntree_update_tag_link_changed(&ntree);
  }
  else {
    nodeRemLink(&ntree, old_link);
  }

  if (best_input != nullptr) {
    /* Add a new link that connects the node on the left to the newly inserted node. */
    nodeAddLink(&ntree, from_node, from_socket, node_to_insert, best_input);
  }

  /* Set up insert offset data, it needs stuff from here. */
  if ((snode->flag & SNODE_SKIP_INSOFFSET) == 0) {
    NodeInsertOfsData *iofsd = MEM_cnew<NodeInsertOfsData>(__func__);

    iofsd->insert = node_to_insert;
    iofsd->prev = from_node;
    iofsd->next = to_node;

    snode->runtime->iofsd = iofsd;
  }

  ED_node_tree_propagate_change(nullptr, bmain, snode->edittree);
}

/** \} */
