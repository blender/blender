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

#include "BLI_blenlib.h"
#include "BLI_easing.h"
#include "BLI_math.h"

#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "ED_node.h" /* own include */
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "BLT_translation.h"

#include "node_intern.h" /* own include */

/* ****************** Relations helpers *********************** */

static bool ntree_has_drivers(bNodeTree *ntree)
{
  const AnimData *adt = BKE_animdata_from_id(&ntree->id);
  if (adt == NULL) {
    return false;
  }
  return !BLI_listbase_is_empty(&adt->drivers);
}

static bool ntree_check_nodes_connected_dfs(bNodeTree *ntree, bNode *from, bNode *to)
{
  if (from->flag & NODE_TEST) {
    return false;
  }
  from->flag |= NODE_TEST;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->fromnode == from) {
      if (link->tonode == to) {
        return true;
      }

      if (ntree_check_nodes_connected_dfs(ntree, link->tonode, to)) {
        return true;
      }
    }
  }
  return false;
}

static bool ntree_check_nodes_connected(bNodeTree *ntree, bNode *from, bNode *to)
{
  if (from == to) {
    return true;
  }
  ntreeNodeFlagSet(ntree, NODE_TEST, false);
  return ntree_check_nodes_connected_dfs(ntree, from, to);
}

static bool node_group_has_output_dfs(bNode *node)
{
  bNodeTree *ntree = (bNodeTree *)node->id;
  if (ntree->id.tag & LIB_TAG_DOIT) {
    return false;
  }
  ntree->id.tag |= LIB_TAG_DOIT;
  for (bNode *current_node = ntree->nodes.first; current_node != NULL;
       current_node = current_node->next) {
    if (current_node->type == NODE_GROUP) {
      if (current_node->id && node_group_has_output_dfs(current_node)) {
        return true;
      }
    }
    if (current_node->flag & NODE_DO_OUTPUT && current_node->type != NODE_GROUP_OUTPUT) {
      return true;
    }
  }
  return false;
}

static bool node_group_has_output(Main *bmain, bNode *node)
{
  BLI_assert(ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP));
  bNodeTree *ntree = (bNodeTree *)node->id;
  if (ntree == NULL) {
    return false;
  }
  BKE_main_id_tag_listbase(&bmain->nodetrees, LIB_TAG_DOIT, false);
  return node_group_has_output_dfs(node);
}

bool node_connected_to_output(Main *bmain, bNodeTree *ntree, bNode *node)
{
  /* Special case for drivers: if node tree has any drivers we assume it is
   * always to be tagged for update when node changes. Otherwise we will be
   * doomed to do some deep and nasty deep search of indirect dependencies,
   * which will be too complicated without real benefit.
   */
  if (ntree_has_drivers(ntree)) {
    return true;
  }
  LISTBASE_FOREACH (bNode *, current_node, &ntree->nodes) {
    /* Special case for group nodes -- if modified node connected to a group
     * with active output inside we consider refresh is needed.
     *
     * We could make check more grained here by taking which socket the node
     * is connected to and so eventually.
     */
    if (ELEM(current_node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) {
      if (current_node->id != NULL && ntree_has_drivers((bNodeTree *)current_node->id)) {
        return true;
      }
      if (ntree_check_nodes_connected(ntree, node, current_node) &&
          node_group_has_output(bmain, current_node)) {
        return true;
      }
    }
    if (current_node->flag & NODE_DO_OUTPUT) {
      if (ntree_check_nodes_connected(ntree, node, current_node)) {
        return true;
      }
    }
  }
  return false;
}

/* ****************** Add *********************** */

typedef struct bNodeListItem {
  struct bNodeListItem *next, *prev;
  struct bNode *node;
} bNodeListItem;

typedef struct NodeInsertOfsData {
  bNodeTree *ntree;
  bNode *insert;      /* inserted node */
  bNode *prev, *next; /* prev/next node in the chain */
  bNode *insert_parent;

  wmTimer *anim_timer;

  float offset_x; /* offset to apply to node chain */
} NodeInsertOfsData;

static void clear_picking_highlight(ListBase *links)
{
  LISTBASE_FOREACH (bNodeLink *, link, links) {
    link->flag &= ~NODE_LINK_TEMP_HIGHLIGHT;
  }
}

static LinkData *create_drag_link(Main *bmain, SpaceNode *snode, bNode *node, bNodeSocket *sock)
{
  LinkData *linkdata = MEM_callocN(sizeof(LinkData), "drag link op link data");
  bNodeLink *oplink = MEM_callocN(sizeof(bNodeLink), "drag link op link");
  linkdata->data = oplink;
  if (sock->in_out == SOCK_OUT) {
    oplink->fromnode = node;
    oplink->fromsock = sock;
  }
  else {
    oplink->tonode = node;
    oplink->tosock = sock;
  }
  oplink->flag |= NODE_LINK_VALID;
  oplink->flag &= ~NODE_LINK_TEST;
  if (node_connected_to_output(bmain, snode->edittree, node)) {
    oplink->flag |= NODE_LINK_TEST;
  }
  return linkdata;
}

static void pick_link(const bContext *C,
                      wmOperator *op,
                      bNodeLinkDrag *nldrag,
                      SpaceNode *snode,
                      bNode *node,
                      bNodeLink *link_to_pick)
{
  clear_picking_highlight(&snode->edittree->links);
  RNA_boolean_set(op->ptr, "has_link_picked", true);

  Main *bmain = CTX_data_main(C);
  LinkData *linkdata = create_drag_link(
      bmain, snode, link_to_pick->fromnode, link_to_pick->fromsock);

  BLI_addtail(&nldrag->links, linkdata);
  nodeRemLink(snode->edittree, link_to_pick);
  /* Send changed event to original link->tonode. */
  if (node) {
    snode_update(snode, node);
  }
}

static void pick_input_link_by_link_intersect(const bContext *C,
                                              wmOperator *op,
                                              bNodeLinkDrag *nldrag,
                                              const float *cursor)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  const ARegion *region = CTX_wm_region(C);
  const View2D *v2d = &region->v2d;

  float drag_start[2];
  RNA_float_get_array(op->ptr, "drag_start", drag_start);
  bNode *node;
  bNodeSocket *socket;
  node_find_indicated_socket(snode, &node, &socket, drag_start, SOCK_IN);

  const float trigger_drag_distance = 25.0f;
  const float cursor_link_touch_distance = 25.0f;

  const float socket_height = node_socket_calculate_height(socket);

  float cursor_to_socket_relative[2];
  float socket_position[2] = {socket->locx, socket->locy};
  sub_v2_v2v2(cursor_to_socket_relative, cursor, socket_position);
  float distance_from_socket_v2[2] = {
      max_ff(0, fabs(cursor_to_socket_relative[0]) - NODE_SOCKSIZE * 0.5),
      max_ff(0, fabs(cursor_to_socket_relative[1]) - socket_height)};
  const float distance_from_socket = len_v2(distance_from_socket_v2);

  const int resolution = NODE_LINK_RESOL;

  bNodeLink *link_to_pick = NULL;
  clear_picking_highlight(&snode->edittree->links);
  LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
    if (link->tosock == socket) {
      /* Test if the cursor is near a link. */
      float vec[4][2];
      node_link_bezier_handles(v2d, snode, link, vec);

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
          nldrag->last_picked_multi_input_socket_link = link_to_pick;
        }
      }
    }
  }

  /* If no linked was picked in this call, try using the one picked in the previous call.
   * Not essential for the basic behavior, but can make interaction feel a bit better if
   * the  mouse moves to the right and loses the "selection." */
  if (!link_to_pick) {
    bNodeLink *last_picked_link = nldrag->last_picked_multi_input_socket_link;
    if (last_picked_link) {
      link_to_pick = last_picked_link;
    }
  }

  if (link_to_pick) {
    /* Highlight is set here and cleared in the next iteration or if the operation finishes. */
    link_to_pick->flag |= NODE_LINK_TEMP_HIGHLIGHT;
    ED_area_tag_redraw(CTX_wm_area(C));

    if (distance_from_socket > trigger_drag_distance) {
      pick_link(C, op, nldrag, snode, node, link_to_pick);
    }
  }
}

static int sort_nodes_locx(const void *a, const void *b)
{
  const bNodeListItem *nli1 = a;
  const bNodeListItem *nli2 = b;
  const bNode *node1 = nli1->node;
  const bNode *node2 = nli2->node;

  if (node1->locx > node2->locx) {
    return 1;
  }
  return 0;
}

static bool socket_is_available(bNodeTree *UNUSED(ntree), bNodeSocket *sock, const bool allow_used)
{
  if (nodeSocketIsHidden(sock)) {
    return 0;
  }

  if (!allow_used && (sock->flag & SOCK_IN_USE)) {
    return 0;
  }

  return 1;
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
    return node->outputs.first;
  }

  return NULL;
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

  return NULL;
}

static bool snode_autoconnect_input(SpaceNode *snode,
                                    bNode *node_fr,
                                    bNodeSocket *sock_fr,
                                    bNode *node_to,
                                    bNodeSocket *sock_to,
                                    int replace)
{
  bNodeTree *ntree = snode->edittree;

  /* then we can connect */
  if (replace) {
    nodeRemSocketLinks(ntree, sock_to);
  }

  nodeAddLink(ntree, node_fr, sock_fr, node_to, sock_to);
  return true;
}

typedef struct LinkAndPosition {
  struct bNodeLink *link;
  float multi_socket_position[2];
} LinkAndPosition;

static int compare_link_by_y_position(const void *a, const void *b)
{
  const LinkAndPosition *link_and_position_a = *(const LinkAndPosition **)a;
  const LinkAndPosition *link_and_position_b = *(const LinkAndPosition **)b;

  BLI_assert(link_and_position_a->link->tosock == link_and_position_b->link->tosock);
  const float link_a_y = link_and_position_a->multi_socket_position[1];
  const float link_b_y = link_and_position_b->multi_socket_position[1];
  return link_a_y > link_b_y ? 1 : -1;
}

void sort_multi_input_socket_links(SpaceNode *snode,
                                   bNode *node,
                                   bNodeLink *drag_link,
                                   float cursor[2])
{
  LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
    if (!(socket->flag & SOCK_MULTI_INPUT)) {
      continue;
    }
    /* The total is calculated in #node_update_nodetree, which runs before this draw step. */
    int total_inputs = socket->total_inputs + 1;
    struct LinkAndPosition **input_links = MEM_malloc_arrayN(
        total_inputs, sizeof(LinkAndPosition *), __func__);

    int index = 0;
    LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
      if (link->tosock == socket) {
        struct LinkAndPosition *link_and_position = MEM_callocN(sizeof(struct LinkAndPosition),
                                                                __func__);
        link_and_position->link = link;
        node_link_calculate_multi_input_position(link->tosock->locx,
                                                 link->tosock->locy,
                                                 link->multi_input_socket_index,
                                                 link->tosock->total_inputs,
                                                 link_and_position->multi_socket_position);
        input_links[index] = link_and_position;
        index++;
      }
    }

    if (drag_link) {
      LinkAndPosition *link_and_position = MEM_callocN(sizeof(LinkAndPosition), __func__);
      link_and_position->link = drag_link;
      copy_v2_v2(link_and_position->multi_socket_position, cursor);
      input_links[index] = link_and_position;
      index++;
    }

    qsort(input_links, index, sizeof(bNodeLink *), compare_link_by_y_position);

    for (int i = 0; i < index; i++) {
      input_links[i]->link->multi_input_socket_index = i;
    }

    for (int i = 0; i < index; i++) {
      if (input_links[i]) {
        MEM_freeN(input_links[i]);
      }
    }
    MEM_freeN(input_links);
  }
}

static void snode_autoconnect(Main *bmain,
                              SpaceNode *snode,
                              const bool allow_multiple,
                              const bool replace)
{
  bNodeTree *ntree = snode->edittree;
  ListBase *nodelist = MEM_callocN(sizeof(ListBase), "items_list");

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & NODE_SELECT) {
      bNodeListItem *nli = MEM_mallocN(sizeof(bNodeListItem), "temporary node list item");
      nli->node = node;
      BLI_addtail(nodelist, nli);
    }
  }

  /* sort nodes left to right */
  BLI_listbase_sort(nodelist, sort_nodes_locx);

  int numlinks = 0;
  LISTBASE_FOREACH (bNodeListItem *, nli, nodelist) {
    bool has_selected_inputs = false;

    if (nli->next == NULL) {
      break;
    }

    bNode *node_fr = nli->node;
    bNode *node_to = nli->next->node;
    /* corner case: input/output node aligned the wrong way around (T47729) */
    if (BLI_listbase_is_empty(&node_to->inputs) || BLI_listbase_is_empty(&node_fr->outputs)) {
      SWAP(bNode *, node_fr, node_to);
    }

    /* if there are selected sockets, connect those */
    LISTBASE_FOREACH (bNodeSocket *, sock_to, &node_to->inputs) {
      if (sock_to->flag & SELECT) {
        has_selected_inputs = 1;

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

  if (numlinks > 0) {
    ntreeUpdateTree(bmain, ntree);
  }

  BLI_freelistN(nodelist);
  MEM_freeN(nodelist);
}

/* *************************** link viewer op ******************** */

static int node_link_viewer(const bContext *C, bNode *tonode)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  /* context check */
  if (tonode == NULL || BLI_listbase_is_empty(&tonode->outputs)) {
    return OPERATOR_CANCELLED;
  }
  if (ELEM(tonode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
    return OPERATOR_CANCELLED;
  }

  /* get viewer */
  bNode *viewer_node = NULL;
  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
      if (node->flag & NODE_DO_OUTPUT) {
        viewer_node = node;
        break;
      }
    }
  }
  /* no viewer, we make one active */
  if (viewer_node == NULL) {
    LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
      if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
        node->flag |= NODE_DO_OUTPUT;
        viewer_node = node;
        break;
      }
    }
  }

  bNodeSocket *sock = NULL;
  bNodeLink *link = NULL;

  /* try to find an already connected socket to cycle to the next */
  if (viewer_node) {
    link = NULL;

    for (link = snode->edittree->links.first; link; link = link->next) {
      if (link->tonode == viewer_node && link->fromnode == tonode) {
        if (link->tosock == viewer_node->inputs.first) {
          break;
        }
      }
    }
    if (link) {
      /* unlink existing connection */
      sock = link->fromsock;
      nodeRemLink(snode->edittree, link);

      /* find a socket after the previously connected socket */
      for (sock = sock->next; sock; sock = sock->next) {
        if (!nodeSocketIsHidden(sock)) {
          break;
        }
      }
    }
  }

  if (tonode) {
    /* Find a selected socket that overrides the socket to connect to */
    LISTBASE_FOREACH (bNodeSocket *, sock2, &tonode->outputs) {
      if (!nodeSocketIsHidden(sock2) && sock2->flag & SELECT) {
        sock = sock2;
        break;
      }
    }
  }

  /* find a socket starting from the first socket */
  if (!sock) {
    for (sock = tonode->outputs.first; sock; sock = sock->next) {
      if (!nodeSocketIsHidden(sock)) {
        break;
      }
    }
  }

  if (sock) {
    /* add a new viewer if none exists yet */
    if (!viewer_node) {
      /* XXX location is a quick hack, just place it next to the linked socket */
      viewer_node = node_add_node(C, NULL, CMP_NODE_VIEWER, sock->locx + 100, sock->locy);
      if (!viewer_node) {
        return OPERATOR_CANCELLED;
      }

      link = NULL;
    }
    else {
      /* get link to viewer */
      for (link = snode->edittree->links.first; link; link = link->next) {
        if (link->tonode == viewer_node && link->tosock == viewer_node->inputs.first) {
          break;
        }
      }
    }

    if (link == NULL) {
      nodeAddLink(snode->edittree, tonode, sock, viewer_node, viewer_node->inputs.first);
    }
    else {
      link->fromnode = tonode;
      link->fromsock = sock;
      /* make sure the dependency sorting is updated */
      snode->edittree->update |= NTREE_UPDATE_LINKS;
    }
    ntreeUpdateTree(CTX_data_main(C), snode->edittree);
    snode_update(snode, viewer_node);
  }

  return OPERATOR_FINISHED;
}

static int node_active_link_viewer_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNode *node = nodeGetActive(snode->edittree);

  if (!node) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  if (node_link_viewer(C, node) == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  snode_notify(C, snode);

  return OPERATOR_FINISHED;
}

void NODE_OT_link_viewer(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Link to Viewer Node";
  ot->description = "Link to viewer node";
  ot->idname = "NODE_OT_link_viewer";

  /* api callbacks */
  ot->exec = node_active_link_viewer_exec;
  ot->poll = composite_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *************************** add link op ******************** */

static void node_link_update_header(bContext *C, bNodeLinkDrag *UNUSED(nldrag))
{
  char header[UI_MAX_DRAW_STR];

  BLI_strncpy(header, TIP_("LMB: drag node link, RMB: cancel"), sizeof(header));
  ED_workspace_status_text(C, header);
}

static int node_count_links(bNodeTree *ntree, bNodeSocket *sock)
{
  int count = 0;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->fromsock == sock) {
      count++;
    }
    if (link->tosock == sock) {
      count++;
    }
  }
  return count;
}

static void node_remove_extra_links(SpaceNode *snode, bNodeLink *link)
{
  bNodeTree *ntree = snode->edittree;
  bNodeSocket *from = link->fromsock, *to = link->tosock;
  int to_count = node_count_links(ntree, to);
  int from_count = node_count_links(ntree, from);
  int to_link_limit = nodeSocketLinkLimit(to);
  int from_link_limit = nodeSocketLinkLimit(from);

  LISTBASE_FOREACH_MUTABLE (bNodeLink *, tlink, &ntree->links) {
    if (tlink == link) {
      continue;
    }

    if (tlink && tlink->fromsock == from) {
      if (from_count > from_link_limit) {
        nodeRemLink(ntree, tlink);
        tlink = NULL;
        from_count--;
      }
    }

    if (tlink && tlink->tosock == to) {
      if (to_count > to_link_limit) {
        nodeRemLink(ntree, tlink);
        tlink = NULL;
        to_count--;
      }
      else if (tlink->fromsock == from) {
        /* Also remove link if it comes from the same output. */
        nodeRemLink(ntree, tlink);
        tlink = NULL;
        to_count--;
        from_count--;
      }
    }
  }
}

static void node_link_exit(bContext *C, wmOperator *op, bool apply_links)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  bNodeLinkDrag *nldrag = op->customdata;
  bool do_tag_update = false;

  /* avoid updates while applying links */
  ntree->is_updating = true;
  LISTBASE_FOREACH (LinkData *, linkdata, &nldrag->links) {
    bNodeLink *link = linkdata->data;

    /* See note below, but basically TEST flag means that the link
     * was connected to output (or to a node which affects the
     * output).
     */
    do_tag_update |= (link->flag & NODE_LINK_TEST) != 0;

    if (apply_links && link->tosock && link->fromsock) {
      /* before actually adding the link,
       * let nodes perform special link insertion handling
       */
      if (link->fromnode->typeinfo->insert_link) {
        link->fromnode->typeinfo->insert_link(ntree, link->fromnode, link);
      }
      if (link->tonode->typeinfo->insert_link) {
        link->tonode->typeinfo->insert_link(ntree, link->tonode, link);
      }

      /* add link to the node tree */
      BLI_addtail(&ntree->links, link);

      ntree->update |= NTREE_UPDATE_LINKS;

      /* tag tonode for update */
      link->tonode->update |= NODE_UPDATE;

      /* we might need to remove a link */
      node_remove_extra_links(snode, link);

      if (link->tonode) {
        do_tag_update |= (do_tag_update || node_connected_to_output(bmain, ntree, link->tonode));
      }
    }
    else {
      nodeRemLink(ntree, link);
    }
  }
  ntree->is_updating = false;

  do_tag_update |= ED_node_is_geometry(snode);

  ntreeUpdateTree(bmain, ntree);
  snode_notify(C, snode);
  if (do_tag_update) {
    snode_dag_update(C, snode);
  }

  BLI_remlink(&snode->runtime->linkdrag, nldrag);
  /* links->data pointers are either held by the tree or freed already */
  BLI_freelistN(&nldrag->links);
  MEM_freeN(nldrag);
}

static void node_link_find_socket(bContext *C, wmOperator *op, float cursor[2])
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeLinkDrag *nldrag = op->customdata;

  if (nldrag->in_out == SOCK_OUT) {
    bNode *tnode;
    bNodeSocket *tsock = NULL;
    if (node_find_indicated_socket(snode, &tnode, &tsock, cursor, SOCK_IN)) {
      LISTBASE_FOREACH (LinkData *, linkdata, &nldrag->links) {
        bNodeLink *link = linkdata->data;

        /* skip if socket is on the same node as the fromsock */
        if (tnode && link->fromnode == tnode) {
          continue;
        }

        /* Skip if tsock is already linked with this output. */
        bNodeLink *existing_link_connected_to_fromsock = NULL;
        LISTBASE_FOREACH (bNodeLink *, existing_link, &snode->edittree->links) {
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
        sort_multi_input_socket_links(snode, tnode, link, cursor);
      }
    }
    else {
      LISTBASE_FOREACH (LinkData *, linkdata, &nldrag->links) {
        bNodeLink *link = linkdata->data;
        if (nldrag->last_node_hovered_while_dragging_a_link) {
          sort_multi_input_socket_links(
              snode, nldrag->last_node_hovered_while_dragging_a_link, NULL, cursor);
        }
        link->tonode = NULL;
        link->tosock = NULL;
      }
    }
  }
  else {
    bNode *tnode;
    bNodeSocket *tsock = NULL;
    if (node_find_indicated_socket(snode, &tnode, &tsock, cursor, SOCK_OUT)) {
      LISTBASE_FOREACH (LinkData *, linkdata, &nldrag->links) {
        bNodeLink *link = linkdata->data;

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
      LISTBASE_FOREACH (LinkData *, linkdata, &nldrag->links) {
        bNodeLink *link = linkdata->data;

        link->fromnode = NULL;
        link->fromsock = NULL;
      }
    }
  }
}

/* Loop that adds a node-link, called by function below. */
/* in_out = starting socket */
static int node_link_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  bNodeLinkDrag *nldrag = op->customdata;
  ARegion *region = CTX_wm_region(C);
  float cursor[2];

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &cursor[0], &cursor[1]);

  switch (event->type) {
    case MOUSEMOVE:
      if (nldrag->from_multi_input_socket && !RNA_boolean_get(op->ptr, "has_link_picked")) {
        pick_input_link_by_link_intersect(C, op, nldrag, cursor);
      }
      else {
        node_link_find_socket(C, op, cursor);

        node_link_update_header(C, nldrag);
        ED_region_tag_redraw(region);
      }
      break;

    case LEFTMOUSE:
    case RIGHTMOUSE:
    case MIDDLEMOUSE: {
      if (event->val == KM_RELEASE) {
        node_link_exit(C, op, true);

        ED_workspace_status_text(C, NULL);
        ED_region_tag_redraw(region);
        SpaceNode *snode = CTX_wm_space_node(C);
        clear_picking_highlight(&snode->edittree->links);
        return OPERATOR_FINISHED;
      }
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* return 1 when socket clicked */
static bNodeLinkDrag *node_link_init(Main *bmain, SpaceNode *snode, float cursor[2], bool detach)
{
  bNodeLinkDrag *nldrag = NULL;

  /* output indicated? */
  bNode *node;
  bNodeSocket *sock;
  if (node_find_indicated_socket(snode, &node, &sock, cursor, SOCK_OUT)) {
    nldrag = MEM_callocN(sizeof(bNodeLinkDrag), "drag link op customdata");

    const int num_links = nodeCountSocketLinks(snode->edittree, sock);
    int link_limit = nodeSocketLinkLimit(sock);
    if (num_links > 0 && (num_links >= link_limit || detach)) {
      /* dragged links are fixed on input side */
      nldrag->in_out = SOCK_IN;
      /* detach current links and store them in the operator data */
      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &snode->edittree->links) {
        if (link->fromsock == sock) {
          LinkData *linkdata = MEM_callocN(sizeof(LinkData), "drag link op link data");
          bNodeLink *oplink = MEM_callocN(sizeof(bNodeLink), "drag link op link");
          linkdata->data = oplink;
          *oplink = *link;
          oplink->next = oplink->prev = NULL;
          oplink->flag |= NODE_LINK_VALID;

          /* The link could be disconnected and in that case we
           * wouldn't be able to check whether tag update is
           * needed or not when releasing mouse button. So we
           * cache whether the link affects output or not here
           * using TEST flag.
           */
          oplink->flag &= ~NODE_LINK_TEST;
          if (node_connected_to_output(bmain, snode->edittree, link->tonode)) {
            oplink->flag |= NODE_LINK_TEST;
          }

          BLI_addtail(&nldrag->links, linkdata);
          nodeRemLink(snode->edittree, link);
        }
      }
    }
    else {
      /* dragged links are fixed on output side */
      nldrag->in_out = SOCK_OUT;
      /* create a new link */
      LinkData *linkdata = create_drag_link(bmain, snode, node, sock);

      BLI_addtail(&nldrag->links, linkdata);
    }
  }
  /* or an input? */
  else if (node_find_indicated_socket(snode, &node, &sock, cursor, SOCK_IN)) {
    nldrag = MEM_callocN(sizeof(bNodeLinkDrag), "drag link op customdata");

    const int num_links = nodeCountSocketLinks(snode->edittree, sock);
    if (num_links > 0) {
      /* dragged links are fixed on output side */
      nldrag->in_out = SOCK_OUT;
      /* detach current links and store them in the operator data */
      bNodeLink *link_to_pick;
      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &snode->edittree->links) {
        if (link->tosock == sock) {
          if (sock->flag & SOCK_MULTI_INPUT) {
            nldrag->from_multi_input_socket = true;
          }
          link_to_pick = link;
        }
      }

      if (link_to_pick != NULL && !nldrag->from_multi_input_socket) {
        LinkData *linkdata = MEM_callocN(sizeof(LinkData), "drag link op link data");
        bNodeLink *oplink = MEM_callocN(sizeof(bNodeLink), "drag link op link");
        linkdata->data = oplink;
        *oplink = *link_to_pick;
        oplink->next = oplink->prev = NULL;
        oplink->flag |= NODE_LINK_VALID;
        oplink->flag &= ~NODE_LINK_TEST;
        if (node_connected_to_output(bmain, snode->edittree, link_to_pick->tonode)) {
          oplink->flag |= NODE_LINK_TEST;
        }

        BLI_addtail(&nldrag->links, linkdata);
        nodeRemLink(snode->edittree, link_to_pick);

        /* send changed event to original link->tonode */
        if (node) {
          snode_update(snode, node);
        }
      }
    }
    else {
      /* dragged links are fixed on input side */
      nldrag->in_out = SOCK_IN;
      /* create a new link */
      LinkData *linkdata = create_drag_link(bmain, snode, node, sock);

      BLI_addtail(&nldrag->links, linkdata);
    }
  }

  return nldrag;
}

static int node_link_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);

  bool detach = RNA_boolean_get(op->ptr, "detach");

  float cursor[2];
  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &cursor[0], &cursor[1]);
  RNA_float_set_array(op->ptr, "drag_start", cursor);
  RNA_boolean_set(op->ptr, "has_link_picked", false);

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  bNodeLinkDrag *nldrag = node_link_init(bmain, snode, cursor, detach);

  if (nldrag) {
    op->customdata = nldrag;
    BLI_addtail(&snode->runtime->linkdrag, nldrag);

    /* add modal handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

static void node_link_cancel(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeLinkDrag *nldrag = op->customdata;

  BLI_remlink(&snode->runtime->linkdrag, nldrag);

  BLI_freelistN(&nldrag->links);
  MEM_freeN(nldrag);
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
                      0,
                      -UI_PRECISION_FLOAT_MAX,
                      UI_PRECISION_FLOAT_MAX,
                      "Drag Start",
                      "The position of the mouse cursor at the start of the operation",
                      -UI_PRECISION_FLOAT_MAX,
                      UI_PRECISION_FLOAT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* ********************** Make Link operator ***************** */

/* makes a link between selected output and input sockets */
static int node_make_link_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  const bool replace = RNA_boolean_get(op->ptr, "replace");

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  snode_autoconnect(bmain, snode, 1, replace);

  /* deselect sockets after linking */
  node_deselect_all_input_sockets(snode, 0);
  node_deselect_all_output_sockets(snode, 0);

  ntreeUpdateTree(CTX_data_main(C), snode->edittree);
  snode_notify(C, snode);
  snode_dag_update(C, snode);

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
      ot->srna, "replace", 0, "Replace", "Replace socket connections with the new links");
}

/* ********************** Node Link Intersect ***************** */
static bool node_links_intersect(bNodeLink *link, const float mcoords[][2], int tot)
{
  float coord_array[NODE_LINK_RESOL + 1][2];

  if (node_link_bezier_points(NULL, NULL, link, coord_array, NODE_LINK_RESOL)) {
    for (int i = 0; i < tot - 1; i++) {
      for (int b = 0; b < NODE_LINK_RESOL; b++) {
        if (isect_seg_seg_v2(mcoords[i], mcoords[i + 1], coord_array[b], coord_array[b + 1]) > 0) {
          return 1;
        }
      }
    }
  }
  return 0;
}

/* ********************** Cut Link operator ***************** */
static int cut_links_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  bool do_tag_update = false;

  int i = 0;
  float mcoords[256][2];
  RNA_BEGIN (op->ptr, itemptr, "path") {
    float loc[2];

    RNA_float_get_array(&itemptr, "loc", loc);
    UI_view2d_region_to_view(
        &region->v2d, (int)loc[0], (int)loc[1], &mcoords[i][0], &mcoords[i][1]);
    i++;
    if (i >= 256) {
      break;
    }
  }
  RNA_END;

  if (i > 1) {
    bool found = false;

    ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

    LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &snode->edittree->links) {
      if (nodeLinkIsHidden(link)) {
        continue;
      }

      if (node_links_intersect(link, mcoords, i)) {

        if (found == false) {
          /* TODO(sergey): Why did we kill jobs twice? */
          ED_preview_kill_jobs(CTX_wm_manager(C), bmain);
          found = true;
        }

        do_tag_update |= (do_tag_update ||
                          node_connected_to_output(bmain, snode->edittree, link->tonode));

        snode_update(snode, link->tonode);
        nodeRemLink(snode->edittree, link);
      }
    }

    do_tag_update |= ED_node_is_geometry(snode);

    if (found) {
      ntreeUpdateTree(CTX_data_main(C), snode->edittree);
      snode_notify(C, snode);
      if (do_tag_update) {
        snode_dag_update(C, snode);
      }

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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  /* internal */
  RNA_def_int(ot->srna, "cursor", WM_CURSOR_KNIFE, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

/* ********************** Mute links operator ***************** */

static int mute_links_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  bool do_tag_update = false;

  int i = 0;
  float mcoords[256][2];
  RNA_BEGIN (op->ptr, itemptr, "path") {
    float loc[2];

    RNA_float_get_array(&itemptr, "loc", loc);
    UI_view2d_region_to_view(
        &region->v2d, (int)loc[0], (int)loc[1], &mcoords[i][0], &mcoords[i][1]);
    i++;
    if (i >= 256) {
      break;
    }
  }
  RNA_END;

  if (i > 1) {
    ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

    /* Count intersected links and clear test flag. */
    int tot = 0;
    LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
      if (nodeLinkIsHidden(link)) {
        continue;
      }
      link->flag &= ~NODE_LINK_TEST;
      if (node_links_intersect(link, mcoords, i)) {
        tot++;
      }
    }
    if (tot == 0) {
      return OPERATOR_CANCELLED;
    }

    /* Mute links. */
    LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
      if (nodeLinkIsHidden(link) || (link->flag & NODE_LINK_TEST)) {
        continue;
      }

      if (node_links_intersect(link, mcoords, i)) {
        do_tag_update |= (do_tag_update ||
                          node_connected_to_output(bmain, snode->edittree, link->tonode));

        snode_update(snode, link->tonode);
        nodeMuteLinkToggle(snode->edittree, link);
      }
    }

    /* Clear remaining test flags. */
    LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
      if (nodeLinkIsHidden(link)) {
        continue;
      }
      link->flag &= ~NODE_LINK_TEST;
    }

    do_tag_update |= ED_node_is_geometry(snode);

    ntreeUpdateTree(CTX_data_main(C), snode->edittree);
    snode_notify(C, snode);
    if (do_tag_update) {
      snode_dag_update(C, snode);
    }

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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  /* internal */
  RNA_def_int(ot->srna, "cursor", WM_CURSOR_MUTE, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

/* ********************** Detach links operator ***************** */

static int detach_links_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & SELECT) {
      nodeInternalRelink(ntree, node);
    }
  }

  ntreeUpdateTree(CTX_data_main(C), ntree);

  snode_notify(C, snode);
  snode_dag_update(C, snode);

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

/* ****************** Set Parent ******************* */

static int node_parent_set_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  bNode *frame = nodeGetActive(ntree);
  if (!frame || frame->type != NODE_FRAME) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node == frame) {
      continue;
    }
    if (node->flag & NODE_SELECT) {
      nodeDetachNode(node);
      nodeAttachNode(node, frame);
    }
  }

  ED_node_sort(ntree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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

/* ****************** Join Nodes ******************* */

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
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  /* XXX save selection: node_add_node call below sets the new frame as single
   * active+selected node */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & NODE_SELECT) {
      node->flag |= NODE_TEST;
    }
    else {
      node->flag &= ~NODE_TEST;
    }
  }

  bNode *frame = node_add_node(C, NULL, NODE_FRAME, 0.0f, 0.0f);

  /* reset tags */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->done = 0;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (!(node->done & NODE_JOIN_DONE)) {
      node_join_attach_recursive(node, frame);
    }
  }

  /* restore selection */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & NODE_TEST) {
      node->flag |= NODE_SELECT;
    }
  }

  ED_node_sort(ntree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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

/* ****************** Attach ******************* */

static bNode *node_find_frame_to_attach(ARegion *region,
                                        const bNodeTree *ntree,
                                        const int mouse_xy[2])
{
  /* convert mouse coordinates to v2d space */
  float cursor[2];
  UI_view2d_region_to_view(&region->v2d, UNPACK2(mouse_xy), &cursor[0], &cursor[1]);

  LISTBASE_FOREACH_BACKWARD (bNode *, frame, &ntree->nodes) {
    /* skip selected, those are the nodes we want to attach */
    if ((frame->type != NODE_FRAME) || (frame->flag & NODE_SELECT)) {
      continue;
    }
    if (BLI_rctf_isect_pt_v(&frame->totr, cursor)) {
      return frame;
    }
  }

  return NULL;
}

static int node_attach_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  bNode *frame = node_find_frame_to_attach(region, ntree, event->mval);

  if (frame) {
    LISTBASE_FOREACH_BACKWARD (bNode *, node, &ntree->nodes) {
      if (node->flag & NODE_SELECT) {
        if (node->parent == NULL) {
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

  ED_node_sort(ntree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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

/* ****************** Detach ******************* */

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
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  /* reset tags */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->done = 0;
  }
  /* detach nodes recursively
   * relative order is preserved here!
   */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (!(node->done & NODE_DETACH_DONE)) {
      node_detach_recursive(node);
    }
  }

  ED_node_sort(ntree);
  WM_event_add_notifier(C, NC_NODE | ND_DISPLAY, NULL);

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

/* *********************  automatic node insert on dragging ******************* */

/* prevent duplicate testing code below */
static bool ed_node_link_conditions(ScrArea *area,
                                    bool test,
                                    SpaceNode **r_snode,
                                    bNode **r_select)
{
  SpaceNode *snode = area ? area->spacedata.first : NULL;

  *r_snode = snode;
  *r_select = NULL;

  /* no unlucky accidents */
  if (area == NULL || area->spacetype != SPACE_NODE) {
    return false;
  }

  if (!test) {
    /* no need to look for a node */
    return true;
  }

  bNode *node;
  bNode *select = NULL;
  for (node = snode->edittree->nodes.first; node; node = node->next) {
    if (node->flag & SELECT) {
      if (select) {
        break;
      }
      select = node;
    }
  }
  /* only one selected */
  if (node || select == NULL) {
    return false;
  }

  /* correct node */
  if (BLI_listbase_is_empty(&select->inputs) || BLI_listbase_is_empty(&select->outputs)) {
    return false;
  }

  /* test node for links */
  LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
    if (nodeLinkIsHidden(link)) {
      continue;
    }

    if (link->tonode == select || link->fromnode == select) {
      return false;
    }
  }

  *r_select = select;
  return true;
}

/* test == 0, clear all intersect flags */
void ED_node_link_intersect_test(ScrArea *area, int test)
{
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

  /* find link to select/highlight */
  bNodeLink *selink = NULL;
  float dist_best = FLT_MAX;
  LISTBASE_FOREACH (bNodeLink *, link, &snode->edittree->links) {
    float coord_array[NODE_LINK_RESOL + 1][2];

    if (nodeLinkIsHidden(link)) {
      continue;
    }

    if (node_link_bezier_points(NULL, NULL, link, coord_array, NODE_LINK_RESOL)) {
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

/* assumes sockets in list */
static bNodeSocket *socket_best_match(ListBase *sockets)
{
  /* find type range */
  int maxtype = 0;
  LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
    maxtype = max_ii(sock->type, maxtype);
  }

  /* try all types, starting from 'highest' (i.e. colors, vectors, values) */
  for (int type = maxtype; type >= 0; type--) {
    LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
      if (!nodeSocketIsHidden(sock) && type == sock->type) {
        return sock;
      }
    }
  }

  /* no visible sockets, unhide first of highest type */
  for (int type = maxtype; type >= 0; type--) {
    LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
      if (type == sock->type) {
        sock->flag &= ~SOCK_HIDDEN;
        return sock;
      }
    }
  }

  return NULL;
}

static bool node_parents_offset_flag_enable_cb(bNode *parent, void *UNUSED(userdata))
{
  /* NODE_TEST is used to flag nodes that shouldn't be offset (again) */
  parent->flag |= NODE_TEST;

  return true;
}

static void node_offset_apply(bNode *node, const float offset_x)
{
  /* NODE_TEST is used to flag nodes that shouldn't be offset (again) */
  if ((node->flag & NODE_TEST) == 0) {
    node->anim_init_locx = node->locx;
    node->anim_ofsx = (offset_x / UI_DPI_FAC);
    node->flag |= NODE_TEST;
  }
}

static void node_parent_offset_apply(NodeInsertOfsData *data, bNode *parent, const float offset_x)
{
  node_offset_apply(parent, offset_x);

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
 * Callback that applies NodeInsertOfsData.offset_x to a node or its parent, similar
 * to node_link_insert_offset_output_chain_cb below, but with slightly different logic
 */
static bool node_link_insert_offset_frame_chain_cb(bNode *fromnode,
                                                   bNode *tonode,
                                                   void *userdata,
                                                   const bool reversed)
{
  NodeInsertOfsData *data = userdata;
  bNode *ofs_node = reversed ? fromnode : tonode;

  if (ofs_node->parent && ofs_node->parent != data->insert_parent) {
    node_offset_apply(ofs_node->parent, data->offset_x);
  }
  else {
    node_offset_apply(ofs_node, data->offset_x);
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
  NodeInsertOfsData *data = userdata;
  bNode *ofs_node = reversed ? fromnode : tonode;

  if (data->insert_parent) {
    if (ofs_node->parent && (ofs_node->parent->flag & NODE_TEST) == 0) {
      node_parent_offset_apply(data, ofs_node->parent, data->offset_x);
      node_link_insert_offset_frame_chains(data->ntree, ofs_node->parent, data, reversed);
    }
    else {
      node_offset_apply(ofs_node, data->offset_x);
    }

    if (nodeIsChildOf(data->insert_parent, ofs_node) == false) {
      data->insert_parent = NULL;
    }
  }
  else if (ofs_node->parent) {
    bNode *node = nodeFindRootParent(ofs_node);
    node_offset_apply(node, data->offset_x);
  }
  else {
    node_offset_apply(ofs_node, data->offset_x);
  }

  return true;
}

static void node_link_insert_offset_ntree(NodeInsertOfsData *iofsd,
                                          ARegion *region,
                                          const int mouse_xy[2],
                                          const bool right_alignment)
{
  bNodeTree *ntree = iofsd->ntree;
  bNode *insert = iofsd->insert;
  bNode *prev = iofsd->prev, *next = iofsd->next;
  bNode *init_parent = insert->parent; /* store old insert->parent for restoring later */

  const float min_margin = U.node_margin * UI_DPI_FAC;
  const float width = NODE_WIDTH(insert);
  const bool needs_alignment = (next->totr.xmin - prev->totr.xmax) < (width + (min_margin * 2.0f));

  float margin = width;

  /* NODE_TEST will be used later, so disable for all nodes */
  ntreeNodeFlagSet(ntree, NODE_TEST, false);

  /* insert->totr isn't updated yet,
   * so totr_insert is used to get the correct worldspace coords */
  rctf totr_insert;
  node_to_updated_rect(insert, &totr_insert);

  /* frame attachment wasn't handled yet
   * so we search the frame that the node will be attached to later */
  insert->parent = node_find_frame_to_attach(region, ntree, mouse_xy);

  /* this makes sure nodes are also correctly offset when inserting a node on top of a frame
   * without actually making it a part of the frame (because mouse isn't intersecting it)
   * - logic here is similar to node_find_frame_to_attach */
  if (!insert->parent ||
      (prev->parent && (prev->parent == next->parent) && (prev->parent != insert->parent))) {
    bNode *frame;
    rctf totr_frame;

    /* check nodes front to back */
    for (frame = ntree->nodes.last; frame; frame = frame->prev) {
      /* skip selected, those are the nodes we want to attach */
      if ((frame->type != NODE_FRAME) || (frame->flag & NODE_SELECT)) {
        continue;
      }

      /* for some reason frame y coords aren't correct yet */
      node_to_updated_rect(frame, &totr_frame);

      if (BLI_rctf_isect_x(&totr_frame, totr_insert.xmin) &&
          BLI_rctf_isect_x(&totr_frame, totr_insert.xmax)) {
        if (BLI_rctf_isect_y(&totr_frame, totr_insert.ymin) ||
            BLI_rctf_isect_y(&totr_frame, totr_insert.ymax)) {
          /* frame isn't insert->parent actually, but this is needed to make offsetting
           * nodes work correctly for above checked cases (it is restored later) */
          insert->parent = frame;
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
      if (!offs_node->parent || offs_node->parent == insert->parent ||
          nodeIsChildOf(offs_node->parent, insert)) {
        node_offset_apply(offs_node, addval);
      }
      else if (!insert->parent && offs_node->parent) {
        node_offset_apply(nodeFindRootParent(offs_node), addval);
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
    iofsd->insert_parent = insert->parent;
    iofsd->offset_x = margin;

    /* flag all parents of insert as offset to prevent them from being offset */
    nodeParentsIter(insert, node_parents_offset_flag_enable_cb, NULL);
    /* iterate over entire chain and apply offsets */
    nodeChainIter(ntree,
                  right_alignment ? next : prev,
                  node_link_insert_offset_chain_cb,
                  iofsd,
                  !right_alignment);
  }

  insert->parent = init_parent;
}

/**
 * Modal handler for insert offset animation
 */
static int node_insert_offset_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  NodeInsertOfsData *iofsd = snode->runtime->iofsd;
  bool redraw = false;

  if (!snode || event->type != TIMER || iofsd == NULL || iofsd->anim_timer != event->customdata) {
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
    WM_event_remove_timer(CTX_wm_manager(C), NULL, iofsd->anim_timer);

    LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
      node->anim_init_locx = node->anim_ofsx = 0.0f;
    }

    snode->runtime->iofsd = NULL;
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

/* assumes link with NODE_LINKFLAG_HILITE set */
void ED_node_link_insert(Main *bmain, ScrArea *area)
{
  bNode *select;
  SpaceNode *snode;
  if (!ed_node_link_conditions(area, true, &snode, &select)) {
    return;
  }

  /* get the link */
  bNodeLink *link;
  for (link = snode->edittree->links.first; link; link = link->next) {
    if (link->flag & NODE_LINKFLAG_HILITE) {
      break;
    }
  }

  if (link) {
    bNodeSocket *best_input = socket_best_match(&select->inputs);
    bNodeSocket *best_output = socket_best_match(&select->outputs);

    if (best_input && best_output) {
      bNode *node = link->tonode;
      bNodeSocket *sockto = link->tosock;

      link->tonode = select;
      link->tosock = best_input;
      node_remove_extra_links(snode, link);
      link->flag &= ~NODE_LINKFLAG_HILITE;

      nodeAddLink(snode->edittree, select, best_output, node, sockto);

      /* set up insert offset data, it needs stuff from here */
      if ((snode->flag & SNODE_SKIP_INSOFFSET) == 0) {
        NodeInsertOfsData *iofsd = MEM_callocN(sizeof(NodeInsertOfsData), __func__);

        iofsd->insert = select;
        iofsd->prev = link->fromnode;
        iofsd->next = node;

        snode->runtime->iofsd = iofsd;
      }

      ntreeUpdateTree(bmain, snode->edittree); /* needed for pointers */
      snode_update(snode, select);
      ED_node_tag_update_id((ID *)snode->edittree);
      ED_node_tag_update_id(snode->id);

      sort_multi_input_socket_links(snode, node, NULL, NULL);
    }
  }
}
