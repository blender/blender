/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_node_types.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "PIL_time.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_report.h"

#include "DEG_depsgraph_build.h"

#include "ED_node.hh" /* own include */
#include "ED_node.hh"
#include "ED_node_preview.hh"
#include "ED_render.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_resources.hh"

#include "NOD_common.h"
#include "NOD_composite.h"
#include "NOD_geometry.hh"
#include "NOD_shader.h"
#include "NOD_socket.hh"
#include "NOD_texture.h"

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
                 "GeometryNodeTree"))
    {
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
        ED_node_is_geometry(snode))
    {
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
    return ntreeType_Shader->group_idname;
  }
  if (ED_node_is_compositor(snode)) {
    return ntreeType_Composite->group_idname;
  }
  if (ED_node_is_texture(snode)) {
    return ntreeType_Texture->group_idname;
  }
  if (ED_node_is_geometry(snode)) {
    return ntreeType_Geometry->group_idname;
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

/* Maps old to new identifiers for simulation input node pairing. */
static void remap_pairing(bNodeTree &dst_tree,
                          Span<bNode *> nodes,
                          const Map<int32_t, int32_t> &identifier_map)
{
  for (bNode *dst_node : nodes) {
    switch (dst_node->type) {
      case GEO_NODE_SIMULATION_INPUT: {
        NodeGeometrySimulationInput *data = static_cast<NodeGeometrySimulationInput *>(
            dst_node->storage);
        if (data->output_node_id == 0) {
          continue;
        }

        data->output_node_id = identifier_map.lookup_default(data->output_node_id, 0);
        if (data->output_node_id == 0) {
          blender::nodes::update_node_declaration_and_sockets(dst_tree, *dst_node);
        }
        break;
      }
      case GEO_NODE_REPEAT_INPUT: {
        NodeGeometryRepeatInput *data = static_cast<NodeGeometryRepeatInput *>(dst_node->storage);
        if (data->output_node_id == 0) {
          continue;
        }

        data->output_node_id = identifier_map.lookup_default(data->output_node_id, 0);
        if (data->output_node_id == 0) {
          blender::nodes::update_node_declaration_and_sockets(dst_tree, *dst_node);
        }
        break;
      }
    }
  }
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
  stop_preview_job(*CTX_wm_manager(C));

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

static void update_nested_node_refs_after_ungroup(bNodeTree &ntree,
                                                  const bNodeTree &ngroup,
                                                  const bNode &gnode,
                                                  const Map<int32_t, int32_t> &node_identifier_map)
{
  for (bNestedNodeRef &ref : ntree.nested_node_refs_span()) {
    if (ref.path.node_id != gnode.identifier) {
      continue;
    }
    const bNestedNodeRef *child_ref = ngroup.find_nested_node_ref(ref.path.id_in_node);
    if (!child_ref) {
      continue;
    }
    constexpr int32_t missing_id = -1;
    const int32_t new_node_id = node_identifier_map.lookup_default(child_ref->path.node_id,
                                                                   missing_id);
    if (new_node_id == missing_id) {
      continue;
    }
    ref.path.node_id = new_node_id;
    ref.path.id_in_node = child_ref->path.id_in_node;
  }
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
  bNodeTree *wgroup = bke::ntreeCopyTree(bmain, ngroup);

  /* Add the nodes into the `ntree`. */
  Vector<bNode *> new_nodes;
  Map<int32_t, int32_t> node_identifier_map;
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &wgroup->nodes) {
    new_nodes.append(node);
    /* Remove interface nodes.
     * This also removes remaining links to and from interface nodes.
     */
    if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
      /* We must delay removal since sockets will reference this node. see: #52092 */
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
    const int32_t old_identifier = node->identifier;
    nodeUniqueID(ntree, node);
    nodeUniqueName(ntree, node);
    node_identifier_map.add(old_identifier, node->identifier);

    BKE_ntree_update_tag_node_new(ntree, node);

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
  wgroup->runtime->nodes_by_id.clear();

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

  remap_pairing(*ntree, new_nodes, node_identifier_map);

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
             tlink = tlink->next)
        {
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
         link = link->next)
    {
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

  update_nested_node_refs_after_ungroup(*ntree, *ngroup, *gnode, node_identifier_map);

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
  node_deselect_all(ntree);

  ListBase anim_basepaths = {nullptr, nullptr};

  Map<bNode *, bNode *> node_map;
  Map<const bNodeSocket *, bNodeSocket *> socket_map;
  Map<int32_t, int32_t> node_identifier_map;

  /* Add selected nodes into the ntree, ignoring interface nodes. */
  VectorSet<bNode *> nodes_to_move = get_selected_nodes(ngroup);
  nodes_to_move.remove_if(
      [](const bNode *node) { return node->is_group_input() || node->is_group_output(); });

  for (bNode *node : nodes_to_move) {
    bNode *newnode;
    if (make_copy) {
      newnode = bke::node_copy_with_mapping(&ntree, *node, LIB_ID_COPY_DEFAULT, true, socket_map);
      node_identifier_map.add(node->identifier, newnode->identifier);
    }
    else {
      newnode = node;
      BLI_remlink(&ngroup.nodes, newnode);
      BLI_addtail(&ntree.nodes, newnode);
      const int32_t old_identifier = node->identifier;
      nodeUniqueID(&ntree, newnode);
      nodeUniqueName(&ntree, newnode);
      node_identifier_map.add(old_identifier, newnode->identifier);
    }
    node_map.add_new(node, newnode);

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
      nodeDetachNode(&ngroup, newnode);
    }

    if (!newnode->parent) {
      newnode->locx += offset.x;
      newnode->locy += offset.y;
    }
  }
  if (!make_copy) {
    bke::nodeRebuildIDVector(&ngroup);
  }

  /* add internal links to the ntree */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ngroup.links) {
    const bool fromselect = (link->fromnode && nodes_to_move.contains(link->fromnode));
    const bool toselect = (link->tonode && nodes_to_move.contains(link->tonode));

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

  remap_pairing(ntree, nodes_to_move, node_identifier_map);

  for (bNode *node : node_map.values()) {
    bke::nodeDeclarationEnsure(&ntree, node);
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
  stop_preview_job(*CTX_wm_manager(C));

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

static int node_group_separate_invoke(bContext *C, wmOperator * /*op*/, const wmEvent * /*event*/)
{
  uiPopupMenu *pup = UI_popup_menu_begin(
      C, CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Separate"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
  uiItemEnumO(layout, "NODE_OT_group_separate", nullptr, ICON_NONE, "type", NODE_GS_COPY);
  uiItemEnumO(layout, "NODE_OT_group_separate", nullptr, ICON_NONE, "type", NODE_GS_MOVE);

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

static VectorSet<bNode *> get_nodes_to_group(bNodeTree &node_tree, bNode *group_node)
{
  VectorSet<bNode *> nodes_to_group = get_selected_nodes(node_tree);
  nodes_to_group.remove_if(
      [](bNode *node) { return node->is_group_input() || node->is_group_output(); });
  nodes_to_group.remove(group_node);
  return nodes_to_group;
}

static bool node_group_make_test_selected(bNodeTree &ntree,
                                          const VectorSet<bNode *> &nodes_to_group,
                                          const char *ntree_idname,
                                          ReportList &reports)
{
  if (nodes_to_group.is_empty()) {
    return false;
  }
  /* make a local pseudo node tree to pass to the node poll functions */
  bNodeTree *ngroup = ntreeAddTree(nullptr, "Pseudo Node Group", ntree_idname);
  BLI_SCOPED_DEFER([&]() {
    bke::ntreeFreeTree(ngroup);
    MEM_freeN(ngroup);
  });

  /* check poll functions for selected nodes */
  for (bNode *node : nodes_to_group) {
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
      return false;
    }
  }

  /* check if all connections are OK, no unselected node has both
   * inputs and outputs to a selection */
  ntree.ensure_topology_cache();
  for (bNode *node : ntree.all_nodes()) {
    if (nodes_to_group.contains(node)) {
      continue;
    }
    auto sockets_connected_to_group = [&](const Span<bNodeSocket *> sockets) {
      for (const bNodeSocket *socket : sockets) {
        for (const bNodeSocket *other_socket : socket->directly_linked_sockets()) {
          if (nodes_to_group.contains(const_cast<bNode *>(&other_socket->owner_node()))) {
            return true;
          }
        }
      }
      return false;
    };
    if (sockets_connected_to_group(node->input_sockets()) &&
        sockets_connected_to_group(node->output_sockets()))
    {
      return false;
    }
  }
  /* Check if simulation zone pairs are fully selected.
   * Simulation input or output nodes can only be grouped together with the paired node.
   */
  for (bNode *input_node : ntree.nodes_by_type("GeometryNodeSimulationInput")) {
    const NodeGeometrySimulationInput &input_data =
        *static_cast<const NodeGeometrySimulationInput *>(input_node->storage);

    if (bNode *output_node = ntree.node_by_id(input_data.output_node_id)) {
      const bool input_selected = nodes_to_group.contains(input_node);
      const bool output_selected = nodes_to_group.contains(output_node);
      if (input_selected && !output_selected) {
        BKE_reportf(
            &reports,
            RPT_WARNING,
            "Can not add simulation input node '%s' to a group without its paired output '%s'",
            input_node->name,
            output_node->name);
        return false;
      }
      if (output_selected && !input_selected) {
        BKE_reportf(
            &reports,
            RPT_WARNING,
            "Can not add simulation output node '%s' to a group without its paired input '%s'",
            output_node->name,
            input_node->name);
        return false;
      }
    }
  }
  for (bNode *input_node : ntree.nodes_by_type("GeometryNodeRepeatInput")) {
    const NodeGeometryRepeatInput &input_data = *static_cast<const NodeGeometryRepeatInput *>(
        input_node->storage);

    if (bNode *output_node = ntree.node_by_id(input_data.output_node_id)) {
      const bool input_selected = nodes_to_group.contains(input_node);
      const bool output_selected = nodes_to_group.contains(output_node);
      if (input_selected && !output_selected) {
        BKE_reportf(&reports,
                    RPT_WARNING,
                    "Can not add repeat input node '%s' to a group without its paired output '%s'",
                    input_node->name,
                    output_node->name);
        return false;
      }
      if (output_selected && !input_selected) {
        BKE_reportf(&reports,
                    RPT_WARNING,
                    "Can not add repeat output node '%s' to a group without its paired input '%s'",
                    output_node->name,
                    input_node->name);
        return false;
      }
    }
  }

  return true;
}

static void get_min_max_of_nodes(const Span<bNode *> nodes,
                                 const bool use_size,
                                 float2 &min,
                                 float2 &max)
{
  if (nodes.is_empty()) {
    min = float2(0);
    max = float2(0);
    return;
  }

  INIT_MINMAX2(min, max);
  for (const bNode *node : nodes) {
    const float2 node_offset = {node->offsetx, node->offsety};
    float2 loc = bke::nodeToView(node, node_offset);
    math::min_max(loc, min, max);
    if (use_size) {
      loc.x += node->width;
      loc.y -= node->height;
      math::min_max(loc, min, max);
    }
  }
}

/**
 * Skip reroute nodes when finding the socket to use as an example for a new group interface
 * item. This moves "inward" into nodes selected for grouping to find properties like whether a
 * connected socket has a hidden value. It only works in trivial situations-- a single line of
 * connected reroutes with no branching.
 */
static const bNodeSocket &find_socket_to_use_for_interface(const bNodeTree &node_tree,
                                                           const bNodeSocket &socket)
{
  if (node_tree.has_available_link_cycle()) {
    return socket;
  }
  const bNode &node = socket.owner_node();
  if (!node.is_reroute()) {
    return socket;
  }
  const bNodeSocket &other_socket = socket.in_out == SOCK_IN ? node.output_socket(0) :
                                                               node.input_socket(0);
  if (!other_socket.is_logically_linked()) {
    return socket;
  }
  return *other_socket.logically_linked_sockets().first();
}

/**
 * The output sockets of group nodes usually have consciously given names so they have
 * precedence over socket names the link points to.
 */
static bool prefer_node_for_interface_name(const bNode &node)
{
  return node.is_group() || node.is_group_input() || node.is_group_output();
}

static bNodeTreeInterfaceSocket *add_interface_from_socket(const bNodeTree &original_tree,
                                                           bNodeTree &tree_for_interface,
                                                           const bNodeSocket &socket)
{
  /* The "example socket" has to have the same `in_out` status as the new interface socket. */
  const bNodeSocket &socket_for_io = find_socket_to_use_for_interface(original_tree, socket);
  const bNode &node_for_io = socket_for_io.owner_node();
  const bNodeSocket &socket_for_name = prefer_node_for_interface_name(socket.owner_node()) ?
                                           socket :
                                           socket_for_io;
  return bke::node_interface::add_interface_socket_from_node(
      tree_for_interface, node_for_io, socket_for_io, socket_for_io.idname, socket_for_name.name);
}

static void update_nested_node_refs_after_moving_nodes_into_group(
    bNodeTree &ntree,
    bNodeTree &group,
    bNode &gnode,
    const Map<int32_t, int32_t> &node_identifier_map)
{
  /* Update nested node references in the parent and child node tree. */
  RandomNumberGenerator rng(PIL_check_seconds_timer_i() & UINT_MAX);
  Vector<bNestedNodeRef> new_nested_node_refs;
  /* Keep all nested node references that were in the group before. */
  for (const bNestedNodeRef &ref : group.nested_node_refs_span()) {
    new_nested_node_refs.append(ref);
  }
  Set<int32_t> used_nested_node_ref_ids;
  for (const bNestedNodeRef &ref : group.nested_node_refs_span()) {
    used_nested_node_ref_ids.add(ref.id);
  }
  Map<bNestedNodePath, int32_t> new_id_by_old_path;
  for (bNestedNodeRef &ref : ntree.nested_node_refs_span()) {
    const int32_t new_node_id = node_identifier_map.lookup_default(ref.path.node_id, -1);
    if (new_node_id == -1) {
      /* The node was not moved between node groups. */
      continue;
    }
    bNestedNodeRef new_ref = ref;
    new_ref.path.node_id = new_node_id;
    /* Find new unique identifier for the nested node ref. */
    while (true) {
      const int32_t new_id = rng.get_int32(INT32_MAX);
      if (used_nested_node_ref_ids.add(new_id)) {
        new_ref.id = new_id;
        break;
      }
    }
    new_id_by_old_path.add_new(ref.path, new_ref.id);
    new_nested_node_refs.append(new_ref);
    /* Updated the nested node ref in the parent so that it points to the same node that is now
     * inside of a nested group. */
    ref.path.node_id = gnode.identifier;
    ref.path.id_in_node = new_ref.id;
  }
  MEM_SAFE_FREE(group.nested_node_refs);
  group.nested_node_refs = static_cast<bNestedNodeRef *>(
      MEM_malloc_arrayN(new_nested_node_refs.size(), sizeof(bNestedNodeRef), __func__));
  uninitialized_copy_n(
      new_nested_node_refs.data(), new_nested_node_refs.size(), group.nested_node_refs);
  group.nested_node_refs_num = new_nested_node_refs.size();
}

static void node_group_make_insert_selected(const bContext &C,
                                            bNodeTree &ntree,
                                            bNode *gnode,
                                            const VectorSet<bNode *> &nodes_to_move)
{
  Main *bmain = CTX_data_main(&C);
  bNodeTree &group = *reinterpret_cast<bNodeTree *>(gnode->id);
  BLI_assert(!nodes_to_move.contains(gnode));

  node_deselect_all(group);

  float2 min, max;
  get_min_max_of_nodes(nodes_to_move, false, min, max);
  const float2 center = math::midpoint(min, max);

  float2 real_min, real_max;
  get_min_max_of_nodes(nodes_to_move, true, real_min, real_max);

  /* Reuse an existing output node or create a new one. */
  group.ensure_topology_cache();
  bNode *output_node = [&]() {
    if (bNode *node = group.group_output_node()) {
      return node;
    }
    bNode *output_node = nodeAddStaticNode(&C, &group, NODE_GROUP_OUTPUT);
    output_node->locx = real_max[0] - center[0] + 50.0f;
    return output_node;
  }();

  /* Create new group input node for easier organization of the new nodes inside the group. */
  bNode *input_node = nodeAddStaticNode(&C, &group, NODE_GROUP_INPUT);
  input_node->locx = real_min[0] - center[0] - 200.0f;

  struct InputSocketInfo {
    /* The unselected node the original link came from. */
    bNode *from_node;
    /* All the links that came from the socket on the unselected node. */
    Vector<bNodeLink *> links;
    const bNodeTreeInterfaceSocket *interface_socket;
  };

  struct OutputLinkInfo {
    bNodeLink *link;
    const bNodeTreeInterfaceSocket *interface_socket;
  };

  /* Map from single non-selected output sockets to potentially many selected input sockets. */
  Map<bNodeSocket *, InputSocketInfo> input_links;
  Vector<OutputLinkInfo> output_links;
  Set<bNodeLink *> internal_links_to_move;
  Set<bNodeLink *> links_to_remove;
  /* Map old to new node identifiers. */
  Map<int32_t, int32_t> node_identifier_map;

  ntree.ensure_topology_cache();
  for (bNode *node : nodes_to_move) {
    for (bNodeSocket *output_socket : node->output_sockets()) {
      for (bNodeLink *link : output_socket->directly_linked_links()) {
        if (nodeLinkIsHidden(link)) {
          links_to_remove.add(link);
          continue;
        }
        if (link->tonode == gnode) {
          links_to_remove.add(link);
          continue;
        }
        if (nodes_to_move.contains(link->tonode)) {
          internal_links_to_move.add(link);
          continue;
        }
        bNodeTreeInterfaceSocket *io_socket = add_interface_from_socket(
            ntree, group, *link->fromsock);
        if (io_socket) {
          output_links.append({link, io_socket});
        }
        else {
          links_to_remove.add(link);
        }
      }
    }
    for (bNodeSocket *input_socket : node->input_sockets()) {
      for (bNodeLink *link : input_socket->directly_linked_links()) {
        if (nodeLinkIsHidden(link)) {
          links_to_remove.add(link);
          continue;
        }
        if (link->fromnode == gnode) {
          links_to_remove.add(link);
          continue;
        }
        if (nodes_to_move.contains(link->fromnode)) {
          internal_links_to_move.add(link);
          continue;
        }
        InputSocketInfo &info = input_links.lookup_or_add_default(link->fromsock);
        info.from_node = link->fromnode;
        info.links.append(link);
        if (!info.interface_socket) {
          info.interface_socket = add_interface_from_socket(ntree, group, *link->tosock);
        }
        else {
          links_to_remove.add(link);
        }
      }
    }
  }

  struct NewInternalLinkInfo {
    bNode *node;
    bNodeSocket *socket;
    const bNodeTreeInterfaceSocket *interface_socket;
  };

  const bool expose_visible = nodes_to_move.size() == 1;
  Vector<NewInternalLinkInfo> new_internal_links;
  if (expose_visible) {
    for (bNode *node : nodes_to_move) {
      auto expose_sockets = [&](const Span<bNodeSocket *> sockets) {
        for (bNodeSocket *socket : sockets) {
          if (!socket->is_available() || socket->is_hidden()) {
            continue;
          }
          if (socket->is_directly_linked()) {
            continue;
          }
          const bNodeTreeInterfaceSocket *io_socket =
              bke::node_interface::add_interface_socket_from_node(group, *node, *socket);
          if (io_socket) {
            new_internal_links.append({node, socket, io_socket});
          }
        }
      };
      expose_sockets(node->input_sockets());
      expose_sockets(node->output_sockets());
    }
  }

  /* Un-parent nodes when only the parent or child moves into the group. */
  for (bNode *node : ntree.all_nodes()) {
    if (node->parent && nodes_to_move.contains(node->parent) && !nodes_to_move.contains(node)) {
      nodeDetachNode(&ntree, node);
    }
  }
  for (bNode *node : nodes_to_move) {
    if (node->parent && !nodes_to_move.contains(node->parent)) {
      nodeDetachNode(&ntree, node);
    }
  }

  /* Move animation data from the parent tree to the group. */
  if (ntree.adt) {
    ListBase anim_basepaths = {nullptr, nullptr};
    for (bNode *node : nodes_to_move) {
      PointerRNA ptr;
      RNA_pointer_create(&ntree.id, &RNA_Node, node, &ptr);
      if (char *path = RNA_path_from_ID_to_struct(&ptr)) {
        BLI_addtail(&anim_basepaths, animation_basepath_change_new(path, path));
      }
    }
    BKE_animdata_transfer_by_basepath(bmain, &ntree.id, &group.id, &anim_basepaths);

    LISTBASE_FOREACH_MUTABLE (AnimationBasePathChange *, basepath_change, &anim_basepaths) {
      animation_basepath_change_free(basepath_change);
    }
  }

  /* Move nodes into the group. */
  for (bNode *node : nodes_to_move) {
    const int32_t old_identifier = node->identifier;

    BLI_remlink(&ntree.nodes, node);
    BLI_addtail(&group.nodes, node);
    nodeUniqueID(&group, node);
    nodeUniqueName(&group, node);

    node_identifier_map.add(old_identifier, node->identifier);

    BKE_ntree_update_tag_node_removed(&ntree);
    BKE_ntree_update_tag_node_new(&group, node);
  }
  bke::nodeRebuildIDVector(&ntree);

  /* Update input and output node first, since the group node declaration can depend on them. */
  nodes::update_node_declaration_and_sockets(group, *input_node);
  nodes::update_node_declaration_and_sockets(group, *output_node);

  /* move nodes in the group to the center */
  for (bNode *node : nodes_to_move) {
    if (!node->parent) {
      node->locx -= center[0];
      node->locy -= center[1];
    }
  }

  for (bNodeLink *link : internal_links_to_move) {
    BLI_remlink(&ntree.links, link);
    BLI_addtail(&group.links, link);
    BKE_ntree_update_tag_link_removed(&ntree);
    BKE_ntree_update_tag_link_added(&group, link);
  }

  for (bNodeLink *link : links_to_remove) {
    nodeRemLink(&ntree, link);
  }

  /* Handle links to the new group inputs. */
  for (const auto item : input_links.items()) {
    const StringRefNull interface_identifier = item.value.interface_socket->identifier;
    bNodeSocket *input_socket = node_group_input_find_socket(input_node,
                                                             interface_identifier.c_str());

    for (bNodeLink *link : item.value.links) {
      /* Move the link into the new group, connected from the input node to the original socket. */
      BLI_remlink(&ntree.links, link);
      BLI_addtail(&group.links, link);
      BKE_ntree_update_tag_link_removed(&ntree);
      BKE_ntree_update_tag_link_added(&group, link);
      link->fromnode = input_node;
      link->fromsock = input_socket;
    }
  }

  /* Handle links to new group outputs. */
  for (const OutputLinkInfo &info : output_links) {
    /* Create a new link inside of the group. */
    const StringRefNull io_identifier = info.interface_socket->identifier;
    bNodeSocket *output_sock = node_group_output_find_socket(output_node, io_identifier.c_str());
    nodeAddLink(&group, info.link->fromnode, info.link->fromsock, output_node, output_sock);
  }

  /* Handle new links inside the group. */
  for (const NewInternalLinkInfo &info : new_internal_links) {
    const StringRefNull io_identifier = info.interface_socket->identifier;
    if (info.socket->in_out == SOCK_IN) {
      bNodeSocket *input_socket = node_group_input_find_socket(input_node, io_identifier.c_str());
      nodeAddLink(&group, input_node, input_socket, info.node, info.socket);
    }
    else {
      bNodeSocket *output_socket = node_group_output_find_socket(output_node,
                                                                 io_identifier.c_str());
      nodeAddLink(&group, info.node, info.socket, output_node, output_socket);
    }
  }

  remap_pairing(group, nodes_to_move, node_identifier_map);

  if (group.type == NTREE_GEOMETRY) {
    bke::node_field_inferencing::update_field_inferencing(group);
  }
  nodes::update_node_declaration_and_sockets(ntree, *gnode);

  /* Add new links to inputs outside of the group. */
  for (const auto item : input_links.items()) {
    const StringRefNull interface_identifier = item.value.interface_socket->identifier;
    bNodeSocket *group_node_socket = node_group_find_input_socket(gnode,
                                                                  interface_identifier.c_str());
    nodeAddLink(&ntree, item.value.from_node, item.key, gnode, group_node_socket);
  }

  /* Add new links to outputs outside the group. */
  for (const OutputLinkInfo &info : output_links) {
    /* Reconnect the link to the group node instead of the node now inside the group. */
    info.link->fromnode = gnode;
    info.link->fromsock = node_group_find_output_socket(gnode, info.interface_socket->identifier);
  }

  update_nested_node_refs_after_moving_nodes_into_group(ntree, group, *gnode, node_identifier_map);

  ED_node_tree_propagate_change(&C, bmain, nullptr);
}

static bNode *node_group_make_from_nodes(const bContext &C,
                                         bNodeTree &ntree,
                                         const VectorSet<bNode *> &nodes_to_group,
                                         const char *ntype,
                                         const char *ntreetype)
{
  Main *bmain = CTX_data_main(&C);

  float2 min, max;
  get_min_max_of_nodes(nodes_to_group, false, min, max);

  /* New node-tree. */
  bNodeTree *ngroup = ntreeAddTree(bmain, "NodeGroup", ntreetype);

  /* make group node */
  bNode *gnode = nodeAddNode(&C, &ntree, ntype);
  gnode->id = (ID *)ngroup;

  gnode->locx = 0.5f * (min[0] + max[0]);
  gnode->locy = 0.5f * (min[1] + max[1]);

  node_group_make_insert_selected(C, ntree, gnode, nodes_to_group);

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
  stop_preview_job(*CTX_wm_manager(C));

  VectorSet<bNode *> nodes_to_group = get_nodes_to_group(ntree, nullptr);
  if (!node_group_make_test_selected(ntree, nodes_to_group, ntree_idname, *op->reports)) {
    return OPERATOR_CANCELLED;
  }

  bNode *gnode = node_group_make_from_nodes(*C, ntree, nodes_to_group, node_idname, ntree_idname);

  if (gnode) {
    bNodeTree *ngroup = (bNodeTree *)gnode->id;

    nodeSetActive(&ntree, gnode);
    if (ngroup) {
      ED_node_tree_push(&snode, ngroup, gnode);
    }
  }

  WM_event_add_notifier(C, NC_NODE | NA_ADDED, nullptr);

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

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));
  stop_preview_job(*CTX_wm_manager(C));

  bNode *gnode = node_group_get_active(C, node_idname);
  if (!gnode || !gnode->id) {
    return OPERATOR_CANCELLED;
  }

  bNodeTree *ngroup = reinterpret_cast<bNodeTree *>(gnode->id);
  VectorSet<bNode *> nodes_to_group = get_nodes_to_group(*ntree, gnode);

  /* Make sure that there won't be a node group containing itself afterwards. */
  for (const bNode *group : nodes_to_group) {
    if (!group->is_group() || group->id == nullptr) {
      continue;
    }
    if (ntreeContainsTree(reinterpret_cast<bNodeTree *>(group->id), ngroup)) {
      BKE_reportf(
          op->reports, RPT_WARNING, "Can not insert group '%s' in '%s'", group->name, gnode->name);
      return OPERATOR_CANCELLED;
    }
  }

  if (!node_group_make_test_selected(*ntree, nodes_to_group, ngroup->idname, *op->reports)) {
    return OPERATOR_CANCELLED;
  }

  node_group_make_insert_selected(*C, *ntree, gnode, nodes_to_group);

  nodeSetActive(ntree, gnode);
  ED_node_tree_push(snode, ngroup, gnode);

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
