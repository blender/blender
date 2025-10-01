/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "NOD_geometry.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_node.hh"
#include "BKE_object.hh"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_space_types.h"

#include "RNA_prototypes.hh"

#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "node_common.h"

blender::bke::bNodeTreeType *ntreeType_Geometry;

static void geometry_node_tree_get_from_context(const bContext *C,
                                                blender::bke::bNodeTreeType * /*treetype*/,
                                                bNodeTree **r_ntree,
                                                ID **r_id,
                                                ID **r_from)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode->node_tree_sub_type == SNODE_GEOMETRY_TOOL) {
    if (snode->selected_node_group && snode->selected_node_group->type == NTREE_GEOMETRY) {
      *r_ntree = snode->selected_node_group;
      return;
    }
    *r_ntree = nullptr;
    return;
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  if (ob == nullptr) {
    return;
  }

  const ModifierData *md = BKE_object_active_modifier(ob);

  if (md == nullptr) {
    return;
  }

  if (md->type == eModifierType_Nodes) {
    const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
    if (nmd->node_group != nullptr) {
      *r_from = &ob->id;
      *r_id = &ob->id;
      *r_ntree = nmd->node_group;
    }
  }
}

static void geometry_node_tree_update(bNodeTree *ntree)
{
  blender::bke::node_tree_set_output(*ntree);

  /* Needed to give correct types to reroutes. */
  ntree_update_reroute_nodes(ntree);
}

static void foreach_nodeclass(void *calldata, blender::bke::bNodeClassCallback func)
{
  func(calldata, NODE_CLASS_INPUT, N_("Input"));
  func(calldata, NODE_CLASS_GEOMETRY, N_("Geometry"));
  func(calldata, NODE_CLASS_ATTRIBUTE, N_("Attribute"));
  func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
  func(calldata, NODE_CLASS_OP_VECTOR, N_("Vector"));
  func(calldata, NODE_CLASS_CONVERTER, N_("Converter"));
  func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

static bool geometry_node_tree_validate_link(eNodeSocketDatatype type_a,
                                             eNodeSocketDatatype type_b)
{
  /* Geometry, string, object, material, texture and collection sockets can only be connected to
   * themselves. The other types can be converted between each other. */
  if (ELEM(type_a, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA, SOCK_BOOLEAN, SOCK_INT) &&
      ELEM(type_b, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA, SOCK_BOOLEAN, SOCK_INT))
  {
    return true;
  }
  if (ELEM(type_a, SOCK_FLOAT, SOCK_VECTOR) && type_b == SOCK_ROTATION) {
    /* Floats and vectors implicitly convert to rotations. */
    return true;
  }

  /* Support implicit conversions between matrices and rotations. */
  if (type_a == SOCK_MATRIX && type_b == SOCK_ROTATION) {
    return true;
  }
  if (type_a == SOCK_ROTATION && type_b == SOCK_MATRIX) {
    return true;
  }

  if (type_a == SOCK_ROTATION && type_b == SOCK_VECTOR) {
    /* Rotations implicitly convert to vectors. */
    return true;
  }
  return type_a == type_b;
}

static bool geometry_node_tree_socket_type_valid(blender::bke::bNodeTreeType * /*treetype*/,
                                                 blender::bke::bNodeSocketType *socket_type)
{
  return blender::bke::node_is_static_socket_type(*socket_type) &&
         (ELEM(socket_type->type,
               SOCK_FLOAT,
               SOCK_VECTOR,
               SOCK_RGBA,
               SOCK_BOOLEAN,
               SOCK_ROTATION,
               SOCK_MATRIX,
               SOCK_INT,
               SOCK_STRING,
               SOCK_OBJECT,
               SOCK_GEOMETRY,
               SOCK_COLLECTION,
               SOCK_IMAGE,
               SOCK_MATERIAL,
               SOCK_MENU) ||
          ELEM(socket_type->type, SOCK_BUNDLE, SOCK_CLOSURE));
}

void register_node_tree_type_geo()
{
  blender::bke::bNodeTreeType *tt = ntreeType_Geometry = MEM_new<blender::bke::bNodeTreeType>(
      __func__);
  tt->type = NTREE_GEOMETRY;
  tt->idname = "GeometryNodeTree";
  tt->group_idname = "GeometryNodeGroup";
  tt->ui_name = N_("Geometry Node Editor");
  tt->ui_icon = ICON_GEOMETRY_NODES;
  tt->ui_description = N_("Advanced geometry editing and tools creation using nodes");
  tt->rna_ext.srna = &RNA_GeometryNodeTree;
  tt->update = geometry_node_tree_update;
  tt->get_from_context = geometry_node_tree_get_from_context;
  tt->foreach_nodeclass = foreach_nodeclass;
  tt->valid_socket_type = geometry_node_tree_socket_type_valid;
  tt->validate_link = geometry_node_tree_validate_link;

  blender::bke::node_tree_type_add(*tt);
}

bool is_layer_selection_field(const bNodeTreeInterfaceSocket &socket)
{
  const blender::bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
  BLI_assert(typeinfo != nullptr);

  if (typeinfo->type != SOCK_BOOLEAN) {
    return false;
  }
  return (socket.flag & NODE_INTERFACE_SOCKET_LAYER_SELECTION) != 0;
}
