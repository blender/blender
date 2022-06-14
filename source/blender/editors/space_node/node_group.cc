/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup spnode
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_node_types.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_vec_types.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node_tree_update.h"
#include "BKE_report.h"

#include "DEG_depsgraph_build.h"

#include "ED_node.h" /* own include */
#include "ED_render.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_prototypes.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"

#include "NOD_common.h"
#include "NOD_socket.h"
#include "node_intern.hh" /* own include */

namespace blender::ed::space_node {

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static bool node_group_operator_active_poll(bContext *C)
{
  if (ED_operator_node_active(C)) {
    SpaceNode *snode = CTX_wm_space_node(C);

    /* Group operators only defined for standard node tree types.
     * Disabled otherwise to allow python-nodes define their own operators
     * with same key-map. */
    if (STR_ELEM(snode->tree_idname,
                 "ShaderNodeTree",
                 "CompositorNodeTree",
                 "TextureNodeTree",
                 "GeometryNodeTree",
                 "ParticleNodeTree")) {
      return true;
    }
  }
  return false;
}

static bool node_group_operator_editable(bContext *C)
{
  if (ED_operator_node_editable(C)) {
    SpaceNode *snode = CTX_wm_space_node(C);

    /* Group operators only defined for standard node tree types.
     * Disabled otherwise to allow python-nodes define their own operators
     * with same key-map. */
    if (ED_node_is_shader(snode) || ED_node_is_compositor(snode) || ED_node_is_texture(snode) ||
        ED_node_is_geometry(snode)) {
      return true;
    }
  }
  return false;
}

static const char *group_ntree_idname(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  return snode->tree_idname;
}

const char *node_group_idname(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if (ED_node_is_shader(snode)) {
    return "ShaderNodeGroup";
  }
  if (ED_node_is_compositor(snode)) {
    return "CompositorNodeGroup";
  }
  if (ED_node_is_texture(snode)) {
    return "TextureNodeGroup";
  }
  if (ED_node_is_geometry(snode)) {
    return "GeometryNodeGroup";
  }

  return "";
}

static bNode *node_group_get_active(bContext *C, const char *node_idname)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNode *node = nodeGetActive(snode->edittree);

  if (node && STREQ(node->idname, node_idname)) {
    return node;
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Group Operator
 * \{ */

static int node_group_edit_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  const char *node_idname = node_group_idname(C);
  const bool exit = RNA_boolean_get(op->ptr, "exit");

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *gnode = node_group_get_active(C, node_idname);

  if (gnode && !exit) {
    bNodeTree *ngroup = (bNodeTree *)gnode->id;

    if (ngroup) {
      ED_node_tree_push(snode, ngroup, gnode);
    }
  }
  else {
    ED_node_tree_pop(snode);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_NODES, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_group_edit(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Group";
  ot->description = "Edit node group";
  ot->idname = "NODE_OT_group_edit";

  /* api callbacks */
  ot->exec = node_group_edit_exec;
  ot->poll = node_group_operator_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "exit", false, "Exit", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ungroup Operator
 * \{ */

/**
 * The given paths will be owned by the returned instance.
 * Both pointers are allowed to point to the same string.
 */
static AnimationBasePathChange *animation_basepath_change_new(const char *src_basepath,
                                                              const char *dst_basepath)
{
  AnimationBasePathChange *basepath_change = (AnimationBasePathChange *)MEM_callocN(
      sizeof(*basepath_change), AT);
  basepath_change->src_basepath = src_basepath;
  basepath_change->dst_basepath = dst_basepath;
  return basepath_change;
}

static void animation_basepath_change_free(AnimationBasePathChange *basepath_change)
{
  if (basepath_change->src_basepath != basepath_change->dst_basepath) {
    MEM_freeN((void *)basepath_change->src_basepath);
  }
  MEM_freeN((void *)basepath_change->dst_basepath);
  MEM_freeN(basepath_change);
}

/**
 * \return True if successful.
 */
static bool node_group_ungroup(Main *bmain, bNodeTree *ntree, bNode *gnode)
{
  ListBase anim_basepaths = {nullptr, nullptr};
  Vector<bNode *> nodes_delayed_free;
  const bNodeTree *ngroup = reinterpret_cast<const bNodeTree *>(gnode->id);

  /* `wgroup` is a temporary copy of the #NodeTree we're merging in
   * - All of wgroup's nodes are copied across to their new home.
   * - `ngroup` (i.e. the source NodeTree) is left unscathed.
   * - Temp copy. do change ID user-count for the copies.
   */
  bNodeTree *wgroup = ntreeCopyTree(bmain, ngroup);

  /* Add the nodes into the `ntree`. */
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &wgroup->nodes) {
    /* Remove interface nodes.
     * This also removes remaining links to and from interface nodes.
     */
    if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
      /* We must delay removal since sockets will reference this node. see: T52092 */
      nodes_delayed_free.append(node);
    }

    /* Keep track of this node's RNA "base" path (the part of the path identifying the node)
     * if the old node-tree has animation data which potentially covers this node. */
    const char *old_animation_basepath = nullptr;
    if (wgroup->adt) {
      PointerRNA ptr;
      RNA_pointer_create(&wgroup->id, &RNA_Node, node, &ptr);
      old_animation_basepath = RNA_path_from_ID_to_struct(&ptr);
    }

    /* migrate node */
    BLI_remlink(&wgroup->nodes, node);
    BLI_addtail(&ntree->nodes, node);
    BKE_ntree_update_tag_node_new(ntree, node);

    /* ensure unique node name in the node tree */
    nodeUniqueName(ntree, node);

    if (wgroup->adt) {
      PointerRNA ptr;
      RNA_pointer_create(&ntree->id, &RNA_Node, node, &ptr);
      const char *new_animation_basepath = RNA_path_from_ID_to_struct(&ptr);
      BLI_addtail(&anim_basepaths,
                  animation_basepath_change_new(old_animation_basepath, new_animation_basepath));
    }

    if (!node->parent) {
      node->locx += gnode->locx;
      node->locy += gnode->locy;
    }

    node->flag |= NODE_SELECT;
  }

  bNodeLink *glinks_first = (bNodeLink *)ntree->links.last;

  /* Add internal links to the ntree */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &wgroup->links) {
    BLI_remlink(&wgroup->links, link);
    BLI_addtail(&ntree->links, link);
    BKE_ntree_update_tag_link_added(ntree, link);
  }

  bNodeLink *glinks_last = (bNodeLink *)ntree->links.last;

  /* and copy across the animation,
   * note that the animation data's action can be nullptr here */
  if (wgroup->adt) {
    bAction *waction;

    /* firstly, wgroup needs to temporary dummy action
     * that can be destroyed, as it shares copies */
    waction = wgroup->adt->action = (bAction *)BKE_id_copy(bmain, &wgroup->adt->action->id);

    /* now perform the moving */
    BKE_animdata_transfer_by_basepath(bmain, &wgroup->id, &ntree->id, &anim_basepaths);

    /* paths + their wrappers need to be freed */
    LISTBASE_FOREACH_MUTABLE (AnimationBasePathChange *, basepath_change, &anim_basepaths) {
      animation_basepath_change_free(basepath_change);
    }

    /* free temp action too */
    if (waction) {
      BKE_id_free(bmain, waction);
      wgroup->adt->action = nullptr;
    }
  }

  /* free the group tree (takes care of user count) */
  BKE_id_free(bmain, wgroup);

  /* restore external links to and from the gnode */

  /* input links */
  if (glinks_first != nullptr) {
    for (bNodeLink *link = glinks_first->next; link != glinks_last->next; link = link->next) {
      if (link->fromnode->type == NODE_GROUP_INPUT) {
        const char *identifier = link->fromsock->identifier;
        int num_external_links = 0;

        /* find external links to this input */
        for (bNodeLink *tlink = (bNodeLink *)ntree->links.first; tlink != glinks_first->next;
             tlink = tlink->next) {
          if (tlink->tonode == gnode && STREQ(tlink->tosock->identifier, identifier)) {
            nodeAddLink(ntree, tlink->fromnode, tlink->fromsock, link->tonode, link->tosock);
            num_external_links++;
          }
        }

        /* if group output is not externally linked,
         * convert the constant input value to ensure somewhat consistent behavior */
        if (num_external_links == 0) {
          /* TODO */
#if 0
          bNodeSocket *sock = node_group_find_input_socket(gnode, identifier);
          BLI_assert(sock);

          nodeSocketCopy(
              ntree, link->tosock->new_sock, link->tonode->new_node, ntree, sock, gnode);
#endif
        }
      }
    }

    /* Also iterate over new links to cover passthrough links. */
    glinks_last = (bNodeLink *)ntree->links.last;

    /* output links */
    for (bNodeLink *link = (bNodeLink *)ntree->links.first; link != glinks_first->next;
         link = link->next) {
      if (link->fromnode == gnode) {
        const char *identifier = link->fromsock->identifier;
        int num_internal_links = 0;

        /* find internal links to this output */
        for (bNodeLink *tlink = glinks_first->next; tlink != glinks_last->next;
             tlink = tlink->next) {
          /* only use active output node */
          if (tlink->tonode->type == NODE_GROUP_OUTPUT && (tlink->tonode->flag & NODE_DO_OUTPUT)) {
            if (STREQ(tlink->tosock->identifier, identifier)) {
              nodeAddLink(ntree, tlink->fromnode, tlink->fromsock, link->tonode, link->tosock);
              num_internal_links++;
            }
          }
        }

        /* if group output is not internally linked,
         * convert the constant output value to ensure somewhat consistent behavior */
        if (num_internal_links == 0) {
          /* TODO */
#if 0
          bNodeSocket *sock = node_group_find_output_socket(gnode, identifier);
          BLI_assert(sock);

          nodeSocketCopy(ntree, link->tosock, link->tonode, ntree, sock, gnode);
#endif
        }
      }
    }
  }

  for (bNode *node : nodes_delayed_free) {
    nodeRemoveNode(bmain, ntree, node, false);
  }

  /* delete the group instance and dereference group tree */
  nodeRemoveNode(bmain, ntree, gnode, true);

  return true;
}

static int node_group_ungroup_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  const char *node_idname = node_group_idname(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  bNode *gnode = node_group_get_active(C, node_idname);
  if (!gnode) {
    return OPERATOR_CANCELLED;
  }

  if (gnode->id && node_group_ungroup(bmain, snode->edittree, gnode)) {
    ED_node_tree_propagate_change(C, CTX_data_main(C), nullptr);
  }
  else {
    BKE_report(op->reports, RPT_WARNING, "Cannot ungroup");
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void NODE_OT_group_ungroup(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Ungroup";
  ot->description = "Ungroup selected nodes";
  ot->idname = "NODE_OT_group_ungroup";

  /* api callbacks */
  ot->exec = node_group_ungroup_exec;
  ot->poll = node_group_operator_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Separate Operator
 * \{ */

/**
 * \return True if successful.
 */
static bool node_group_separate_selected(
    Main &bmain, bNodeTree &ntree, bNodeTree &ngroup, const float2 &offset, const bool make_copy)
{
  /* deselect all nodes in the target tree */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    nodeSetSelected(node, false);
  }

  ListBase anim_basepaths = {nullptr, nullptr};

  Map<const bNode *, bNode *> node_map;
  Map<const bNodeSocket *, bNodeSocket *> socket_map;

  /* add selected nodes into the ntree */
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ngroup.nodes) {
    if (!(node->flag & NODE_SELECT)) {
      continue;
    }

    /* ignore interface nodes */
    if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
      nodeSetSelected(node, false);
      continue;
    }

    bNode *newnode;
    if (make_copy) {
      /* make a copy */
      newnode = blender::bke::node_copy_with_mapping(
          &ngroup, *node, LIB_ID_COPY_DEFAULT, true, socket_map);
      node_map.add_new(node, newnode);
    }
    else {
      /* use the existing node */
      newnode = node;
    }

    /* Keep track of this node's RNA "base" path (the part of the path identifying the node)
     * if the old node-tree has animation data which potentially covers this node. */
    if (ngroup.adt) {
      PointerRNA ptr;
      char *path;

      RNA_pointer_create(&ngroup.id, &RNA_Node, newnode, &ptr);
      path = RNA_path_from_ID_to_struct(&ptr);

      if (path) {
        BLI_addtail(&anim_basepaths, animation_basepath_change_new(path, path));
      }
    }

    /* ensure valid parent pointers, detach if parent stays inside the group */
    if (newnode->parent && !(newnode->parent->flag & NODE_SELECT)) {
      nodeDetachNode(newnode);
    }

    /* migrate node */
    BLI_remlink(&ngroup.nodes, newnode);
    BLI_addtail(&ntree.nodes, newnode);

    /* ensure unique node name in the node tree */
    nodeUniqueName(&ntree, newnode);

    if (!newnode->parent) {
      newnode->locx += offset.x;
      newnode->locy += offset.y;
    }
  }

  /* add internal links to the ntree */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ngroup.links) {
    const bool fromselect = (link->fromnode && (link->fromnode->flag & NODE_SELECT));
    const bool toselect = (link->tonode && (link->tonode->flag & NODE_SELECT));

    if (make_copy) {
      /* make a copy of internal links */
      if (fromselect && toselect) {
        nodeAddLink(&ntree,
                    node_map.lookup(link->fromnode),
                    socket_map.lookup(link->fromsock),
                    node_map.lookup(link->tonode),
                    socket_map.lookup(link->tosock));
      }
    }
    else {
      /* move valid links over, delete broken links */
      if (fromselect && toselect) {
        BLI_remlink(&ngroup.links, link);
        BLI_addtail(&ntree.links, link);
      }
      else if (fromselect || toselect) {
        nodeRemLink(&ngroup, link);
      }
    }
  }

  /* and copy across the animation,
   * note that the animation data's action can be nullptr here */
  if (ngroup.adt) {
    /* now perform the moving */
    BKE_animdata_transfer_by_basepath(&bmain, &ngroup.id, &ntree.id, &anim_basepaths);

    /* paths + their wrappers need to be freed */
    LISTBASE_FOREACH_MUTABLE (AnimationBasePathChange *, basepath_change, &anim_basepaths) {
      animation_basepath_change_free(basepath_change);
    }
  }

  BKE_ntree_update_tag_all(&ntree);
  if (!make_copy) {
    BKE_ntree_update_tag_all(&ngroup);
  }

  return true;
}

enum eNodeGroupSeparateType {
  NODE_GS_COPY,
  NODE_GS_MOVE,
};

/* Operator Property */
static const EnumPropertyItem node_group_separate_types[] = {
    {NODE_GS_COPY, "COPY", 0, "Copy", "Copy to parent node tree, keep group intact"},
    {NODE_GS_MOVE, "MOVE", 0, "Move", "Move to parent node tree, remove from group"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int node_group_separate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  int type = RNA_enum_get(op->ptr, "type");

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  /* are we inside of a group? */
  bNodeTree *ngroup = snode->edittree;
  bNodeTree *nparent = ED_node_tree_get(snode, 1);
  if (!nparent) {
    BKE_report(op->reports, RPT_WARNING, "Not inside node group");
    return OPERATOR_CANCELLED;
  }
  /* get node tree offset */
  const float2 offset = space_node_group_offset(*snode);

  switch (type) {
    case NODE_GS_COPY:
      if (!node_group_separate_selected(*bmain, *nparent, *ngroup, offset, true)) {
        BKE_report(op->reports, RPT_WARNING, "Cannot separate nodes");
        return OPERATOR_CANCELLED;
      }
      break;
    case NODE_GS_MOVE:
      if (!node_group_separate_selected(*bmain, *nparent, *ngroup, offset, false)) {
        BKE_report(op->reports, RPT_WARNING, "Cannot separate nodes");
        return OPERATOR_CANCELLED;
      }
      break;
  }

  /* switch to parent tree */
  ED_node_tree_pop(snode);

  ED_node_tree_propagate_change(C, CTX_data_main(C), nullptr);

  return OPERATOR_FINISHED;
}

static int node_group_separate_invoke(bContext *C,
                                      wmOperator *UNUSED(op),
                                      const wmEvent *UNUSED(event))
{
  uiPopupMenu *pup = UI_popup_menu_begin(
      C, CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Separate"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
  uiItemEnumO(layout, "NODE_OT_group_separate", nullptr, 0, "type", NODE_GS_COPY);
  uiItemEnumO(layout, "NODE_OT_group_separate", nullptr, 0, "type", NODE_GS_MOVE);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void NODE_OT_group_separate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Separate";
  ot->description = "Separate selected nodes from the node group";
  ot->idname = "NODE_OT_group_separate";

  /* api callbacks */
  ot->invoke = node_group_separate_invoke;
  ot->exec = node_group_separate_exec;
  ot->poll = node_group_operator_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "type", node_group_separate_types, NODE_GS_COPY, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Group Operator
 * \{ */

static bool node_group_make_use_node(bNode &node, bNode *gnode)
{
  return (&node != gnode && !ELEM(node.type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT) &&
          (node.flag & NODE_SELECT));
}

static bool node_group_make_test_selected(bNodeTree &ntree,
                                          bNode *gnode,
                                          const char *ntree_idname,
                                          struct ReportList &reports)
{
  int ok = true;

  /* make a local pseudo node tree to pass to the node poll functions */
  bNodeTree *ngroup = ntreeAddTree(nullptr, "Pseudo Node Group", ntree_idname);

  /* check poll functions for selected nodes */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node_group_make_use_node(*node, gnode)) {
      const char *disabled_hint = nullptr;
      if (node->typeinfo->poll_instance &&
          !node->typeinfo->poll_instance(node, ngroup, &disabled_hint)) {
        if (disabled_hint) {
          BKE_reportf(&reports,
                      RPT_WARNING,
                      "Can not add node '%s' in a group:\n  %s",
                      node->name,
                      disabled_hint);
        }
        else {
          BKE_reportf(&reports, RPT_WARNING, "Can not add node '%s' in a group", node->name);
        }
        ok = false;
        break;
      }
    }

    node->done = 0;
  }

  /* free local pseudo node tree again */
  ntreeFreeTree(ngroup);
  MEM_freeN(ngroup);
  if (!ok) {
    return false;
  }

  /* check if all connections are OK, no unselected node has both
   * inputs and outputs to a selection */
  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    if (node_group_make_use_node(*link->fromnode, gnode)) {
      link->tonode->done |= 1;
    }
    if (node_group_make_use_node(*link->tonode, gnode)) {
      link->fromnode->done |= 2;
    }
  }
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (!(node->flag & NODE_SELECT) && node != gnode && node->done == 3) {
      return false;
    }
  }
  return true;
}

static int node_get_selected_minmax(
    bNodeTree &ntree, bNode *gnode, float2 &min, float2 &max, bool use_size)
{
  int totselect = 0;

  INIT_MINMAX2(min, max);
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node_group_make_use_node(*node, gnode)) {
      float loc[2];
      nodeToView(node, node->offsetx, node->offsety, &loc[0], &loc[1]);
      minmax_v2v2_v2(min, max, loc);
      if (use_size) {
        loc[0] += node->width;
        loc[1] -= node->height;
        minmax_v2v2_v2(min, max, loc);
      }
      totselect++;
    }
  }

  /* sane min/max if no selected nodes */
  if (totselect == 0) {
    min[0] = min[1] = max[0] = max[1] = 0.0f;
  }

  return totselect;
}

static void node_group_make_insert_selected(const bContext &C, bNodeTree &ntree, bNode *gnode)
{
  Main *bmain = CTX_data_main(&C);
  bNodeTree *ngroup = (bNodeTree *)gnode->id;
  bool expose_visible = false;

  /* XXX rough guess, not nice but we don't have access to UI constants here ... */
  static const float offsetx = 200;
  static const float offsety = 0.0f;

  /* deselect all nodes in the target tree */
  LISTBASE_FOREACH (bNode *, node, &ngroup->nodes) {
    nodeSetSelected(node, false);
  }

  float2 center, min, max;
  const int totselect = node_get_selected_minmax(ntree, gnode, min, max, false);
  add_v2_v2v2(center, min, max);
  mul_v2_fl(center, 0.5f);

  float2 real_min, real_max;
  node_get_selected_minmax(ntree, gnode, real_min, real_max, true);

  /* auto-add interface for "solo" nodes */
  if (totselect == 1) {
    expose_visible = true;
  }

  ListBase anim_basepaths = {nullptr, nullptr};

  /* Detach unselected nodes inside frames when the frame is put into the group. Otherwise the
   * `parent` pointer becomes dangling. */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->parent == nullptr) {
      continue;
    }
    if (node_group_make_use_node(*node->parent, gnode) &&
        !node_group_make_use_node(*node, gnode)) {
      nodeDetachNode(node);
    }
  }

  /* move nodes over */
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree.nodes) {
    if (node_group_make_use_node(*node, gnode)) {
      /* Keep track of this node's RNA "base" path (the part of the pat identifying the node)
       * if the old node-tree has animation data which potentially covers this node. */
      if (ntree.adt) {
        PointerRNA ptr;
        char *path;

        RNA_pointer_create(&ntree.id, &RNA_Node, node, &ptr);
        path = RNA_path_from_ID_to_struct(&ptr);

        if (path) {
          BLI_addtail(&anim_basepaths, animation_basepath_change_new(path, path));
        }
      }

      /* ensure valid parent pointers, detach if parent stays outside the group */
      if (node->parent && !(node->parent->flag & NODE_SELECT)) {
        nodeDetachNode(node);
      }

      /* change node-collection membership */
      BLI_remlink(&ntree.nodes, node);
      BLI_addtail(&ngroup->nodes, node);
      BKE_ntree_update_tag_node_removed(&ntree);
      BKE_ntree_update_tag_node_new(ngroup, node);

      /* ensure unique node name in the ngroup */
      nodeUniqueName(ngroup, node);
    }
  }

  /* move animation data over */
  if (ntree.adt) {
    BKE_animdata_transfer_by_basepath(bmain, &ntree.id, &ngroup->id, &anim_basepaths);

    /* paths + their wrappers need to be freed */
    LISTBASE_FOREACH_MUTABLE (AnimationBasePathChange *, basepath_change, &anim_basepaths) {
      animation_basepath_change_free(basepath_change);
    }
  }

  /* node groups don't use internal cached data */
  ntreeFreeCache(ngroup);

  /* create input node */
  bNode *input_node = nodeAddStaticNode(&C, ngroup, NODE_GROUP_INPUT);
  input_node->locx = real_min[0] - center[0] - offsetx;
  input_node->locy = -offsety;

  /* create output node */
  bNode *output_node = nodeAddStaticNode(&C, ngroup, NODE_GROUP_OUTPUT);
  output_node->locx = real_max[0] - center[0] + offsetx * 0.25f;
  output_node->locy = -offsety;

  /* relink external sockets */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    int fromselect = node_group_make_use_node(*link->fromnode, gnode);
    int toselect = node_group_make_use_node(*link->tonode, gnode);

    if ((fromselect && link->tonode == gnode) || (toselect && link->fromnode == gnode)) {
      /* remove all links to/from the gnode.
       * this can remove link information, but there's no general way to preserve it.
       */
      nodeRemLink(&ntree, link);
    }
    else if (toselect && !fromselect) {
      /* Remove hidden links to not create unconnected sockets in the interface. */
      if (nodeLinkIsHidden(link)) {
        nodeRemLink(&ntree, link);
        continue;
      }

      bNodeSocket *link_sock;
      bNode *link_node;
      node_socket_skip_reroutes(&ntree.links, link->tonode, link->tosock, &link_node, &link_sock);
      bNodeSocket *iosock = ntreeAddSocketInterfaceFromSocket(ngroup, link_node, link_sock);

      /* update the group node and interface node sockets,
       * so the new interface socket can be linked.
       */
      node_group_update(&ntree, gnode);
      node_group_input_update(ngroup, input_node);

      /* create new internal link */
      bNodeSocket *input_sock = node_group_input_find_socket(input_node, iosock->identifier);
      nodeAddLink(ngroup, input_node, input_sock, link->tonode, link->tosock);

      /* redirect external link */
      link->tonode = gnode;
      link->tosock = node_group_find_input_socket(gnode, iosock->identifier);
    }
    else if (fromselect && !toselect) {
      /* Remove hidden links to not create unconnected sockets in the interface. */
      if (nodeLinkIsHidden(link)) {
        nodeRemLink(&ntree, link);
        continue;
      }

      /* First check whether the source of this link is already connected to an output.
       * If yes, reuse that output instead of duplicating it. */
      bool connected = false;
      LISTBASE_FOREACH (bNodeLink *, olink, &ngroup->links) {
        if (olink->fromsock == link->fromsock && olink->tonode == output_node) {
          bNodeSocket *output_sock = node_group_find_output_socket(gnode,
                                                                   olink->tosock->identifier);
          link->fromnode = gnode;
          link->fromsock = output_sock;
          connected = true;
        }
      }

      if (!connected) {
        bNodeSocket *link_sock;
        bNode *link_node;
        node_socket_skip_reroutes(
            &ntree.links, link->fromnode, link->fromsock, &link_node, &link_sock);
        bNodeSocket *iosock = ntreeAddSocketInterfaceFromSocket(ngroup, link_node, link_sock);

        /* update the group node and interface node sockets,
         * so the new interface socket can be linked.
         */
        node_group_update(&ntree, gnode);
        node_group_output_update(ngroup, output_node);

        /* create new internal link */
        bNodeSocket *output_sock = node_group_output_find_socket(output_node, iosock->identifier);
        nodeAddLink(ngroup, link->fromnode, link->fromsock, output_node, output_sock);

        /* redirect external link */
        link->fromnode = gnode;
        link->fromsock = node_group_find_output_socket(gnode, iosock->identifier);
      }
    }
  }

  /* move internal links */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    int fromselect = node_group_make_use_node(*link->fromnode, gnode);
    int toselect = node_group_make_use_node(*link->tonode, gnode);

    if (fromselect && toselect) {
      BLI_remlink(&ntree.links, link);
      BLI_addtail(&ngroup->links, link);
    }
  }

  /* move nodes in the group to the center */
  LISTBASE_FOREACH (bNode *, node, &ngroup->nodes) {
    if (node_group_make_use_node(*node, gnode) && !node->parent) {
      node->locx -= center[0];
      node->locy -= center[1];
    }
  }

  /* Expose all unlinked sockets too but only the visible ones. */
  if (expose_visible) {
    LISTBASE_FOREACH (bNode *, node, &ngroup->nodes) {
      if (node_group_make_use_node(*node, gnode)) {
        LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
          bool skip = false;
          LISTBASE_FOREACH (bNodeLink *, link, &ngroup->links) {
            if (link->tosock == sock) {
              skip = true;
              break;
            }
          }
          if (sock->flag & (SOCK_HIDDEN | SOCK_UNAVAIL)) {
            skip = true;
          }
          if (skip) {
            continue;
          }

          bNodeSocket *iosock = ntreeAddSocketInterfaceFromSocket(ngroup, node, sock);

          node_group_input_update(ngroup, input_node);

          /* create new internal link */
          bNodeSocket *input_sock = node_group_input_find_socket(input_node, iosock->identifier);
          nodeAddLink(ngroup, input_node, input_sock, node, sock);
        }

        LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
          bool skip = false;
          LISTBASE_FOREACH (bNodeLink *, link, &ngroup->links) {
            if (link->fromsock == sock) {
              skip = true;
            }
          }
          if (sock->flag & (SOCK_HIDDEN | SOCK_UNAVAIL)) {
            skip = true;
          }
          if (skip) {
            continue;
          }

          bNodeSocket *iosock = ntreeAddSocketInterfaceFromSocket(ngroup, node, sock);

          node_group_output_update(ngroup, output_node);

          /* create new internal link */
          bNodeSocket *output_sock = node_group_output_find_socket(output_node,
                                                                   iosock->identifier);
          nodeAddLink(ngroup, node, sock, output_node, output_sock);
        }
      }
    }
  }
}

static bNode *node_group_make_from_selected(const bContext &C,
                                            bNodeTree &ntree,
                                            const char *ntype,
                                            const char *ntreetype)
{
  Main *bmain = CTX_data_main(&C);

  float2 min, max;
  const int totselect = node_get_selected_minmax(ntree, nullptr, min, max, false);
  /* don't make empty group */
  if (totselect == 0) {
    return nullptr;
  }

  /* New node-tree. */
  bNodeTree *ngroup = ntreeAddTree(bmain, "NodeGroup", ntreetype);

  /* make group node */
  bNode *gnode = nodeAddNode(&C, &ntree, ntype);
  gnode->id = (ID *)ngroup;

  gnode->locx = 0.5f * (min[0] + max[0]);
  gnode->locy = 0.5f * (min[1] + max[1]);

  node_group_make_insert_selected(C, ntree, gnode);

  return gnode;
}

static int node_group_make_exec(bContext *C, wmOperator *op)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;
  const char *ntree_idname = group_ntree_idname(C);
  const char *node_idname = node_group_idname(C);
  Main *bmain = CTX_data_main(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  if (!node_group_make_test_selected(ntree, nullptr, ntree_idname, *op->reports)) {
    return OPERATOR_CANCELLED;
  }

  bNode *gnode = node_group_make_from_selected(*C, ntree, node_idname, ntree_idname);

  if (gnode) {
    bNodeTree *ngroup = (bNodeTree *)gnode->id;

    nodeSetActive(&ntree, gnode);
    if (ngroup) {
      ED_node_tree_push(&snode, ngroup, gnode);
      LISTBASE_FOREACH (bNode *, node, &ngroup->nodes) {
        sort_multi_input_socket_links(snode, *node, nullptr, nullptr);
      }
    }
  }

  ED_node_tree_propagate_change(C, bmain, nullptr);

  /* We broke relations in node tree, need to rebuild them in the graphs. */
  DEG_relations_tag_update(bmain);

  return OPERATOR_FINISHED;
}

void NODE_OT_group_make(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Group";
  ot->description = "Make group from selected nodes";
  ot->idname = "NODE_OT_group_make";

  /* api callbacks */
  ot->exec = node_group_make_exec;
  ot->poll = node_group_operator_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Group Insert Operator
 * \{ */

static int node_group_insert_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;
  const char *node_idname = node_group_idname(C);
  Main *bmain = CTX_data_main(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *gnode = node_group_get_active(C, node_idname);

  if (!gnode || !gnode->id) {
    return OPERATOR_CANCELLED;
  }

  bNodeTree *ngroup = (bNodeTree *)gnode->id;
  if (!node_group_make_test_selected(*ntree, gnode, ngroup->idname, *op->reports)) {
    return OPERATOR_CANCELLED;
  }

  node_group_make_insert_selected(*C, *ntree, gnode);

  nodeSetActive(ntree, gnode);
  ED_node_tree_push(snode, ngroup, gnode);
  ED_node_tree_propagate_change(C, bmain, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_group_insert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Group Insert";
  ot->description = "Insert selected nodes into a node group";
  ot->idname = "NODE_OT_group_insert";

  /* api callbacks */
  ot->exec = node_group_insert_exec;
  ot->poll = node_group_operator_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::space_node
