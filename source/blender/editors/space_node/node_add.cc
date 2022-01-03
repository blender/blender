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

#include "DNA_collection_types.h"
#include "DNA_node_types.h"
#include "DNA_texture_types.h"

#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_tree_update.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_texture.h"

#include "DEG_depsgraph_build.h"

#include "ED_node.h" /* own include */
#include "ED_render.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "node_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

bNode *node_add_node(const bContext &C, const char *idname, int type, float locx, float locy)
{
  SpaceNode &snode = *CTX_wm_space_node(&C);
  Main &bmain = *CTX_data_main(&C);
  bNode *node = nullptr;

  node_deselect_all(snode);

  if (idname) {
    node = nodeAddNode(&C, snode.edittree, idname);
  }
  else {
    node = nodeAddStaticNode(&C, snode.edittree, type);
  }
  BLI_assert(node && node->typeinfo);

  /* Position mouse in node header. */
  node->locx = locx - NODE_DY * 1.5f / UI_DPI_FAC;
  node->locy = locy + NODE_DY * 0.5f / UI_DPI_FAC;

  nodeSetSelected(node, true);

  ED_node_set_active(&bmain, &snode, snode.edittree, node, nullptr);
  ED_node_tree_propagate_change(&C, &bmain, snode.edittree);
  return node;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Reroute Operator
 * \{ */

static bool add_reroute_intersect_check(const bNodeLink &link,
                                        float mcoords[][2],
                                        int tot,
                                        float result[2])
{
  float coord_array[NODE_LINK_RESOL + 1][2];

  if (node_link_bezier_points(nullptr, nullptr, link, coord_array, NODE_LINK_RESOL)) {
    for (int i = 0; i < tot - 1; i++) {
      for (int b = 0; b < NODE_LINK_RESOL; b++) {
        if (isect_seg_seg_v2_point(
                mcoords[i], mcoords[i + 1], coord_array[b], coord_array[b + 1], result) > 0) {
          return true;
        }
      }
    }
  }
  return false;
}

struct bNodeSocketLink {
  struct bNodeSocketLink *next, *prev;

  struct bNodeSocket *sock;
  struct bNodeLink *link;
  float point[2];
};

static bNodeSocketLink *add_reroute_insert_socket_link(ListBase *lb,
                                                       bNodeSocket *sock,
                                                       bNodeLink *link,
                                                       const float point[2])
{
  bNodeSocketLink *socklink, *prev;

  socklink = MEM_cnew<bNodeSocketLink>("socket link");
  socklink->sock = sock;
  socklink->link = link;
  copy_v2_v2(socklink->point, point);

  for (prev = (bNodeSocketLink *)lb->last; prev; prev = prev->prev) {
    if (prev->sock == sock) {
      break;
    }
  }
  BLI_insertlinkafter(lb, prev, socklink);
  return socklink;
}

static bNodeSocketLink *add_reroute_do_socket_section(bContext *C,
                                                      bNodeSocketLink *socklink,
                                                      int in_out)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  bNode *reroute_node = nullptr;
  bNodeSocket *cursock = socklink->sock;
  float insert_point[2];
  int num_links;

  zero_v2(insert_point);
  num_links = 0;

  while (socklink && socklink->sock == cursock) {
    if (!(socklink->link->flag & NODE_LINK_TEST)) {
      socklink->link->flag |= NODE_LINK_TEST;

      /* create the reroute node for this cursock */
      if (!reroute_node) {
        reroute_node = nodeAddStaticNode(C, ntree, NODE_REROUTE);

        /* add a single link to/from the reroute node to replace multiple links */
        if (in_out == SOCK_OUT) {
          nodeAddLink(ntree,
                      socklink->link->fromnode,
                      socklink->link->fromsock,
                      reroute_node,
                      (bNodeSocket *)reroute_node->inputs.first);
        }
        else {
          nodeAddLink(ntree,
                      reroute_node,
                      (bNodeSocket *)reroute_node->outputs.first,
                      socklink->link->tonode,
                      socklink->link->tosock);
        }
      }

      /* insert the reroute node into the link */
      if (in_out == SOCK_OUT) {
        socklink->link->fromnode = reroute_node;
        socklink->link->fromsock = (bNodeSocket *)reroute_node->outputs.first;
      }
      else {
        socklink->link->tonode = reroute_node;
        socklink->link->tosock = (bNodeSocket *)reroute_node->inputs.first;
      }

      add_v2_v2(insert_point, socklink->point);
      num_links++;
    }
    socklink = socklink->next;
  }

  if (num_links > 0) {
    /* average cut point from shared links */
    mul_v2_fl(insert_point, 1.0f / num_links);

    reroute_node->locx = insert_point[0] / UI_DPI_FAC;
    reroute_node->locy = insert_point[1] / UI_DPI_FAC;
  }

  return socklink;
}

static int add_reroute_exec(bContext *C, wmOperator *op)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  ARegion &region = *CTX_wm_region(C);
  bNodeTree &ntree = *snode.edittree;
  float mcoords[256][2];
  int i = 0;

  /* Get the cut path */
  RNA_BEGIN (op->ptr, itemptr, "path") {
    float loc[2];

    RNA_float_get_array(&itemptr, "loc", loc);
    UI_view2d_region_to_view(
        &region.v2d, (short)loc[0], (short)loc[1], &mcoords[i][0], &mcoords[i][1]);
    i++;
    if (i >= 256) {
      break;
    }
  }
  RNA_END;

  if (i > 1) {
    ListBase output_links, input_links;
    bNodeSocketLink *socklink;
    float insert_point[2];

    /* always first */
    ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

    node_deselect_all(snode);

    /* Find cut links and sort them by sockets */
    BLI_listbase_clear(&output_links);
    BLI_listbase_clear(&input_links);

    LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
      if (node_link_is_hidden_or_dimmed(region.v2d, *link)) {
        continue;
      }
      if (add_reroute_intersect_check(*link, mcoords, i, insert_point)) {
        add_reroute_insert_socket_link(&output_links, link->fromsock, link, insert_point);
        add_reroute_insert_socket_link(&input_links, link->tosock, link, insert_point);

        /* Clear flag */
        link->flag &= ~NODE_LINK_TEST;
      }
    }

    /* Create reroute nodes for intersected links.
     * Only one reroute if links share the same input/output socket.
     */
    socklink = (bNodeSocketLink *)output_links.first;
    while (socklink) {
      socklink = add_reroute_do_socket_section(C, socklink, SOCK_OUT);
    }
    socklink = (bNodeSocketLink *)input_links.first;
    while (socklink) {
      socklink = add_reroute_do_socket_section(C, socklink, SOCK_IN);
    }

    BLI_freelistN(&output_links);
    BLI_freelistN(&input_links);

    /* always last */
    ED_node_tree_propagate_change(C, CTX_data_main(C), &ntree);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

void NODE_OT_add_reroute(wmOperatorType *ot)
{
  ot->name = "Add Reroute";
  ot->idname = "NODE_OT_add_reroute";
  ot->description = "Add a reroute node";

  ot->invoke = WM_gesture_lines_invoke;
  ot->modal = WM_gesture_lines_modal;
  ot->exec = add_reroute_exec;
  ot->cancel = WM_gesture_lines_cancel;

  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
  RNA_def_property_flag(prop, (PropertyFlag)(PROP_HIDDEN | PROP_SKIP_SAVE));
  /* internal */
  RNA_def_int(ot->srna, "cursor", WM_CURSOR_CROSS, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Node Group Operator
 * \{ */

static bNodeTree *node_add_group_get_and_poll_group_node_tree(Main *bmain,
                                                              wmOperator *op,
                                                              bNodeTree *ntree)
{
  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "name", name);

  bNodeTree *node_group = (bNodeTree *)BKE_libblock_find_name(bmain, ID_NT, name);
  if (!node_group) {
    return nullptr;
  }

  const char *disabled_hint = nullptr;
  if ((node_group->type != ntree->type) || !nodeGroupPoll(ntree, node_group, &disabled_hint)) {
    if (disabled_hint) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Can not add node group '%s' to '%s':\n  %s",
                  node_group->id.name + 2,
                  ntree->id.name + 2,
                  disabled_hint);
    }
    else {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Can not add node group '%s' to '%s'",
                  node_group->id.name + 2,
                  ntree->id.name + 2);
    }

    return nullptr;
  }

  return node_group;
}

static int node_add_group_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  bNodeTree *node_group;

  if (!(node_group = node_add_group_get_and_poll_group_node_tree(bmain, op, ntree))) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *group_node = node_add_node(*C,
                                    node_group_idname(C),
                                    (node_group->type == NTREE_CUSTOM) ? NODE_CUSTOM_GROUP :
                                                                         NODE_GROUP,
                                    snode->runtime->cursor[0],
                                    snode->runtime->cursor[1]);
  if (!group_node) {
    BKE_report(op->reports, RPT_WARNING, "Could not add node group");
    return OPERATOR_CANCELLED;
  }

  group_node->id = &node_group->id;
  id_us_plus(group_node->id);
  BKE_ntree_update_tag_node_property(snode->edittree, group_node);

  nodeSetActive(ntree, group_node);
  ED_node_tree_propagate_change(C, bmain, nullptr);
  return OPERATOR_FINISHED;
}

static int node_add_group_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Convert mouse coordinates to v2d space. */
  UI_view2d_region_to_view(&region->v2d,
                           event->mval[0],
                           event->mval[1],
                           &snode->runtime->cursor[0],
                           &snode->runtime->cursor[1]);

  snode->runtime->cursor[0] /= UI_DPI_FAC;
  snode->runtime->cursor[1] /= UI_DPI_FAC;

  return node_add_group_exec(C, op);
}

void NODE_OT_add_group(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Node Group";
  ot->description = "Add an existing node group to the current node editor";
  ot->idname = "NODE_OT_add_group";

  /* callbacks */
  ot->exec = node_add_group_exec;
  ot->invoke = node_add_group_invoke;
  ot->poll = ED_operator_node_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_string(ot->srna, "name", "Mask", MAX_ID_NAME - 2, "Name", "Data-block name to assign");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Node Object Operator
 * \{ */

static Object *node_add_object_get_and_poll_object_node_tree(Main *bmain, wmOperator *op)
{
  if (RNA_struct_property_is_set(op->ptr, "session_uuid")) {
    const uint32_t session_uuid = (uint32_t)RNA_int_get(op->ptr, "session_uuid");
    return (Object *)BKE_libblock_find_session_uuid(bmain, ID_OB, session_uuid);
  }

  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "name", name);
  return (Object *)BKE_libblock_find_name(bmain, ID_OB, name);
}

static int node_add_object_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  Object *object;

  if (!(object = node_add_object_get_and_poll_object_node_tree(bmain, op))) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *object_node = node_add_node(
      *C, nullptr, GEO_NODE_OBJECT_INFO, snode->runtime->cursor[0], snode->runtime->cursor[1]);
  if (!object_node) {
    BKE_report(op->reports, RPT_WARNING, "Could not add node object");
    return OPERATOR_CANCELLED;
  }

  bNodeSocket *sock = nodeFindSocket(object_node, SOCK_IN, "Object");
  if (!sock) {
    BKE_report(op->reports, RPT_WARNING, "Could not find node object socket");
    return OPERATOR_CANCELLED;
  }

  bNodeSocketValueObject *socket_data = (bNodeSocketValueObject *)sock->default_value;
  socket_data->value = object;
  id_us_plus(&object->id);

  nodeSetActive(ntree, object_node);
  ED_node_tree_propagate_change(C, bmain, ntree);
  DEG_relations_tag_update(bmain);

  return OPERATOR_FINISHED;
}

static int node_add_object_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Convert mouse coordinates to v2d space. */
  UI_view2d_region_to_view(&region->v2d,
                           event->mval[0],
                           event->mval[1],
                           &snode->runtime->cursor[0],
                           &snode->runtime->cursor[1]);

  snode->runtime->cursor[0] /= UI_DPI_FAC;
  snode->runtime->cursor[1] /= UI_DPI_FAC;

  return node_add_object_exec(C, op);
}

static bool node_add_object_poll(bContext *C)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  return ED_operator_node_editable(C) && ELEM(snode->nodetree->type, NTREE_GEOMETRY) &&
         !UI_but_active_drop_name(C);
}

void NODE_OT_add_object(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Node Object";
  ot->description = "Add an object info node to the current node editor";
  ot->idname = "NODE_OT_add_object";

  /* callbacks */
  ot->exec = node_add_object_exec;
  ot->invoke = node_add_object_invoke;
  ot->poll = node_add_object_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_string(ot->srna, "name", "Object", MAX_ID_NAME - 2, "Name", "Data-block name to assign");
  RNA_def_int(ot->srna,
              "session_uuid",
              0,
              INT32_MIN,
              INT32_MAX,
              "Session UUID",
              "Session UUID of the data-block to assign",
              INT32_MIN,
              INT32_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Node Texture Operator
 * \{ */

static Tex *node_add_texture_get_and_poll_texture_node_tree(Main *bmain, wmOperator *op)
{
  if (RNA_struct_property_is_set(op->ptr, "session_uuid")) {
    const uint32_t session_uuid = (uint32_t)RNA_int_get(op->ptr, "session_uuid");
    return (Tex *)BKE_libblock_find_session_uuid(bmain, ID_TE, session_uuid);
  }

  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "name", name);
  return (Tex *)BKE_libblock_find_name(bmain, ID_TE, name);
}

static int node_add_texture_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  Tex *texture;

  if (!(texture = node_add_texture_get_and_poll_texture_node_tree(bmain, op))) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *texture_node = node_add_node(*C,
                                      nullptr,
                                      GEO_NODE_LEGACY_ATTRIBUTE_SAMPLE_TEXTURE,
                                      snode->runtime->cursor[0],
                                      snode->runtime->cursor[1]);
  if (!texture_node) {
    BKE_report(op->reports, RPT_WARNING, "Could not add texture node");
    return OPERATOR_CANCELLED;
  }

  texture_node->id = &texture->id;
  id_us_plus(&texture->id);

  nodeSetActive(ntree, texture_node);
  ED_node_tree_propagate_change(C, bmain, ntree);
  DEG_relations_tag_update(bmain);

  return OPERATOR_FINISHED;
}

static int node_add_texture_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Convert mouse coordinates to v2d space. */
  UI_view2d_region_to_view(&region->v2d,
                           event->mval[0],
                           event->mval[1],
                           &snode->runtime->cursor[0],
                           &snode->runtime->cursor[1]);

  snode->runtime->cursor[0] /= UI_DPI_FAC;
  snode->runtime->cursor[1] /= UI_DPI_FAC;

  return node_add_texture_exec(C, op);
}

static bool node_add_texture_poll(bContext *C)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  return ED_operator_node_editable(C) && ELEM(snode->nodetree->type, NTREE_GEOMETRY) &&
         !UI_but_active_drop_name(C);
}

void NODE_OT_add_texture(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Node Texture";
  ot->description = "Add a texture to the current node editor";
  ot->idname = "NODE_OT_add_texture";

  /* callbacks */
  ot->exec = node_add_texture_exec;
  ot->invoke = node_add_texture_invoke;
  ot->poll = node_add_texture_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_string(
      ot->srna, "name", "Texture", MAX_ID_NAME - 2, "Name", "Data-block name to assign");
  RNA_def_int(ot->srna,
              "session_uuid",
              0,
              INT32_MIN,
              INT32_MAX,
              "Session UUID",
              "Session UUID of the data-block to assign",
              INT32_MIN,
              INT32_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Node Collection Operator
 * \{ */

static Collection *node_add_collection_get_and_poll_collection_node_tree(Main *bmain,
                                                                         wmOperator *op)
{
  if (RNA_struct_property_is_set(op->ptr, "session_uuid")) {
    const uint32_t session_uuid = (uint32_t)RNA_int_get(op->ptr, "session_uuid");
    return (Collection *)BKE_libblock_find_session_uuid(bmain, ID_GR, session_uuid);
  }

  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "name", name);
  return (Collection *)BKE_libblock_find_name(bmain, ID_GR, name);
}

static int node_add_collection_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree *ntree = snode.edittree;
  Collection *collection;

  if (!(collection = node_add_collection_get_and_poll_collection_node_tree(bmain, op))) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *collection_node = node_add_node(
      *C, nullptr, GEO_NODE_COLLECTION_INFO, snode.runtime->cursor[0], snode.runtime->cursor[1]);
  if (!collection_node) {
    BKE_report(op->reports, RPT_WARNING, "Could not add node collection");
    return OPERATOR_CANCELLED;
  }

  bNodeSocket *sock = nodeFindSocket(collection_node, SOCK_IN, "Collection");
  if (!sock) {
    BKE_report(op->reports, RPT_WARNING, "Could not find node collection socket");
    return OPERATOR_CANCELLED;
  }

  bNodeSocketValueCollection *socket_data = (bNodeSocketValueCollection *)sock->default_value;
  socket_data->value = collection;
  id_us_plus(&collection->id);

  nodeSetActive(ntree, collection_node);
  ED_node_tree_propagate_change(C, bmain, ntree);
  DEG_relations_tag_update(bmain);

  return OPERATOR_FINISHED;
}

static int node_add_collection_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  /* Convert mouse coordinates to v2d space. */
  UI_view2d_region_to_view(&region->v2d,
                           event->mval[0],
                           event->mval[1],
                           &snode->runtime->cursor[0],
                           &snode->runtime->cursor[1]);

  snode->runtime->cursor[0] /= UI_DPI_FAC;
  snode->runtime->cursor[1] /= UI_DPI_FAC;

  return node_add_collection_exec(C, op);
}

static bool node_add_collection_poll(bContext *C)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  return ED_operator_node_editable(C) && ELEM(snode->nodetree->type, NTREE_GEOMETRY) &&
         !UI_but_active_drop_name(C);
}

void NODE_OT_add_collection(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Node Collection";
  ot->description = "Add an collection info node to the current node editor";
  ot->idname = "NODE_OT_add_collection";

  /* callbacks */
  ot->exec = node_add_collection_exec;
  ot->invoke = node_add_collection_invoke;
  ot->poll = node_add_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_string(
      ot->srna, "name", "Collection", MAX_ID_NAME - 2, "Name", "Data-block name to assign");
  RNA_def_int(ot->srna,
              "session_uuid",
              0,
              INT32_MIN,
              INT32_MAX,
              "Session UUID",
              "Session UUID of the data-block to assign",
              INT32_MIN,
              INT32_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add File Node Operator
 * \{ */

static bool node_add_file_poll(bContext *C)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  return ED_operator_node_editable(C) &&
         ELEM(snode->nodetree->type, NTREE_SHADER, NTREE_TEXTURE, NTREE_COMPOSIT, NTREE_GEOMETRY);
}

static int node_add_file_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNode *node;
  Image *ima;
  int type = 0;

  ima = (Image *)WM_operator_drop_load_path(C, op, ID_IM);
  if (!ima) {
    return OPERATOR_CANCELLED;
  }

  switch (snode.nodetree->type) {
    case NTREE_SHADER:
      type = SH_NODE_TEX_IMAGE;
      break;
    case NTREE_TEXTURE:
      type = TEX_NODE_IMAGE;
      break;
    case NTREE_COMPOSIT:
      type = CMP_NODE_IMAGE;
      break;
    case NTREE_GEOMETRY:
      type = GEO_NODE_IMAGE_TEXTURE;
      break;
    default:
      return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  node = node_add_node(*C, nullptr, type, snode.runtime->cursor[0], snode.runtime->cursor[1]);

  if (!node) {
    BKE_report(op->reports, RPT_WARNING, "Could not add an image node");
    return OPERATOR_CANCELLED;
  }

  if (type == GEO_NODE_IMAGE_TEXTURE) {
    bNodeSocket *image_socket = (bNodeSocket *)node->inputs.first;
    bNodeSocketValueImage *socket_value = (bNodeSocketValueImage *)image_socket->default_value;
    socket_value->value = ima;
  }
  else {
    node->id = (ID *)ima;
  }

  /* When adding new image file via drag-drop we need to load imbuf in order
   * to get proper image source.
   */
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    BKE_image_signal(bmain, ima, nullptr, IMA_SIGNAL_RELOAD);
    WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);
  }

  ED_node_tree_propagate_change(C, bmain, snode.edittree);
  DEG_relations_tag_update(bmain);

  return OPERATOR_FINISHED;
}

static int node_add_file_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  /* convert mouse coordinates to v2d space */
  UI_view2d_region_to_view(&region->v2d,
                           event->mval[0],
                           event->mval[1],
                           &snode->runtime->cursor[0],
                           &snode->runtime->cursor[1]);

  snode->runtime->cursor[0] /= UI_DPI_FAC;
  snode->runtime->cursor[1] /= UI_DPI_FAC;

  if (RNA_struct_property_is_set(op->ptr, "filepath") ||
      RNA_struct_property_is_set(op->ptr, "name")) {
    return node_add_file_exec(C, op);
  }
  return WM_operator_filesel(C, op, event);
}

void NODE_OT_add_file(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add File Node";
  ot->description = "Add a file node to the current node editor";
  ot->idname = "NODE_OT_add_file";

  /* callbacks */
  ot->exec = node_add_file_exec;
  ot->invoke = node_add_file_invoke;
  ot->poll = node_add_file_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  RNA_def_string(ot->srna, "name", "Image", MAX_ID_NAME - 2, "Name", "Data-block name to assign");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Mask Node Operator
 * \{ */

static ID *node_add_mask_get_and_poll_mask(Main *bmain, wmOperator *op)
{
  if (RNA_struct_property_is_set(op->ptr, "session_uuid")) {
    const uint32_t session_uuid = (uint32_t)RNA_int_get(op->ptr, "session_uuid");
    return BKE_libblock_find_session_uuid(bmain, ID_MSK, session_uuid);
  }

  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "name", name);
  return BKE_libblock_find_name(bmain, ID_MSK, name);
}

static bool node_add_mask_poll(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  return ED_operator_node_editable(C) && snode->nodetree->type == NTREE_COMPOSIT;
}

static int node_add_mask_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNode *node;

  ID *mask = node_add_mask_get_and_poll_mask(bmain, op);
  if (!mask) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  node = node_add_node(
      *C, nullptr, CMP_NODE_MASK, snode.runtime->cursor[0], snode.runtime->cursor[1]);

  if (!node) {
    BKE_report(op->reports, RPT_WARNING, "Could not add a mask node");
    return OPERATOR_CANCELLED;
  }

  node->id = mask;
  id_us_plus(mask);

  ED_node_tree_propagate_change(C, bmain, snode.edittree);
  DEG_relations_tag_update(bmain);

  return OPERATOR_FINISHED;
}

void NODE_OT_add_mask(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Mask Node";
  ot->description = "Add a mask node to the current node editor";
  ot->idname = "NODE_OT_add_mask";

  /* callbacks */
  ot->exec = node_add_mask_exec;
  ot->poll = node_add_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_string(ot->srna, "name", "Mask", MAX_ID_NAME - 2, "Name", "Data-block name to assign");
  RNA_def_int(ot->srna,
              "session_uuid",
              0,
              INT32_MIN,
              INT32_MAX,
              "Session UUID",
              "Session UUID of the data-block to assign",
              INT32_MIN,
              INT32_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Node Tree Operator
 * \{ */

static int new_node_tree_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  Main *bmain = CTX_data_main(C);
  bNodeTree *ntree;
  PointerRNA ptr, idptr;
  PropertyRNA *prop;
  const char *idname;
  char treename_buf[MAX_ID_NAME - 2];
  const char *treename;

  if (RNA_struct_property_is_set(op->ptr, "type")) {
    prop = RNA_struct_find_property(op->ptr, "type");
    RNA_property_enum_identifier(C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &idname);
  }
  else if (snode) {
    idname = snode->tree_idname;
  }
  else {
    return OPERATOR_CANCELLED;
  }

  if (RNA_struct_property_is_set(op->ptr, "name")) {
    RNA_string_get(op->ptr, "name", treename_buf);
    treename = treename_buf;
  }
  else {
    treename = DATA_("NodeTree");
  }

  if (!ntreeTypeFind(idname)) {
    BKE_reportf(op->reports, RPT_ERROR, "Node tree type %s undefined", idname);
    return OPERATOR_CANCELLED;
  }

  ntree = ntreeAddTree(bmain, treename, idname);

  /* hook into UI */
  UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

  if (prop) {
    /* RNA_property_pointer_set increases the user count,
     * fixed here as the editor is the initial user.
     */
    id_us_min(&ntree->id);

    RNA_id_pointer_create(&ntree->id, &idptr);
    RNA_property_pointer_set(&ptr, prop, idptr, nullptr);
    RNA_property_update(C, &ptr, prop);
  }
  else if (snode) {
    snode->nodetree = ntree;

    ED_node_tree_update(C);
  }

  return OPERATOR_FINISHED;
}

static const EnumPropertyItem *new_node_tree_type_itemf(bContext *UNUSED(C),
                                                        PointerRNA *UNUSED(ptr),
                                                        PropertyRNA *UNUSED(prop),
                                                        bool *r_free)
{
  return rna_node_tree_type_itemf(nullptr, nullptr, r_free);
}

void NODE_OT_new_node_tree(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "New Node Tree";
  ot->idname = "NODE_OT_new_node_tree";
  ot->description = "Create a new node tree";

  /* api callbacks */
  ot->exec = new_node_tree_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_enum(ot->srna, "type", DummyRNA_NULL_items, 0, "Tree Type", "");
  RNA_def_enum_funcs(prop, new_node_tree_type_itemf);
  RNA_def_string(ot->srna, "name", "NodeTree", MAX_ID_NAME - 2, "Name", "");
}

/** \} */
