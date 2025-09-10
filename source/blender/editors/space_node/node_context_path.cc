/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 * \brief Node breadcrumbs drawing
 */

#include "BLI_listbase.h"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "BKE_context.hh"
#include "BKE_material.hh"
#include "BKE_object.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_screen.hh"

#include "WM_api.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_intern.hh"

struct Material;

namespace blender::ed::space_node {

static void context_path_add_object_data(Vector<ui::ContextPathItem> &path, Object &object)
{
  if (!object.data) {
    return;
  }
  if (object.type == OB_MESH) {
    ui::context_path_add_generic(path, RNA_Mesh, object.data);
  }
  else if (object.type == OB_CURVES) {
    ui::context_path_add_generic(path, RNA_Curves, object.data);
  }
  else if (object.type == OB_LAMP) {
    ui::context_path_add_generic(path, RNA_Light, object.data);
  }
  else if (ELEM(object.type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    ui::context_path_add_generic(path, RNA_Curve, object.data);
  }
}

static std::function<void(bContext &)> tree_path_handle_func(int i)
{
  return [i](bContext &C) {
    PointerRNA op_props;
    wmOperatorType *ot = WM_operatortype_find("NODE_OT_tree_path_parent", false);
    WM_operator_properties_create_ptr(&op_props, ot);
    RNA_int_set(&op_props, "parent_tree_index", i);
    WM_operator_name_call_ptr(
        &C, ot, blender::wm::OpCallContext::InvokeDefault, &op_props, nullptr);
    WM_operator_properties_free(&op_props);
  };
}

static void context_path_add_top_level_shader_node_tree(const SpaceNode &snode,
                                                        Vector<ui::ContextPathItem> &path,
                                                        StructRNA &rna_type,
                                                        void *ptr)
{
  if (snode.nodetree != snode.edittree) {
    ui::context_path_add_generic(path, rna_type, ptr, ICON_NONE, tree_path_handle_func(0));
  }
  else {
    ui::context_path_add_generic(path, rna_type, ptr);
  }
}

static void context_path_add_node_tree_and_node_groups(const SpaceNode &snode,
                                                       Vector<ui::ContextPathItem> &path,
                                                       const bool skip_base = false)
{
  int i = 0;
  LISTBASE_FOREACH_INDEX (const bNodeTreePath *, path_item, &snode.treepath, i) {
    if (skip_base && path_item == snode.treepath.first) {
      continue;
    }
    if (path_item != snode.treepath.last) {
      // We don't need to add handle function to last nodetree
      ui::context_path_add_generic(
          path, RNA_NodeTree, path_item->nodetree, ICON_NODETREE, tree_path_handle_func(i));
    }
    else {
      ui::context_path_add_generic(path, RNA_NodeTree, path_item->nodetree, ICON_NODETREE);
    }
  }
}

static void get_context_path_node_shader(const bContext &C,
                                         SpaceNode &snode,
                                         Vector<ui::ContextPathItem> &path)
{
  if (snode.flag & SNODE_PIN) {
    if (snode.shaderfrom == SNODE_SHADER_WORLD) {
      Scene *scene = CTX_data_scene(&C);
      ui::context_path_add_generic(path, RNA_Scene, scene);
      if (scene != nullptr) {
        context_path_add_top_level_shader_node_tree(snode, path, RNA_World, scene->world);
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
      ui::context_path_add_generic(path, RNA_Object, object);
      if (!(object->matbits && object->matbits[object->actcol - 1])) {
        context_path_add_object_data(path, *object);
      }
      Material *material = BKE_object_material_get(object, object->actcol);
      context_path_add_top_level_shader_node_tree(snode, path, RNA_Material, material);
    }
    else if (snode.shaderfrom == SNODE_SHADER_WORLD) {
      Scene *scene = CTX_data_scene(&C);
      ui::context_path_add_generic(path, RNA_Scene, scene);
      if (scene != nullptr) {
        context_path_add_top_level_shader_node_tree(snode, path, RNA_World, scene->world);
      }
    }
#ifdef WITH_FREESTYLE
    else if (snode.shaderfrom == SNODE_SHADER_LINESTYLE) {
      ViewLayer *viewlayer = CTX_data_view_layer(&C);
      FreestyleLineStyle *linestyle = BKE_linestyle_active_from_view_layer(viewlayer);
      ui::context_path_add_generic(path, RNA_ViewLayer, viewlayer);
      Material *mat = BKE_object_material_get(object, object->actcol);
      ui::context_path_add_generic(path, RNA_Material, mat);
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
  }
  else {
    Scene *scene = CTX_data_scene(&C);
    ui::context_path_add_generic(path, RNA_Scene, scene);
    context_path_add_node_tree_and_node_groups(snode, path);
  }
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
    ui::context_path_add_generic(path, RNA_Object, object);
    ModifierData *modifier = BKE_object_active_modifier(object);
    ui::context_path_add_generic(path, RNA_Modifier, modifier, ICON_GEOMETRY_NODES);
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

  if (snode->edittree->type == NTREE_GEOMETRY) {
    get_context_path_node_geometry(C, *snode, context_path);
  }
  else if (snode->edittree->type == NTREE_SHADER) {
    get_context_path_node_shader(C, *snode, context_path);
  }
  else if (snode->edittree->type == NTREE_COMPOSIT) {
    get_context_path_node_compositor(C, *snode, context_path);
  }

  return context_path;
}

}  // namespace blender::ed::space_node
