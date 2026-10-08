/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 * \brief Node breadcrumbs drawing
 */

#include <algorithm>

#include "BLI_listbase.hh"
#include "BLI_string.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "BKE_compositor.hh"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_node_c.hh"
#include "ED_screen.hh"

#include "SEQ_modifier.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"

#include "WM_api.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_intern.hh"

namespace blender {

struct Material;

namespace ed::space_node {

static void context_path_add_object_data(Vector<ui::ContextPathItem> &path, Object &object)
{
  if (!object.data) {
    return;
  }
  if (object.type == OB_MESH) {
    ui::context_path_add_generic(path, *RNA_Mesh, object.data);
  }
  else if (object.type == OB_CURVES) {
    ui::context_path_add_generic(path, *RNA_Curves, object.data);
  }
  else if (object.type == OB_LAMP) {
    ui::context_path_add_generic(path, *RNA_Light, object.data);
  }
  else if (ELEM(object.type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    ui::context_path_add_generic(path, *RNA_Curve, object.data);
  }
}

static std::function<void(bContext &)> tree_path_handle_func(int i)
{
  return [i](bContext &C) {
    wmOperatorType *ot = WM_operatortype_find("NODE_OT_tree_path_parent", false);
    PointerRNA op_props = WM_operator_properties_create_ptr(ot);
    RNA_int_set(&op_props, "parent_tree_index", i);
    WM_operator_name_call_ptr(&C, ot, wm::OpCallContext::InvokeDefault, &op_props, nullptr);
    WM_operator_properties_free(&op_props);
  };
}

static BIFIconID node_tree_icon(const ID &tree_id)
{
  if (ID_IS_PACKED(&tree_id)) {
    return ICON_PACKAGE;
  }
  if (ID_IS_LINKED(&tree_id)) {
    return ICON_LINKED;
  }
  if (ID_IS_ASSET(&tree_id)) {
    return ICON_ASSET_MANAGER;
  }
  return ICON_NODETREE;
}

static bool node_group_is_enterable(const bNode &node)
{
  return node.is_group() && !node.is_custom_group() && node.id && !ID_MISSING(node.id);
}

static bool node_tree_has_enterable_group(const bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return false;
  }
  ntree->ensure_topology_cache();
  for (const bNode *group_node : ntree->group_nodes()) {
    if (node_group_is_enterable(*group_node)) {
      return true;
    }
  }
  return false;
}

static void navigate_menu_create(bContext *C, ui::Layout *layout, void *arg)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *tree = static_cast<bNodeTree *>(arg);
  if (!snode || !tree) {
    return;
  }

  bNodeTree *child_tree_in_path = nullptr;
  for (bNodeTreePath &path_item : snode->treepath) {
    if (path_item.nodetree == tree && path_item.next) {
      child_tree_in_path = path_item.next->nodetree;
      break;
    }
  }

  tree->ensure_topology_cache();
  Set<bNodeTree *> visited_groups;
  Vector<bNode *> group_nodes;
  for (bNode *group_node : tree->group_nodes()) {
    if (!node_group_is_enterable(*group_node)) {
      continue;
    }
    /* When a group is used multiple times, the menu enters the first node found. */
    if (visited_groups.add(id_cast<bNodeTree *>(group_node->id))) {
      group_nodes.append(group_node);
    }
  }
  std::ranges::sort(group_nodes, [](const bNode *a, const bNode *b) {
    return BLI_strcasecmp_natural(BKE_id_name(*a->id), BKE_id_name(*b->id)) < 0;
  });

  for (bNode *group_node : group_nodes) {
    bNodeTree *group_tree = id_cast<bNodeTree *>(group_node->id);

    auto enter_group_fn = [tree, group_node, group_tree](bContext &C) {
      SpaceNode *snode = CTX_wm_space_node(&C);
      ARegion *region = CTX_wm_region(&C);

      while (snode->edittree != tree) {
        ED_node_tree_pop(region, snode);
      }
      bke::node_set_active(*tree, *group_node);
      ED_node_tree_push(region, snode, group_tree, group_node);
    };

    ui::Button *but = layout->button(
        BKE_id_name(*group_node->id), node_tree_icon(*group_node->id), enter_group_fn);
    if (group_tree == child_tree_in_path) {
      /* Highlight the current path item and it becomes the default item for Enter. */
      ui::button_flag_enable(but, ui::BUT_ACTIVE_DEFAULT);
    }
  }
}

static void context_path_add_top_level_shader_node_tree(const SpaceNode &snode,
                                                        Vector<ui::ContextPathItem> &path,
                                                        StructRNA &rna_type,
                                                        void *ptr,
                                                        bNodeTree *tree)
{
  const bool in_root_tree = snode.nodetree == snode.edittree;
  const bool has_group = in_root_tree ? node_tree_has_enterable_group(tree) : true;
  ui::context_path_add_generic(path,
                               rna_type,
                               ptr,
                               ICON_NONE,
                               in_root_tree ? nullptr : tree_path_handle_func(0),
                               has_group ? navigate_menu_create : nullptr,
                               tree);
}

static void context_path_add_node_tree_and_node_groups(const SpaceNode &snode,
                                                       Vector<ui::ContextPathItem> &path,
                                                       const bool skip_base = false)
{

  for (const auto [i, path_item] : snode.treepath.enumerate()) {
    if (skip_base && &path_item == snode.treepath.first_) {
      continue;
    }
    if (path_item.nodetree == nullptr) {
      continue;
    }

    const bool is_last_path = &path_item == snode.treepath.last();
    const bool has_group = is_last_path ? node_tree_has_enterable_group(path_item.nodetree) : true;
    ui::context_path_add_generic(path,
                                 *RNA_NodeTree,
                                 path_item.nodetree,
                                 node_tree_icon(path_item.nodetree->id),
                                 is_last_path ? nullptr : tree_path_handle_func(i),
                                 has_group ? navigate_menu_create : nullptr,
                                 path_item.nodetree);
  }
}

static void get_context_path_node_shader(const bContext &C,
                                         SpaceNode &snode,
                                         Vector<ui::ContextPathItem> &path)
{
  if (snode.flag & SNODE_PIN) {
    if (snode.shaderfrom == SNODE_SHADER_WORLD) {
      Scene *scene = CTX_data_scene(&C);
      ui::context_path_add_generic(path, *RNA_Scene, scene);
      if (scene != nullptr) {
        context_path_add_top_level_shader_node_tree(
            snode, path, *RNA_World, scene->world, snode.nodetree);
      }
      /* Skip the base node tree here, because the world contains a node tree already. */
      context_path_add_node_tree_and_node_groups(snode, path, true);
    }
    else {
      context_path_add_node_tree_and_node_groups(snode, path);
    }
  }
  else {
    Object *object = CTX_data_active_object(&C);
    if (snode.shaderfrom == SNODE_SHADER_OBJECT && object != nullptr) {
      ui::context_path_add_generic(path, *RNA_Object, object);
      if (!(object->matbits && object->matbits[object->actcol - 1])) {
        context_path_add_object_data(path, *object);
      }
      Material *material = BKE_object_material_get(object, object->actcol);
      context_path_add_top_level_shader_node_tree(
          snode, path, *RNA_Material, material, snode.nodetree);
    }
    else if (snode.shaderfrom == SNODE_SHADER_WORLD) {
      Scene *scene = CTX_data_scene(&C);
      ui::context_path_add_generic(path, *RNA_Scene, scene);
      if (scene != nullptr) {
        context_path_add_top_level_shader_node_tree(
            snode, path, *RNA_World, scene->world, snode.nodetree);
      }
    }
#ifdef WITH_FREESTYLE
    else if (snode.shaderfrom == SNODE_SHADER_LINESTYLE) {
      ViewLayer *viewlayer = CTX_data_view_layer(&C);
      FreestyleLineStyle *linestyle = BKE_linestyle_active_from_view_layer(viewlayer);
      ui::context_path_add_generic(path, *RNA_ViewLayer, viewlayer);
      Material *mat = BKE_object_material_get(object, object->actcol);
      ui::context_path_add_generic(path, *RNA_Material, mat);
    }
#endif
    context_path_add_node_tree_and_node_groups(snode, path, true);
  }
}

static void get_context_path_node_compositor(const bContext &C,
                                             SpaceNode &snode,
                                             Vector<ui::ContextPathItem> &path)
{
  if (snode.flag & SNODE_PIN) {
    context_path_add_node_tree_and_node_groups(snode, path);
    return;
  }

  if (snode.node_tree_sub_type == SNODE_COMPOSITOR_SEQUENCER) {
    bool skip_base = false;
    Scene *sequencer_scene = CTX_data_sequencer_scene(&C);
    if (sequencer_scene) {
      ui::context_path_add_generic(path, *RNA_Scene, sequencer_scene, ICON_SCENE);
      Strip *strip = seq::select_active_get(sequencer_scene);
      if (strip) {
        ui::context_path_add_generic(path, *RNA_Strip, strip, ICON_SEQ_STRIP);
        bNodeTree *node_group = nullptr;
        if (strip->type == STRIP_TYPE_COMPOSITOR && strip->effectdata) {
          CompositorEffectVars *comp_data = static_cast<CompositorEffectVars *>(strip->effectdata);
          node_group = comp_data->node_group;
        }
        else {
          StripModifierData *smd = seq::modifier_get_active(strip);
          if (smd && smd->type == eSeqModifierType_Compositor) {
            SequencerCompositorModifierData *scmd =
                reinterpret_cast<SequencerCompositorModifierData *>(smd);
            node_group = scmd->node_group;
          }
        }

        if (node_group != nullptr) {
          context_path_add_top_level_shader_node_tree(
              snode, path, *RNA_NodeTree, node_group, node_group);
          skip_base = true;
        }
      }
    }
    context_path_add_node_tree_and_node_groups(snode, path, skip_base);
    return;
  }

  Scene *scene = CTX_data_scene(&C);
  ui::context_path_add_generic(path, *RNA_Scene, scene);
  SceneCompositorEffect *effect = bke::compositor::get_active_effect(*scene);
  if (!effect) {
    context_path_add_node_tree_and_node_groups(snode, path);
    return;
  }

  ui::context_path_add_generic(path, *RNA_SceneCompositorEffect, effect, ICON_NODE_COMPOSITING);
  context_path_add_node_tree_and_node_groups(snode, path);
}

static void get_context_path_node_geometry(const bContext &C,
                                           SpaceNode &snode,
                                           Vector<ui::ContextPathItem> &path)
{
  if (snode.flag & SNODE_PIN || snode.node_tree_sub_type == SNODE_GEOMETRY_TOOL) {
    context_path_add_node_tree_and_node_groups(snode, path);
  }
  else {
    Object *object = CTX_data_active_object(&C);
    if (!object) {
      context_path_add_node_tree_and_node_groups(snode, path);
      return;
    }
    ui::context_path_add_generic(path, *RNA_Object, object);
    ModifierData *modifier = BKE_object_active_modifier(object);
    if (!modifier) {
      context_path_add_node_tree_and_node_groups(snode, path);
      return;
    }
    ui::context_path_add_generic(path, *RNA_Modifier, modifier, ICON_GEOMETRY_NODES);
    context_path_add_node_tree_and_node_groups(snode, path);
  }
}

Vector<ui::ContextPathItem> context_path_for_space_node(const bContext &C)
{
  SpaceNode *snode = CTX_wm_space_node(&C);
  if (snode == nullptr) {
    return {};
  }

  Vector<ui::ContextPathItem> context_path;

  if (ED_node_is_geometry(snode)) {
    get_context_path_node_geometry(C, *snode, context_path);
  }
  else if (ED_node_is_shader(snode)) {
    get_context_path_node_shader(C, *snode, context_path);
  }
  else if (ED_node_is_compositor(snode)) {
    get_context_path_node_compositor(C, *snode, context_path);
  }

  return context_path;
}

}  // namespace ed::space_node

}  // namespace blender
