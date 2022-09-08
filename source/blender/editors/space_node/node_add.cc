/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup spnode
 */

#include <numeric>

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
#include "BKE_node_runtime.hh"
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
#include "RNA_prototypes.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "node_intern.hh" /* own include */

namespace blender::ed::space_node {

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

static void position_node_based_on_mouse(bNode &node, const float2 &location)
{
  node.locx = location.x - NODE_DY * 1.5f / UI_DPI_FAC;
  node.locy = location.y + NODE_DY * 0.5f / UI_DPI_FAC;
}

bNode *add_node(const bContext &C, const StringRef idname, const float2 &location)
{
  SpaceNode &snode = *CTX_wm_space_node(&C);
  Main &bmain = *CTX_data_main(&C);

  node_deselect_all(snode);

  const std::string idname_str = idname;

  bNode *node = nodeAddNode(&C, snode.edittree, idname_str.c_str());
  BLI_assert(node && node->typeinfo);

  position_node_based_on_mouse(*node, location);

  nodeSetSelected(node, true);
  ED_node_set_active(&bmain, &snode, snode.edittree, node, nullptr);

  ED_node_tree_propagate_change(&C, &bmain, snode.edittree);
  return node;
}

bNode *add_static_node(const bContext &C, int type, const float2 &location)
{
  SpaceNode &snode = *CTX_wm_space_node(&C);
  Main &bmain = *CTX_data_main(&C);

  node_deselect_all(snode);

  bNode *node = nodeAddStaticNode(&C, snode.edittree, type);
  BLI_assert(node && node->typeinfo);

  position_node_based_on_mouse(*node, location);

  nodeSetSelected(node, true);
  ED_node_set_active(&bmain, &snode, snode.edittree, node, nullptr);

  ED_node_tree_propagate_change(&C, &bmain, snode.edittree);
  return node;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Reroute Operator
 * \{ */

std::optional<float2> link_path_intersection(const bNodeLink &link, const Span<float2> path)
{
  std::array<float2, NODE_LINK_RESOL + 1> coords;
  node_link_bezier_points_evaluated(link, coords);

  for (const int i : path.index_range().drop_back(1)) {
    for (const int j : IndexRange(NODE_LINK_RESOL)) {
      float2 result;
      if (isect_seg_seg_v2_point(path[i], path[i + 1], coords[j], coords[j + 1], result) > 0) {
        return result;
      }
    }
  }

  return std::nullopt;
}

struct RerouteCutsForSocket {
  /* The output socket's owner node. */
  bNode *from_node;
  /* Intersected links connected to the socket and their path intersection locations. */
  Map<bNodeLink *, float2> links;
};

static int add_reroute_exec(bContext *C, wmOperator *op)
{
  const ARegion &region = *CTX_wm_region(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;

  Vector<float2> path;
  RNA_BEGIN (op->ptr, itemptr, "path") {
    float2 loc_region;
    RNA_float_get_array(&itemptr, "loc", loc_region);
    float2 loc_view;
    UI_view2d_region_to_view(&region.v2d, loc_region.x, loc_region.y, &loc_view.x, &loc_view.y);
    path.append(loc_view);
    if (path.size() >= 256) {
      break;
    }
  }
  RNA_END;

  if (path.is_empty()) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  ntree.ensure_topology_cache();
  const Vector<bNode *> frame_nodes = ntree.nodes_by_type("NodeFrame");

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));
  node_deselect_all(snode);

  /* All link "cuts" that start at a particular output socket. Deduplicating new reroutes per
   * output socket is useful because it allows reusing reroutes for connected intersections.
   * Further deduplication using the second map means we only have one cut per link. */
  Map<bNodeSocket *, RerouteCutsForSocket> cuts_per_socket;

  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    if (node_link_is_hidden_or_dimmed(region.v2d, *link)) {
      continue;
    }
    const std::optional<float2> intersection = link_path_intersection(*link, path);
    if (!intersection) {
      continue;
    }
    RerouteCutsForSocket &from_cuts = cuts_per_socket.lookup_or_add_default(link->fromsock);
    from_cuts.from_node = link->fromnode;
    from_cuts.links.add(link, *intersection);
  }

  for (const auto item : cuts_per_socket.items()) {
    const Map<bNodeLink *, float2> &cuts = item.value.links;

    bNode *reroute = nodeAddStaticNode(C, &ntree, NODE_REROUTE);

    nodeAddLink(&ntree,
                item.value.from_node,
                item.key,
                reroute,
                static_cast<bNodeSocket *>(reroute->inputs.first));

    /* Reconnect links from the original output socket to the new reroute. */
    for (bNodeLink *link : cuts.keys()) {
      link->fromnode = reroute;
      link->fromsock = static_cast<bNodeSocket *>(reroute->outputs.first);
      BKE_ntree_update_tag_link_changed(&ntree);
    }

    /* Place the new reroute at the average location of all connected cuts. */
    const float2 loc = std::accumulate(cuts.values().begin(), cuts.values().end(), float2(0)) /
                       cuts.size() / UI_DPI_FAC;
    reroute->locx = loc.x;
    reroute->locy = loc.y;

    /* Attach the reroute node to frame nodes behind it. */
    for (const int i : frame_nodes.index_range()) {
      bNode *frame_node = frame_nodes.last(i);
      if (BLI_rctf_isect_pt_v(&frame_node->totr, loc)) {
        nodeAttachNode(reroute, frame_node);
        break;
      }
    }
  }

  ED_node_tree_propagate_change(C, CTX_data_main(C), &ntree);
  return OPERATOR_FINISHED;
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
  bNodeTree *node_group = reinterpret_cast<bNodeTree *>(
      WM_operator_properties_id_lookup_from_name_or_session_uuid(bmain, op->ptr, ID_NT));
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

  bNodeTree *node_group = node_add_group_get_and_poll_group_node_tree(bmain, op, ntree);
  if (!node_group) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  const char *node_idname = node_group_idname(C);
  if (node_idname[0] == '\0') {
    BKE_report(op->reports, RPT_WARNING, "Could not determine type of group node");
    return OPERATOR_CANCELLED;
  }

  bNode *group_node = add_node(*C, node_idname, snode->runtime->cursor);
  if (!group_node) {
    BKE_report(op->reports, RPT_WARNING, "Could not add node group");
    return OPERATOR_CANCELLED;
  }

  group_node->id = &node_group->id;
  id_us_plus(group_node->id);
  BKE_ntree_update_tag_node_property(snode->edittree, group_node);

  nodeSetActive(ntree, group_node);
  ED_node_tree_propagate_change(C, bmain, nullptr);
  DEG_relations_tag_update(bmain);
  return OPERATOR_FINISHED;
}

static bool node_add_group_poll(bContext *C)
{
  if (!ED_operator_node_editable(C)) {
    return false;
  }
  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode->edittree->type == NTREE_CUSTOM) {
    CTX_wm_operator_poll_msg_set(C,
                                 "This node editor displays a custom (Python defined) node tree. "
                                 "Dropping node groups isn't supported for this");
    return false;
  }
  return true;
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
  ot->poll = node_add_group_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  WM_operator_properties_id_lookup(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Node Object Operator
 * \{ */

static int node_add_object_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  Object *object = reinterpret_cast<Object *>(
      WM_operator_properties_id_lookup_from_name_or_session_uuid(bmain, op->ptr, ID_OB));

  if (!object) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *object_node = add_static_node(*C, GEO_NODE_OBJECT_INFO, snode->runtime->cursor);
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

  WM_operator_properties_id_lookup(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Node Collection Operator
 * \{ */

static int node_add_collection_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;

  Collection *collection = reinterpret_cast<Collection *>(
      WM_operator_properties_id_lookup_from_name_or_session_uuid(bmain, op->ptr, ID_GR));

  if (!collection) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *collection_node = add_static_node(*C, GEO_NODE_COLLECTION_INFO, snode.runtime->cursor);
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

  nodeSetActive(&ntree, collection_node);
  ED_node_tree_propagate_change(C, bmain, &ntree);
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

  WM_operator_properties_id_lookup(ot, true);
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
  int type = 0;

  Image *ima = (Image *)WM_operator_drop_load_path(C, op, ID_IM);
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

  bNode *node = add_static_node(*C, type, snode.runtime->cursor);

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

  if (WM_operator_properties_id_lookup_is_set(op->ptr) ||
      RNA_struct_property_is_set(op->ptr, "filepath")) {
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
  WM_operator_properties_id_lookup(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Mask Node Operator
 * \{ */

static bool node_add_mask_poll(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  return ED_operator_node_editable(C) && snode->nodetree->type == NTREE_COMPOSIT;
}

static int node_add_mask_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);

  ID *mask = WM_operator_properties_id_lookup_from_name_or_session_uuid(bmain, op->ptr, ID_MSK);
  if (!mask) {
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *node = add_static_node(*C, CMP_NODE_MASK, snode.runtime->cursor);

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

  WM_operator_properties_id_lookup(ot, true);
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

}  // namespace blender::ed::space_node
