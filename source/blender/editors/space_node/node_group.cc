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

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_report.hh"

#include "ANIM_action.hh"

#include "DEG_depsgraph_build.hh"

#include "ED_node.hh"
#include "ED_node_preview.hh"
#include "ED_render.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_resources.hh"

#include "NOD_common.hh"
#include "NOD_composite.hh"
#include "NOD_geometry.hh"
#include "NOD_node_declaration.hh"
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

static StringRef group_ntree_idname(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  return snode->tree_idname;
}

StringRef node_group_idname(const bContext *C)
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

static bNode *node_group_get_active(bContext *C, const StringRef node_idname)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNode *node = bke::node_get_active(*snode->edittree);

  if (node && node->idname == node_idname) {
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
    if (bke::all_zone_input_node_types().contains(dst_node->type_legacy)) {
      const bke::bNodeZoneType &zone_type = *bke::zone_type_by_node_type(dst_node->type_legacy);
      int &output_node_id = zone_type.get_corresponding_output_id(*dst_node);
      if (output_node_id == 0) {
        continue;
      }
      output_node_id = identifier_map.lookup_default(output_node_id, 0);
      if (output_node_id == 0) {
        blender::nodes::update_node_declaration_and_sockets(dst_tree, *dst_node);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Group Operator
 * \{ */

static wmOperatorStatus node_group_edit_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  const StringRef node_idname = node_group_idname(C);
  const bool exit = RNA_boolean_get(op->ptr, "exit");

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  bNode *gnode = node_group_get_active(C, node_idname);

  if (gnode && !exit) {
    bNodeTree *ngroup = (bNodeTree *)gnode->id;

    if (ngroup) {
      ED_node_tree_push(region, snode, ngroup, gnode);
    }
  }
  else {
    ED_node_tree_pop(region, snode);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_NODES, nullptr);
  WM_event_add_notifier(C, NC_NODE | ND_NODE_GIZMO, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_group_edit(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Group";
  ot->description = "Edit node group";
  ot->idname = "NODE_OT_group_edit";

  /* API callbacks. */
  ot->exec = node_group_edit_exec;
  ot->poll = node_group_operator_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_boolean(ot->srna, "exit", false, "Exit", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enter group at cursor, or exit when not hovering any node.
 * \{ */

static wmOperatorStatus node_group_enter_exit_invoke(bContext *C,
                                                     wmOperator * /*op*/,
                                                     const wmEvent *event)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  ARegion &region = *CTX_wm_region(C);

  /* Don't interfer when the mouse is interacting with some button. See #147282. */
  if (ISMOUSE_BUTTON(event->type) && UI_but_find_mouse_over(&region, event)) {
    return OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED;
  }

  float2 cursor;
  UI_view2d_region_to_view(&region.v2d, event->mval[0], event->mval[1], &cursor.x, &cursor.y);
  bNode *node = node_under_mouse_get(snode, cursor);

  if (!node || node->is_frame()) {
    ED_node_tree_pop(&region, &snode);
    return OPERATOR_FINISHED;
  }
  if (!node->is_group()) {
    return OPERATOR_PASS_THROUGH;
  }
  if (node->is_custom_group()) {
    return OPERATOR_PASS_THROUGH;
  }
  bNodeTree *group = id_cast<bNodeTree *>(node->id);
  if (!group || ID_MISSING(group)) {
    return OPERATOR_PASS_THROUGH;
  }
  ED_node_tree_push(&region, &snode, group, node);
  return OPERATOR_FINISHED;
}

void NODE_OT_group_enter_exit(wmOperatorType *ot)
{
  ot->name = "Enter/Exit Group";
  ot->description = "Enter or exit node group based on cursor location";
  ot->idname = "NODE_OT_group_enter_exit";

  ot->invoke = node_group_enter_exit_invoke;
  ot->poll = node_group_operator_active_poll;

  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ungroup Operator
 * \{ */

/**
 * The given paths will be owned by the returned instance.
 * Both pointers are allowed to point to the same string.
 */
static AnimationBasePathChange *animation_basepath_change_new(const StringRef src_basepath,
                                                              const StringRef dst_basepath)
{
  AnimationBasePathChange *basepath_change = (AnimationBasePathChange *)MEM_callocN(
      sizeof(*basepath_change), AT);
  basepath_change->src_basepath = BLI_strdupn(src_basepath.data(), src_basepath.size());
  basepath_change->dst_basepath = BLI_strdupn(dst_basepath.data(), dst_basepath.size());
  return basepath_change;
}

static void animation_basepath_change_free(AnimationBasePathChange *basepath_change)
{
  if (basepath_change->src_basepath != basepath_change->dst_basepath) {
    MEM_freeN(basepath_change->src_basepath);
  }
  MEM_freeN(basepath_change->dst_basepath);
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
static void node_group_ungroup(Main *bmain, bNodeTree *ntree, bNode *gnode)
{
  ListBase anim_basepaths = {nullptr, nullptr};
  Vector<bNode *> nodes_delayed_free;
  const bNodeTree *ngroup = reinterpret_cast<const bNodeTree *>(gnode->id);

  /* `wgroup` is a temporary copy of the #NodeTree we're merging in
   * - All of wgroup's nodes are copied across to their new home.
   * - `ngroup` (i.e. the source NodeTree) is left unscathed.
   * - Temp copy. do change ID user-count for the copies.
   */
  bNodeTree *wgroup = bke::node_tree_copy_tree(bmain, *ngroup);

  /* Add the nodes into the `ntree`. */
  Vector<bNode *> new_nodes;
  Map<int32_t, int32_t> node_identifier_map;
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &wgroup->nodes) {
    new_nodes.append(node);
    /* Remove interface nodes.
     * This also removes remaining links to and from interface nodes.
     */
    if (node->is_group_input() || node->is_group_output()) {
      /* We must delay removal since sockets will reference this node. see: #52092 */
      nodes_delayed_free.append(node);
    }

    /* Keep track of this node's RNA "base" path (the part of the path identifying the node)
     * if the old node-tree has animation data which potentially covers this node. */
    std::optional<std::string> old_animation_basepath;
    if (wgroup->adt) {
      PointerRNA ptr = RNA_pointer_create_discrete(&wgroup->id, &RNA_Node, node);
      old_animation_basepath = RNA_path_from_ID_to_struct(&ptr);
    }

    /* migrate node */
    BLI_remlink(&wgroup->nodes, node);
    BLI_addtail(&ntree->nodes, node);
    const int32_t old_identifier = node->identifier;
    bke::node_unique_id(*ntree, *node);
    bke::node_unique_name(*ntree, *node);
    node_identifier_map.add(old_identifier, node->identifier);

    BKE_ntree_update_tag_node_new(ntree, node);

    if (wgroup->adt) {
      PointerRNA ptr = RNA_pointer_create_discrete(&ntree->id, &RNA_Node, node);
      const std::optional<std::string> new_animation_basepath = RNA_path_from_ID_to_struct(&ptr);
      BLI_addtail(&anim_basepaths,
                  animation_basepath_change_new(*old_animation_basepath, *new_animation_basepath));
    }

    node->location[0] += gnode->location[0];
    node->location[1] += gnode->location[1];

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
    /* firstly, wgroup needs to temporary dummy action
     * that can be destroyed, as it shares copies */
    bAction *waction = reinterpret_cast<bAction *>(BKE_id_copy(bmain, &wgroup->adt->action->id));
    const bool assign_ok = animrig::assign_action(waction, {wgroup->id, *wgroup->adt});
    BLI_assert_msg(assign_ok, "assigning a copy of an already-assigned Action should work");
    UNUSED_VARS_NDEBUG(assign_ok);

    /* now perform the moving */
    BKE_animdata_transfer_by_basepath(bmain, &wgroup->id, &ntree->id, &anim_basepaths);

    /* paths + their wrappers need to be freed */
    LISTBASE_FOREACH_MUTABLE (AnimationBasePathChange *, basepath_change, &anim_basepaths) {
      animation_basepath_change_free(basepath_change);
    }

    /* free temp action too */
    if (waction) {
      const bool unassign_ok = animrig::unassign_action({wgroup->id, *wgroup->adt});
      BLI_assert_msg(unassign_ok, "unassigning an Action that was just assigned should work");
      UNUSED_VARS_NDEBUG(unassign_ok);
      BKE_id_free(bmain, waction);
    }
  }

  remap_pairing(*ntree, new_nodes, node_identifier_map);

  /* free the group tree (takes care of user count) */
  BKE_id_free(bmain, wgroup);

  /* restore external links to and from the gnode */

  /* input links */
  if (glinks_first != nullptr) {
    for (bNodeLink *link = glinks_first->next; link != glinks_last->next; link = link->next) {
      if (link->fromnode->is_group_input()) {
        const char *identifier = link->fromsock->identifier;
        int num_external_links = 0;

        /* find external links to this input */
        for (bNodeLink *tlink = (bNodeLink *)ntree->links.first; tlink != glinks_first->next;
             tlink = tlink->next)
        {
          if (tlink->tonode == gnode && STREQ(tlink->tosock->identifier, identifier)) {
            bke::node_add_link(
                *ntree, *tlink->fromnode, *tlink->fromsock, *link->tonode, *link->tosock);
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
             tlink = tlink->next)
        {
          /* only use active output node */
          if (tlink->tonode->is_group_output() && (tlink->tonode->flag & NODE_DO_OUTPUT)) {
            if (STREQ(tlink->tosock->identifier, identifier)) {
              bke::node_add_link(
                  *ntree, *tlink->fromnode, *tlink->fromsock, *link->tonode, *link->tosock);
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
    bke::node_remove_node(bmain, *ntree, *node, false);
  }

  update_nested_node_refs_after_ungroup(*ntree, *ngroup, *gnode, node_identifier_map);

  /* delete the group instance and dereference group tree */
  bke::node_remove_node(bmain, *ntree, *gnode, true);
}

static wmOperatorStatus node_group_ungroup_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  const StringRef node_idname = node_group_idname(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), bmain);

  Vector<bNode *> nodes_to_ungroup;
  for (bNode *node : snode->edittree->all_nodes()) {
    if (node->flag & NODE_SELECT) {
      if (node->idname == node_idname) {
        if (node->id != nullptr) {
          nodes_to_ungroup.append(node);
        }
      }
    }
  }
  if (nodes_to_ungroup.is_empty()) {
    return OPERATOR_CANCELLED;
  }
  for (bNode *node : nodes_to_ungroup) {
    node_group_ungroup(bmain, snode->edittree, node);
  }
  BKE_main_ensure_invariants(*CTX_data_main(C));
  return OPERATOR_FINISHED;
}

void NODE_OT_group_ungroup(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Ungroup";
  ot->description = "Ungroup selected nodes";
  ot->idname = "NODE_OT_group_ungroup";

  /* API callbacks. */
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
      newnode = bke::node_copy_with_mapping(
          &ntree, *node, LIB_ID_COPY_DEFAULT, std::nullopt, std::nullopt, socket_map);
      node_identifier_map.add(node->identifier, newnode->identifier);
    }
    else {
      newnode = node;
      BLI_remlink(&ngroup.nodes, newnode);
      BLI_addtail(&ntree.nodes, newnode);
      const int32_t old_identifier = node->identifier;
      bke::node_unique_id(ntree, *newnode);
      bke::node_unique_name(ntree, *newnode);
      node_identifier_map.add(old_identifier, newnode->identifier);
    }
    node_map.add_new(node, newnode);

    /* Keep track of this node's RNA "base" path (the part of the path identifying the node)
     * if the old node-tree has animation data which potentially covers this node. */
    if (ngroup.adt) {
      PointerRNA ptr = RNA_pointer_create_discrete(&ngroup.id, &RNA_Node, newnode);
      if (const std::optional<std::string> path = RNA_path_from_ID_to_struct(&ptr)) {
        BLI_addtail(&anim_basepaths, animation_basepath_change_new(*path, *path));
      }
    }

    /* ensure valid parent pointers, detach if parent stays inside the group */
    if (newnode->parent && !(newnode->parent->flag & NODE_SELECT)) {
      bke::node_detach_node(ngroup, *newnode);
    }

    newnode->location[0] += offset.x;
    newnode->location[1] += offset.y;
  }
  if (!make_copy) {
    bke::node_rebuild_id_vector(ngroup);
  }

  /* add internal links to the ntree */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ngroup.links) {
    const bool fromselect = (link->fromnode && nodes_to_move.contains(link->fromnode));
    const bool toselect = (link->tonode && nodes_to_move.contains(link->tonode));

    if (make_copy) {
      /* make a copy of internal links */
      if (fromselect && toselect) {
        bke::node_add_link(ntree,
                           *node_map.lookup(link->fromnode),
                           *socket_map.lookup(link->fromsock),
                           *node_map.lookup(link->tonode),
                           *socket_map.lookup(link->tosock));
      }
    }
    else {
      /* move valid links over, delete broken links */
      if (fromselect && toselect) {
        BLI_remlink(&ngroup.links, link);
        BLI_addtail(&ntree.links, link);
      }
      else if (fromselect || toselect) {
        bke::node_remove_link(&ngroup, *link);
      }
    }
  }

  remap_pairing(ntree, nodes_to_move, node_identifier_map);

  for (bNode *node : node_map.values()) {
    bke::node_declaration_ensure(ntree, *node);
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

static wmOperatorStatus node_group_separate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ARegion *region = CTX_wm_region(C);
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
  ED_node_tree_pop(region, snode);

  BKE_main_ensure_invariants(*CTX_data_main(C));

  return OPERATOR_FINISHED;
}

static wmOperatorStatus node_group_separate_invoke(bContext *C,
                                                   wmOperator * /*op*/,
                                                   const wmEvent * /*event*/)
{
  uiPopupMenu *pup = UI_popup_menu_begin(
      C, CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Separate"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  layout->operator_context_set(wm::OpCallContext::ExecDefault);
  PointerRNA op_ptr = layout->op("NODE_OT_group_separate", IFACE_("Copy"), ICON_NONE);
  RNA_enum_set(&op_ptr, "type", NODE_GS_COPY);
  op_ptr = layout->op("NODE_OT_group_separate", IFACE_("Move"), ICON_NONE);
  RNA_enum_set(&op_ptr, "type", NODE_GS_MOVE);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void NODE_OT_group_separate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Separate";
  ot->description = "Separate selected nodes from the node group";
  ot->idname = "NODE_OT_group_separate";

  /* API callbacks. */
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
                                          const StringRef ntree_idname,
                                          ReportList &reports)
{
  if (nodes_to_group.is_empty()) {
    return false;
  }
  /* make a local pseudo node tree to pass to the node poll functions */
  bNodeTree *ngroup = bke::node_tree_add_tree(nullptr, "Pseudo Node Group", ntree_idname);
  BLI_SCOPED_DEFER([&]() {
    bke::node_tree_free_tree(*ngroup);
    MEM_freeN(ngroup);
  });

  /* check poll functions for selected nodes */
  for (bNode *node : nodes_to_group) {
    const char *disabled_hint = nullptr;
    if (node->typeinfo->poll_instance &&
        !node->typeinfo->poll_instance(node, ngroup, &disabled_hint))
    {
      if (disabled_hint) {
        BKE_reportf(&reports,
                    RPT_WARNING,
                    "Cannot add node '%s' in a group:\n  %s",
                    node->name,
                    disabled_hint);
      }
      else {
        BKE_reportf(&reports, RPT_WARNING, "Cannot add node '%s' in a group", node->name);
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
  /* Check if zone pairs are fully selected.
   * Zone input or output nodes can only be grouped together with the paired node. */
  for (const bke::bNodeZoneType *zone_type : bke::all_zone_types()) {
    for (bNode *input_node : ntree.nodes_by_type(zone_type->input_idname)) {
      if (bNode *output_node = zone_type->get_corresponding_output(ntree, *input_node)) {
        const bool input_selected = nodes_to_group.contains(input_node);
        const bool output_selected = nodes_to_group.contains(output_node);
        if (input_selected && !output_selected) {
          BKE_reportf(&reports,
                      RPT_WARNING,
                      "Cannot add zone input node '%s' to a group without its paired output '%s'",
                      input_node->name,
                      output_node->name);
          return false;
        }
        if (output_selected && !input_selected) {
          BKE_reportf(&reports,
                      RPT_WARNING,
                      "Cannot add zone output node '%s' to a group without its paired input '%s'",
                      output_node->name,
                      input_node->name);
          return false;
        }
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
    float2 loc(node->location);
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
  RandomNumberGenerator rng = RandomNumberGenerator::from_random_seed();
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
  group.nested_node_refs = MEM_malloc_arrayN<bNestedNodeRef>(new_nested_node_refs.size(),
                                                             __func__);
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

  /* If only one node is selected expose all its sockets regardless of links. */
  const bool expose_visible = nodes_to_move.size() == 1;

  /* Reuse an existing output node or create a new one. */
  group.ensure_topology_cache();
  bNode *output_node = [&]() {
    if (bNode *node = group.group_output_node()) {
      return node;
    }
    bNode *output_node = bke::node_add_static_node(&C, group, NODE_GROUP_OUTPUT);
    output_node->location[0] = real_max[0] - center[0] + 50.0f;
    return output_node;
  }();

  /* Create new group input node for easier organization of the new nodes inside the group. */
  bNode *input_node = bke::node_add_static_node(&C, group, NODE_GROUP_INPUT);
  input_node->location[0] = real_min[0] - center[0] - 200.0f;

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

  struct NewInternalLinkInfo {
    bNode *node;
    bNodeSocket *socket;
    const bNodeTreeInterfaceSocket *interface_socket;
  };

  /* Map from single non-selected output sockets to potentially many selected input sockets. */
  Map<bNodeSocket *, InputSocketInfo> input_links;
  Vector<OutputLinkInfo> output_links;
  Set<bNodeLink *> internal_links_to_move;
  Set<bNodeLink *> links_to_remove;
  /* Map old to new node identifiers. */
  Map<int32_t, int32_t> node_identifier_map;
  Vector<NewInternalLinkInfo> new_internal_links;

  ntree.ensure_topology_cache();
  /* Add all outputs first. */
  for (bNode *node : nodes_to_move) {
    for (bNodeSocket *output_socket : node->output_sockets()) {
      if (!output_socket->is_visible()) {
        for (bNodeLink *link : output_socket->directly_linked_links()) {
          links_to_remove.add(link);
        }
        continue;
      }

      for (bNodeLink *link : output_socket->directly_linked_links()) {
        if (bke::node_link_is_hidden(*link)) {
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
      if (expose_visible && !output_socket->is_directly_linked()) {
        bNodeTreeInterfaceSocket *io_socket = bke::node_interface::add_interface_socket_from_node(
            group, *node, *output_socket);
        if (io_socket) {
          new_internal_links.append({node, output_socket, io_socket});
        }
      }
    }
  }
  /* Now add all inputs. */
  for (bNode *node : nodes_to_move) {
    for (bNodeSocket *input_socket : node->input_sockets()) {
      if (!input_socket->is_visible()) {
        for (bNodeLink *link : input_socket->directly_linked_links()) {
          links_to_remove.add(link);
        }
        continue;
      }

      for (bNodeLink *link : input_socket->directly_linked_links()) {
        if (bke::node_link_is_hidden(*link)) {
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
      }
      if (expose_visible && !input_socket->is_directly_linked()) {
        bNodeTreeInterfaceSocket *io_socket = bke::node_interface::add_interface_socket_from_node(
            group, *node, *input_socket);
        if (io_socket) {
          new_internal_links.append({node, input_socket, io_socket});
        }
      }
    }
  }

  /* Un-parent nodes when only the parent or child moves into the group. */
  for (bNode *node : ntree.all_nodes()) {
    if (node->parent && nodes_to_move.contains(node->parent) && !nodes_to_move.contains(node)) {
      bke::node_detach_node(ntree, *node);
    }
  }
  for (bNode *node : nodes_to_move) {
    if (node->parent && !nodes_to_move.contains(node->parent)) {
      bke::node_detach_node(ntree, *node);
    }
  }

  /* Move animation data from the parent tree to the group. */
  if (ntree.adt) {
    ListBase anim_basepaths = {nullptr, nullptr};
    for (bNode *node : nodes_to_move) {
      PointerRNA ptr = RNA_pointer_create_discrete(&ntree.id, &RNA_Node, node);
      if (const std::optional<std::string> path = RNA_path_from_ID_to_struct(&ptr)) {
        BLI_addtail(&anim_basepaths, animation_basepath_change_new(*path, *path));
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
    bke::node_unique_id(group, *node);
    bke::node_unique_name(group, *node);

    node_identifier_map.add(old_identifier, node->identifier);

    BKE_ntree_update_tag_node_removed(&ntree);
    BKE_ntree_update_tag_node_new(&group, node);
  }
  bke::node_rebuild_id_vector(ntree);

  /* Update input and output node first, since the group node declaration can depend on them. */
  nodes::update_node_declaration_and_sockets(group, *input_node);
  nodes::update_node_declaration_and_sockets(group, *output_node);

  /* move nodes in the group to the center */
  for (bNode *node : nodes_to_move) {
    node->location[0] -= center[0];
    node->location[1] -= center[1];
  }

  for (bNodeLink *link : internal_links_to_move) {
    BLI_remlink(&ntree.links, link);
    BLI_addtail(&group.links, link);
    BKE_ntree_update_tag_link_removed(&ntree);
    BKE_ntree_update_tag_link_added(&group, link);
  }

  for (bNodeLink *link : links_to_remove) {
    bke::node_remove_link(&ntree, *link);
  }

  /* Handle links to the new group inputs. */
  for (const auto item : input_links.items()) {
    const StringRefNull interface_identifier = item.value.interface_socket->identifier;
    bNodeSocket *input_socket = node_group_input_find_socket(input_node, interface_identifier);

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
    bNodeSocket *output_sock = node_group_output_find_socket(output_node, io_identifier);
    bke::node_add_link(
        group, *info.link->fromnode, *info.link->fromsock, *output_node, *output_sock);
  }

  /* Handle new links inside the group. */
  for (const NewInternalLinkInfo &info : new_internal_links) {
    const StringRefNull io_identifier = info.interface_socket->identifier;
    if (info.socket->in_out == SOCK_IN) {
      bNodeSocket *input_socket = node_group_input_find_socket(input_node, io_identifier);
      bke::node_add_link(group, *input_node, *input_socket, *info.node, *info.socket);
    }
    else {
      bNodeSocket *output_socket = node_group_output_find_socket(output_node, io_identifier);
      bke::node_add_link(group, *info.node, *info.socket, *output_node, *output_socket);
    }
  }

  remap_pairing(group, nodes_to_move, node_identifier_map);

  if (group.type == NTREE_GEOMETRY) {
    bke::node_field_inferencing::update_field_inferencing(group);
  }

  if (ELEM(group.type, NTREE_GEOMETRY, NTREE_COMPOSIT)) {
    bke::node_structure_type_inferencing::update_structure_type_interface(group);
  }

  nodes::update_node_declaration_and_sockets(ntree, *gnode);

  /* Add new links to inputs outside of the group. */
  for (const auto item : input_links.items()) {
    const StringRefNull interface_identifier = item.value.interface_socket->identifier;
    bNodeSocket *group_node_socket = node_group_find_input_socket(gnode, interface_identifier);
    bke::node_add_link(ntree, *item.value.from_node, *item.key, *gnode, *group_node_socket);
  }

  /* Add new links to outputs outside the group. */
  for (const OutputLinkInfo &info : output_links) {
    /* Reconnect the link to the group node instead of the node now inside the group. */
    info.link->fromnode = gnode;
    info.link->fromsock = node_group_find_output_socket(gnode, info.interface_socket->identifier);
  }

  update_nested_node_refs_after_moving_nodes_into_group(ntree, group, *gnode, node_identifier_map);

  BKE_main_ensure_invariants(*bmain);
}

static bNode *node_group_make_from_nodes(const bContext &C,
                                         bNodeTree &ntree,
                                         const VectorSet<bNode *> &nodes_to_group,
                                         const StringRef ntype,
                                         const StringRef ntreetype)
{
  Main *bmain = CTX_data_main(&C);

  float2 min, max;
  get_min_max_of_nodes(nodes_to_group, false, min, max);

  /* New node-tree. */
  bNodeTree *ngroup = bke::node_tree_add_tree(bmain, "NodeGroup", ntreetype);

  BKE_id_move_to_same_lib(*bmain, ngroup->id, ntree.id);

  /* make group node */
  bNode *gnode = bke::node_add_node(&C, ntree, ntype);
  gnode->id = (ID *)ngroup;

  gnode->location[0] = 0.5f * (min[0] + max[0]);
  gnode->location[1] = 0.5f * (min[1] + max[1]);

  node_group_make_insert_selected(C, ntree, gnode, nodes_to_group);

  return gnode;
}

struct WrapperNodeGroupMapping {
  int num_inputs = 0;
  int num_outputs = 0;
  Map<const bNodeSocket *, int> new_index_by_src_socket;
  Map<int, int> new_by_old_panel_identifier;
  Vector<int> exposed_input_indices;
  Vector<int> exposed_output_indices;

  bNodeSocket *get_new_input(const bNodeSocket *old_socket, bNode &new_node) const
  {
    if (const std::optional<int> index = new_index_by_src_socket.lookup_try(old_socket)) {
      return &new_node.input_socket(*index);
    }
    return nullptr;
  }

  bNodeSocket *get_new_output(const bNodeSocket *old_socket, bNode &new_node) const
  {
    if (const std::optional<int> index = new_index_by_src_socket.lookup_try(old_socket)) {
      return &new_node.output_socket(*index);
    }
    return nullptr;
  }
};

static void add_node_group_interface_from_declaration_recursive(
    bNodeTree &group,
    const bNode &src_node,
    const nodes::ItemDeclaration &item_decl,
    bNodeTreeInterfacePanel *parent,
    WrapperNodeGroupMapping &r_mapping)
{
  if (const nodes::SocketDeclaration *socket_decl = dynamic_cast<const nodes::SocketDeclaration *>(
          &item_decl))
  {
    const bNodeSocket &socket = src_node.socket_by_decl(*socket_decl);
    if (!socket.is_available()) {
      return;
    }
    bNodeTreeInterfaceSocket *io_socket = bke::node_interface::add_interface_socket_from_node(
        group, src_node, socket);
    if (!io_socket) {
      return;
    }
    group.tree_interface.move_item_to_parent(io_socket->item, parent, INT32_MAX);
    if (socket.is_input()) {
      r_mapping.new_index_by_src_socket.add_new(&socket, r_mapping.num_inputs++);
      r_mapping.exposed_input_indices.append(socket.index());
    }
    else {
      r_mapping.new_index_by_src_socket.add_new(&socket, r_mapping.num_outputs++);
      r_mapping.exposed_output_indices.append(socket.index());
    }
  }
  else if (const nodes::PanelDeclaration *panel_decl =
               dynamic_cast<const nodes::PanelDeclaration *>(&item_decl))
  {
    NodeTreeInterfacePanelFlag flag{};
    if (panel_decl->default_collapsed) {
      flag |= NODE_INTERFACE_PANEL_DEFAULT_CLOSED;
    }
    bNodeTreeInterfacePanel *io_panel = group.tree_interface.add_panel(
        panel_decl->name, panel_decl->description, flag, parent);
    r_mapping.new_by_old_panel_identifier.add_new(panel_decl->identifier, io_panel->identifier);
    for (const nodes::ItemDeclaration *child_item_decl : panel_decl->items) {
      add_node_group_interface_from_declaration_recursive(
          group, src_node, *child_item_decl, io_panel, r_mapping);
    }
  }
}

static bNodeTree *node_group_make_wrapper(const bContext &C,
                                          const bNodeTree &src_tree,
                                          const bNode &src_node,
                                          WrapperNodeGroupMapping &r_mapping)
{
  Main &bmain = *CTX_data_main(&C);

  bNodeTree *dst_group = bke::node_tree_add_tree(
      &bmain, bke::node_label(src_tree, src_node), src_tree.idname);
  dst_group->color_tag = int(bke::node_color_tag(src_node));

  const nodes::NodeDeclaration &node_decl = *src_node.declaration();
  for (const nodes::ItemDeclaration *item_decl : node_decl.root_items) {
    add_node_group_interface_from_declaration_recursive(
        *dst_group, src_node, *item_decl, nullptr, r_mapping);
  }

  /* Add the node that make up the wrapper node group. */
  bNode &input_node = *bke::node_add_static_node(&C, *dst_group, NODE_GROUP_INPUT);
  bNode &output_node = *bke::node_add_static_node(&C, *dst_group, NODE_GROUP_OUTPUT);

  Map<const bNodeSocket *, bNodeSocket *> inner_node_socket_mapping;
  bNode &inner_node = *bke::node_copy_with_mapping(
      dst_group, src_node, 0, std::nullopt, std::nullopt, inner_node_socket_mapping);

  /* Position nodes. */
  input_node.location[0] = -300 - input_node.width;
  output_node.location[0] = 300;
  inner_node.location[0] = -src_node.width / 2;
  inner_node.location[1] = 0;
  inner_node.width = src_node.width;
  inner_node.parent = nullptr;

  /* This makes sure that all nodes have the correct sockets so that we can link. */
  BKE_main_ensure_invariants(bmain, dst_group->id);

  /* Expand all panels in wrapper node group. */
  for (bNodePanelState &panel_state : inner_node.panel_states()) {
    panel_state.flag &= ~NODE_PANEL_COLLAPSED;
  }
  /* Make all sockets visible in wrapper node group. */
  for (bNodeSocket *socket : inner_node.input_sockets()) {
    socket->flag &= ~SOCK_HIDDEN;
  }
  for (bNodeSocket *socket : inner_node.output_sockets()) {
    socket->flag &= ~SOCK_HIDDEN;
  }

  const Array<bNodeSocket *> group_inputs = input_node.output_sockets().drop_back(1);
  const Array<bNodeSocket *> group_outputs = output_node.input_sockets().drop_back(1);
  const Array<bNodeSocket *> inner_inputs = inner_node.input_sockets();
  const Array<bNodeSocket *> inner_outputs = inner_node.output_sockets();
  BLI_assert(group_inputs.size() == r_mapping.exposed_input_indices.size());
  BLI_assert(group_outputs.size() == r_mapping.exposed_output_indices.size());

  /* Add links. */
  for (const int i : group_inputs.index_range()) {
    bke::node_add_link(*dst_group,
                       input_node,
                       *group_inputs[i],
                       inner_node,
                       *inner_inputs[r_mapping.exposed_input_indices[i]]);
  }
  for (const int i : group_outputs.index_range()) {
    bke::node_add_link(*dst_group,
                       inner_node,
                       *inner_outputs[r_mapping.exposed_output_indices[i]],
                       output_node,
                       *group_outputs[i]);
  }

  BKE_main_ensure_invariants(bmain, dst_group->id);
  return dst_group;
}

static bNode *node_group_make_from_node_declaration(bContext &C,
                                                    bNodeTree &ntree,
                                                    bNode &src_node,
                                                    const StringRef node_idname)
{
  Main &bmain = *CTX_data_main(&C);

  WrapperNodeGroupMapping mapping;
  bNodeTree *wrapper_group = node_group_make_wrapper(C, ntree, src_node, mapping);

  /* Create a group node. */
  bNode *gnode = bke::node_add_node(&C, ntree, node_idname);
  STRNCPY_UTF8(gnode->name, BKE_id_name(wrapper_group->id));
  bke::node_unique_name(ntree, *gnode);

  /* Assign the newly created wrapper group to the new group node. */
  gnode->id = &wrapper_group->id;

  /* Position node exactly where the old node was. */
  gnode->parent = src_node.parent;
  gnode->width = std::max<float>(src_node.width, GROUP_NODE_MIN_WIDTH);
  copy_v2_v2(gnode->location, src_node.location);

  BKE_main_ensure_invariants(bmain);
  ntree.ensure_topology_cache();

  /* Keep old socket visibility. */
  for (const bNodeSocket *src_socket : src_node.input_sockets()) {
    if (bNodeSocket *new_socket = mapping.get_new_input(src_socket, *gnode)) {
      new_socket->flag |= src_socket->flag & (SOCK_HIDDEN | SOCK_COLLAPSED);
    }
  }
  for (const bNodeSocket *src_socket : src_node.output_sockets()) {
    if (bNodeSocket *new_socket = mapping.get_new_output(src_socket, *gnode)) {
      new_socket->flag |= src_socket->flag & (SOCK_HIDDEN | SOCK_COLLAPSED);
    }
  }

  /* Keep old panel collapse status. */
  const Span<bNodePanelState> src_panel_states = src_node.panel_states();
  MutableSpan<bNodePanelState> new_panel_states = gnode->panel_states();
  for (const bNodePanelState &src_panel_state : src_panel_states) {
    if (const std::optional<int> new_identifier = mapping.new_by_old_panel_identifier.lookup_try(
            src_panel_state.identifier))
    {
      for (bNodePanelState &new_panel_state : new_panel_states) {
        if (new_panel_state.identifier == *new_identifier) {
          SET_FLAG_FROM_TEST(new_panel_state.flag,
                             src_panel_state.flag & NODE_PANEL_COLLAPSED,
                             NODE_PANEL_COLLAPSED);
        }
      }
    }
  }

  /* Relink links from old to new node. */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    if (link->tonode == &src_node) {
      if (bNodeSocket *new_to_socket = mapping.get_new_input(link->tosock, *gnode)) {
        link->tonode = gnode;
        link->tosock = new_to_socket;
        continue;
      }
      bke::node_remove_link(&ntree, *link);
      continue;
    }
    if (link->fromnode == &src_node) {
      if (bNodeSocket *new_from_socket = mapping.get_new_output(link->fromsock, *gnode)) {
        link->fromnode = gnode;
        link->fromsock = new_from_socket;
        continue;
      }
      bke::node_remove_link(&ntree, *link);
      continue;
    }
  }

  /* Remove the old node because it has been replaced. Use the name of the removed node for the new
   * group node. This also keeps animation data working. */
  std::string old_node_name = src_node.name;
  bke::node_remove_node(&bmain, ntree, src_node, true, false);
  STRNCPY(gnode->name, old_node_name.c_str());

  BKE_ntree_update_tag_node_property(&ntree, gnode);
  BKE_main_ensure_invariants(bmain);
  return gnode;
}

static wmOperatorStatus node_group_make_exec(bContext *C, wmOperator *op)
{
  ARegion &region = *CTX_wm_region(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &ntree = *snode.edittree;
  const StringRef ntree_idname = group_ntree_idname(C);
  const StringRef node_idname = node_group_idname(C);
  Main *bmain = CTX_data_main(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  VectorSet<bNode *> nodes_to_group = get_nodes_to_group(ntree, nullptr);
  if (!node_group_make_test_selected(ntree, nodes_to_group, ntree_idname, *op->reports)) {
    return OPERATOR_CANCELLED;
  }

  bNode *gnode = nullptr;
  if (nodes_to_group.size() == 1 && nodes_to_group[0]->declaration()) {
    gnode = node_group_make_from_node_declaration(*C, ntree, *nodes_to_group[0], node_idname);
  }
  else {
    gnode = node_group_make_from_nodes(*C, ntree, nodes_to_group, node_idname, ntree_idname);
  }

  if (gnode) {
    bNodeTree *ngroup = (bNodeTree *)gnode->id;

    bke::node_set_active(ntree, *gnode);
    if (ngroup) {
      ED_node_tree_push(&region, &snode, ngroup, gnode);
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

  /* API callbacks. */
  ot->exec = node_group_make_exec;
  ot->poll = node_group_operator_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Group Insert Operator
 * \{ */

static wmOperatorStatus node_group_insert_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  bNodeTree *ntree = snode->edittree;
  const StringRef node_idname = node_group_idname(C);

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

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
    if (bke::node_tree_contains_tree(*reinterpret_cast<bNodeTree *>(group->id), *ngroup)) {
      BKE_reportf(
          op->reports, RPT_WARNING, "Cannot insert group '%s' in '%s'", group->name, gnode->name);
      return OPERATOR_CANCELLED;
    }
  }

  if (!node_group_make_test_selected(*ntree, nodes_to_group, ngroup->idname, *op->reports)) {
    return OPERATOR_CANCELLED;
  }

  node_group_make_insert_selected(*C, *ntree, gnode, nodes_to_group);

  bke::node_set_active(*ntree, *gnode);
  ED_node_tree_push(region, snode, ngroup, gnode);

  return OPERATOR_FINISHED;
}

void NODE_OT_group_insert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Group Insert";
  ot->description = "Insert selected nodes into a node group";
  ot->idname = "NODE_OT_group_insert";

  /* API callbacks. */
  ot->exec = node_group_insert_exec;
  ot->poll = node_group_operator_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Default Group Width Operator
 * \{ */

static bool node_default_group_width_set_poll(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (!snode) {
    return false;
  }
  bNodeTree *ntree = snode->edittree;
  if (!ntree) {
    return false;
  }
  if (!ID_IS_EDITABLE(ntree)) {
    return false;
  }
  if (snode->nodetree == snode->edittree) {
    /* Top-level node group does not have enough context to set the node width. */
    CTX_wm_operator_poll_msg_set(C, "There is no parent group node in this context");
    return false;
  }
  return true;
}

static wmOperatorStatus node_default_group_width_set_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *ntree = snode->edittree;

  bNodeTreePath *last_path_item = static_cast<bNodeTreePath *>(snode->treepath.last);
  bNodeTreePath *parent_path_item = last_path_item->prev;
  if (!parent_path_item) {
    return OPERATOR_CANCELLED;
  }
  bNodeTree *parent_ntree = parent_path_item->nodetree;
  if (!parent_ntree) {
    return OPERATOR_CANCELLED;
  }
  parent_ntree->ensure_topology_cache();
  bNode *parent_node = bke::node_find_node_by_name(*parent_ntree, last_path_item->node_name);
  if (!parent_node) {
    return OPERATOR_CANCELLED;
  }
  ntree->default_group_node_width = parent_node->width;
  WM_event_add_notifier(C, NC_NODE | NA_EDITED, nullptr);
  return OPERATOR_CANCELLED;
}

void NODE_OT_default_group_width_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Default Group Node Width";
  ot->description = "Set the width based on the parent group node in the current context";
  ot->idname = "NODE_OT_default_group_width_set";

  /* API callbacks. */
  ot->exec = node_default_group_width_set_exec;
  ot->poll = node_default_group_width_set_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::space_node
