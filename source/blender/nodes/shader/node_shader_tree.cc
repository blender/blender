/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include <cstring>

#include "DNA_light_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "BLI_array.hh"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_vector.h"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_linestyle.h"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_scene.hh"

#include "RNA_prototypes.hh"

#include "UI_resources.hh"

#include "NOD_shader.h"

#include "node_common.h"
#include "node_exec.hh"
#include "node_shader_util.hh"
#include "node_util.hh"

using blender::Array;
using blender::Vector;

static bool shader_tree_poll(const bContext *C, blender::bke::bNodeTreeType * /*treetype*/)
{
  Scene *scene = CTX_data_scene(C);
  const char *engine_id = scene->r.engine;

  /* Allow empty engine string too,
   * this is from older versions that didn't have registerable engines yet. */
  return (engine_id[0] == '\0' || STREQ(engine_id, RE_engine_id_CYCLES) ||
          !BKE_scene_use_shading_nodes_custom(scene));
}

static void shader_get_from_context(const bContext *C,
                                    blender::bke::bNodeTreeType * /*treetype*/,
                                    bNodeTree **r_ntree,
                                    ID **r_id,
                                    ID **r_from)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  if (snode->shaderfrom == SNODE_SHADER_OBJECT) {
    if (ob) {
      *r_from = &ob->id;
      if (ob->type == OB_LAMP) {
        *r_id = static_cast<ID *>(ob->data);
        *r_ntree = ((Light *)ob->data)->nodetree;
      }
      else {
        Material *ma = BKE_object_material_get(ob, ob->actcol);
        if (ma) {
          *r_id = &ma->id;
          *r_ntree = ma->nodetree;
        }
      }
    }
  }
#ifdef WITH_FREESTYLE
  else if (snode->shaderfrom == SNODE_SHADER_LINESTYLE) {
    FreestyleLineStyle *linestyle = BKE_linestyle_active_from_view_layer(view_layer);
    if (linestyle) {
      *r_from = nullptr;
      *r_id = &linestyle->id;
      *r_ntree = linestyle->nodetree;
    }
  }
#endif
  else { /* SNODE_SHADER_WORLD */
    if (scene->world) {
      *r_from = nullptr;
      *r_id = &scene->world->id;
      *r_ntree = scene->world->nodetree;
    }
  }
}

static void foreach_nodeclass(void *calldata, blender::bke::bNodeClassCallback func)
{
  func(calldata, NODE_CLASS_INPUT, N_("Input"));
  func(calldata, NODE_CLASS_OUTPUT, N_("Output"));
  func(calldata, NODE_CLASS_SHADER, N_("Shader"));
  func(calldata, NODE_CLASS_TEXTURE, N_("Texture"));
  func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
  func(calldata, NODE_CLASS_OP_VECTOR, N_("Vector"));
  func(calldata, NODE_CLASS_CONVERTER, N_("Converter"));
  func(calldata, NODE_CLASS_SCRIPT, N_("Script"));
  func(calldata, NODE_CLASS_GROUP, N_("Group"));
  func(calldata, NODE_CLASS_INTERFACE, N_("Interface"));
  func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

static void localize(bNodeTree *localtree, bNodeTree * /*ntree*/)
{
  /* replace muted nodes and reroute nodes by internal links */
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &localtree->nodes) {
    if (node->is_muted() || node->is_reroute()) {
      if (node->is_group() && node->id) {
        /* Free the group like in #ntree_shader_groups_flatten. */
        bNodeTree *group = reinterpret_cast<bNodeTree *>(node->id);
        blender::bke::node_tree_free_tree(*group);
        MEM_freeN(group);
        node->id = nullptr;
      }

      blender::bke::node_internal_relink(*localtree, *node);
      blender::bke::node_tree_free_local_node(*localtree, *node);
    }
  }
}

static void update(bNodeTree *ntree)
{
  blender::bke::node_tree_set_output(*ntree);

  ntree_update_reroute_nodes(ntree);
}

static bool shader_validate_link(eNodeSocketDatatype from, eNodeSocketDatatype to)
{
  /* Can't connect shader into other socket types, other way around is fine
   * since it will be interpreted as emission. */
  if (from == SOCK_SHADER) {
    return to == SOCK_SHADER;
  }
  if (ELEM(to, SOCK_BUNDLE, SOCK_CLOSURE, SOCK_MENU) ||
      ELEM(from, SOCK_BUNDLE, SOCK_CLOSURE, SOCK_MENU))
  {
    return from == to;
  }
  return true;
}

static bool shader_node_tree_socket_type_valid(blender::bke::bNodeTreeType * /*ntreetype*/,
                                               blender::bke::bNodeSocketType *socket_type)
{
  return blender::bke::node_is_static_socket_type(*socket_type) && ELEM(socket_type->type,
                                                                        SOCK_FLOAT,
                                                                        SOCK_INT,
                                                                        SOCK_BOOLEAN,
                                                                        SOCK_VECTOR,
                                                                        SOCK_RGBA,
                                                                        SOCK_SHADER,
                                                                        SOCK_BUNDLE,
                                                                        SOCK_CLOSURE,
                                                                        SOCK_MENU);
}

blender::bke::bNodeTreeType *ntreeType_Shader;

void register_node_tree_type_sh()
{
  blender::bke::bNodeTreeType *tt = ntreeType_Shader = MEM_new<blender::bke::bNodeTreeType>(
      __func__);

  tt->type = NTREE_SHADER;
  tt->idname = "ShaderNodeTree";
  tt->group_idname = "ShaderNodeGroup";
  tt->ui_name = N_("Shader Editor");
  tt->ui_icon = ICON_NODE_MATERIAL;
  tt->ui_description = N_("Edit materials, lights, and world shading using nodes");

  tt->foreach_nodeclass = foreach_nodeclass;
  tt->localize = localize;
  tt->update = update;
  tt->poll = shader_tree_poll;
  tt->get_from_context = shader_get_from_context;
  tt->validate_link = shader_validate_link;
  tt->valid_socket_type = shader_node_tree_socket_type_valid;

  tt->rna_ext.srna = &RNA_ShaderNodeTree;

  blender::bke::node_tree_type_add(*tt);
}

/* GPU material from shader nodes */

bNode *ntreeShaderOutputNode(bNodeTree *ntree, int target)
{
  /* Make sure we only have single node tagged as output. */
  blender::bke::node_tree_set_output(*ntree);

  /* Find output node that matches type and target. If there are
   * multiple, we prefer exact target match and active nodes. */
  bNode *output_node = nullptr;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (!ELEM(node->type_legacy,
              SH_NODE_OUTPUT_MATERIAL,
              SH_NODE_OUTPUT_WORLD,
              SH_NODE_OUTPUT_LIGHT))
    {
      continue;
    }

    if (node->custom1 == SHD_OUTPUT_ALL) {
      if (output_node == nullptr) {
        output_node = node;
      }
      else if (output_node->custom1 == SHD_OUTPUT_ALL) {
        if ((node->flag & NODE_DO_OUTPUT) && !(output_node->flag & NODE_DO_OUTPUT)) {
          output_node = node;
        }
      }
    }
    else if (node->custom1 == target) {
      if (output_node == nullptr) {
        output_node = node;
      }
      else if (output_node->custom1 == SHD_OUTPUT_ALL) {
        output_node = node;
      }
      else if ((node->flag & NODE_DO_OUTPUT) && !(output_node->flag & NODE_DO_OUTPUT)) {
        output_node = node;
      }
    }
  }

  return output_node;
}

/* Find socket with a specified identifier. */
static bNodeSocket *ntree_shader_node_find_socket(ListBase *sockets, const char *identifier)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, sockets) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return nullptr;
}

/* Find input socket with a specified identifier. */
static bNodeSocket *ntree_shader_node_find_input(bNode *node, const char *identifier)
{
  return ntree_shader_node_find_socket(&node->inputs, identifier);
}

/* Find output socket with a specified identifier. */
static bNodeSocket *ntree_shader_node_find_output(bNode *node, const char *identifier)
{
  return ntree_shader_node_find_socket(&node->outputs, identifier);
}

/* Find input socket at a specific position. */
static bNodeSocket *ntree_shader_node_input_get(bNode *node, int n)
{
  return reinterpret_cast<bNodeSocket *>(BLI_findlink(&node->inputs, n));
}

/* Find output socket at a specific position. */
static bNodeSocket *ntree_shader_node_output_get(bNode *node, int n)
{
  return reinterpret_cast<bNodeSocket *>(BLI_findlink(&node->outputs, n));
}

static void ntree_shader_unlink_script_nodes(bNodeTree *ntree)
{
  /* To avoid more trouble in the node tree processing (especially inside
   * `ntree_shader_weight_tree_invert()`) we disconnect the script node since they are not
   * supported in EEVEE (see #101702). */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if ((link->tonode->type_legacy == SH_NODE_SCRIPT) ||
        (link->fromnode->type_legacy == SH_NODE_SCRIPT))
    {
      blender::bke::node_remove_link(ntree, *link);
    }
  }
}

struct branchIterData {
  bool (*node_filter)(const bNode *node);
  int node_count;
};

static bool ntree_branch_count_and_tag_nodes(bNode *fromnode, bNode *tonode, void *userdata)
{
  branchIterData *iter = (branchIterData *)userdata;
  if (fromnode->runtime->tmp_flag == -1 &&
      (iter->node_filter == nullptr || iter->node_filter(fromnode)))
  {
    fromnode->runtime->tmp_flag = iter->node_count;
    iter->node_count++;
  }
  if (tonode->runtime->tmp_flag == -1 &&
      (iter->node_filter == nullptr || iter->node_filter(tonode)))
  {
    tonode->runtime->tmp_flag = iter->node_count;
    iter->node_count++;
  }
  return true;
}

/* Create a copy of a branch starting from a given node. */
static void ntree_shader_copy_branch(bNodeTree *ntree,
                                     bNode *start_node,
                                     bool (*node_filter)(const bNode *node))
{
  auto gather_branch_nodes = [](bNode *fromnode, bNode * /*tonode*/, void *userdata) {
    blender::Set<bNode *> *set = static_cast<blender::Set<bNode *> *>(userdata);
    set->add(fromnode);
    return true;
  };
  blender::Set<bNode *> branch_nodes = {start_node};
  blender::bke::node_chain_iterator_backwards(
      ntree, start_node, gather_branch_nodes, &branch_nodes, 0);

  /* Initialize `runtime->tmp_flag`. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->runtime->tmp_flag = -1;
  }
  /* Count and tag all nodes inside the displacement branch of the tree. */
  branchIterData iter_data;
  iter_data.node_filter = node_filter;
  iter_data.node_count = 0;
  blender::bke::node_chain_iterator_backwards(
      ntree, start_node, ntree_branch_count_and_tag_nodes, &iter_data, 1);
  /* Copies of the non-filtered nodes on the branch. */
  Array<bNode *> nodes_copy(iter_data.node_count);

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->runtime->tmp_flag >= 0) {
      int id = node->runtime->tmp_flag;
      /* Avoid creating unique names in the new tree, since it is very slow.
       * The names on the new nodes will be invalid. */
      blender::Map<const bNodeSocket *, bNodeSocket *> socket_map;
      nodes_copy[id] = blender::bke::node_copy_with_mapping(ntree,
                                                            *node,
                                                            LIB_ID_CREATE_NO_USER_REFCOUNT |
                                                                LIB_ID_CREATE_NO_MAIN,
                                                            std::nullopt,
                                                            std::nullopt,
                                                            socket_map,
                                                            true);

      bNode *copy = nodes_copy[id];
      copy->runtime->tmp_flag = -2; /* Copy */
      copy->runtime->original = node->runtime->original;
      /* Make sure to clear all sockets links as they are invalid. */
      LISTBASE_FOREACH (bNodeSocket *, sock, &copy->inputs) {
        sock->link = nullptr;
      }
      LISTBASE_FOREACH (bNodeSocket *, sock, &copy->outputs) {
        sock->link = nullptr;
      }
    }
  }

  /* Unlink the original nodes from this branch and link the copies. */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    bool from_copy = link->fromnode->runtime->tmp_flag >= 0;
    bool to_copy = link->tonode->runtime->tmp_flag >= 0;
    if (from_copy && to_copy) {
      bNode *from_node = nodes_copy[link->fromnode->runtime->tmp_flag];
      bNode *to_node = nodes_copy[link->tonode->runtime->tmp_flag];
      blender::bke::node_add_link(
          *ntree,
          *from_node,
          *ntree_shader_node_find_output(from_node, link->fromsock->identifier),
          *to_node,
          *ntree_shader_node_find_input(to_node, link->tosock->identifier));
    }
    else if (to_copy) {
      bNode *to_node = nodes_copy[link->tonode->runtime->tmp_flag];
      blender::bke::node_add_link(
          *ntree,
          *link->fromnode,
          *link->fromsock,
          *to_node,
          *ntree_shader_node_find_input(to_node, link->tosock->identifier));
    }
    else if (from_copy && branch_nodes.contains(link->tonode)) {
      bNode *from_node = nodes_copy[link->fromnode->runtime->tmp_flag];
      blender::bke::node_add_link(
          *ntree,
          *from_node,
          *ntree_shader_node_find_output(from_node, link->fromsock->identifier),
          *link->tonode,
          *link->tosock);
      blender::bke::node_remove_link(ntree, *link);
    }
  }
}

/* Generate emission node to convert regular data to closure sockets.
 * Returns validity of the tree.
 */
static bool ntree_shader_implicit_closure_cast(bNodeTree *ntree)
{
  bool modified = false;
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if ((link->fromsock->type != SOCK_SHADER) && (link->tosock->type == SOCK_SHADER)) {
      bNode *emission_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_EMISSION);
      bNodeSocket *in_sock = ntree_shader_node_find_input(emission_node, "Color");
      bNodeSocket *out_sock = ntree_shader_node_find_output(emission_node, "Emission");
      blender::bke::node_add_link(
          *ntree, *link->fromnode, *link->fromsock, *emission_node, *in_sock);
      blender::bke::node_add_link(*ntree, *emission_node, *out_sock, *link->tonode, *link->tosock);
      blender::bke::node_remove_link(ntree, *link);
      modified = true;
    }
    else if ((link->fromsock->type == SOCK_SHADER) && (link->tosock->type != SOCK_SHADER)) {
      blender::bke::node_remove_link(ntree, *link);
      BKE_ntree_update_without_main(*ntree);
      modified = true;
    }
  }
  if (modified) {
    BKE_ntree_update_without_main(*ntree);
  }
  return true;
}

/* Socket already has a link to it. Add weights together. */
static void ntree_weight_tree_merge_weight(bNodeTree *ntree,
                                           bNode * /*fromnode*/,
                                           bNodeSocket *fromsock,
                                           bNode **tonode,
                                           bNodeSocket **tosock)
{
  bNode *addnode = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
  addnode->custom1 = NODE_MATH_ADD;
  addnode->runtime->tmp_flag = -2; /* Copy */
  bNodeSocket *addsock_out = ntree_shader_node_output_get(addnode, 0);
  bNodeSocket *addsock_in0 = ntree_shader_node_input_get(addnode, 0);
  bNodeSocket *addsock_in1 = ntree_shader_node_input_get(addnode, 1);
  bNodeLink *oldlink = fromsock->link;
  blender::bke::node_add_link(
      *ntree, *oldlink->fromnode, *oldlink->fromsock, *addnode, *addsock_in0);
  blender::bke::node_add_link(*ntree, **tonode, **tosock, *addnode, *addsock_in1);
  blender::bke::node_remove_link(ntree, *oldlink);
  *tonode = addnode;
  *tosock = addsock_out;
}

static bool ntree_weight_tree_tag_nodes(bNode *fromnode, bNode *tonode, void *userdata)
{
  int *node_count = (int *)userdata;
  bool to_node_from_weight_tree = ELEM(tonode->type_legacy,
                                       SH_NODE_ADD_SHADER,
                                       SH_NODE_MIX_SHADER,
                                       SH_NODE_OUTPUT_WORLD,
                                       SH_NODE_OUTPUT_MATERIAL,
                                       SH_NODE_SHADERTORGB);
  if (tonode->runtime->tmp_flag == -1 && to_node_from_weight_tree) {
    tonode->runtime->tmp_flag = *node_count;
    *node_count += (tonode->type_legacy == SH_NODE_MIX_SHADER) ? 4 : 1;
  }
  if (fromnode->runtime->tmp_flag == -1 &&
      ELEM(fromnode->type_legacy, SH_NODE_ADD_SHADER, SH_NODE_MIX_SHADER))
  {
    fromnode->runtime->tmp_flag = *node_count;
    *node_count += (fromnode->type_legacy == SH_NODE_MIX_SHADER) ? 4 : 1;
  }
  return to_node_from_weight_tree;
}

/* Invert evaluation order of the weight tree (add & mix closure nodes) to feed the closure nodes
 * with their respective weights. */
static void ntree_shader_weight_tree_invert(bNodeTree *ntree, bNode *output_node)
{
  bNodeLink *displace_link = nullptr;
  bNodeSocket *displace_output = ntree_shader_node_find_input(output_node, "Displacement");
  if (displace_output && displace_output->link) {
    /* Remove any displacement link to avoid tagging it later on. */
    displace_link = displace_output->link;
    displace_output->link = nullptr;
  }
  bNodeLink *thickness_link = nullptr;
  bNodeSocket *thickness_output = ntree_shader_node_find_input(output_node, "Thickness");
  if (thickness_output && thickness_output->link) {
    /* Remove any thickness link to avoid tagging it later on. */
    thickness_link = thickness_output->link;
    thickness_output->link = nullptr;
  }
  /* Init tmp flag. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->runtime->tmp_flag = -1;
  }
  /* Tag nodes from the weight tree. Only tag output node and mix/add shader nodes. */
  output_node->runtime->tmp_flag = 0;
  int node_count = 1;
  blender::bke::node_chain_iterator_backwards(
      ntree, output_node, ntree_weight_tree_tag_nodes, &node_count, 0);
  /* Make a mirror copy of the weight tree. */
  Array<bNode *> nodes_copy(node_count);
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->runtime->tmp_flag >= 0) {
      int id = node->runtime->tmp_flag;

      switch (node->type_legacy) {
        case SH_NODE_SHADERTORGB:
        case SH_NODE_OUTPUT_LIGHT:
        case SH_NODE_OUTPUT_WORLD:
        case SH_NODE_OUTPUT_MATERIAL: {
          /* Start the tree with full weight. */
          nodes_copy[id] = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_VALUE);
          nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
          ((bNodeSocketValueFloat *)ntree_shader_node_output_get(nodes_copy[id], 0)->default_value)
              ->value = 1.0f;
          break;
        }
        case SH_NODE_ADD_SHADER: {
          /* Simple passthrough node. Each original inputs will get the same weight. */
          /* TODO(fclem): Better use some kind of reroute node? */
          nodes_copy[id] = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
          nodes_copy[id]->custom1 = NODE_MATH_ADD;
          nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
          ((bNodeSocketValueFloat *)ntree_shader_node_input_get(nodes_copy[id], 0)->default_value)
              ->value = 0.0f;
          break;
        }
        case SH_NODE_MIX_SHADER: {
          /* We need multiple nodes to emulate the mix node in reverse. */
          bNode *fromnode, *tonode;
          bNodeSocket *fromsock, *tosock;
          int id_start = id;
          /* output = (factor * input_weight) */
          nodes_copy[id] = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
          nodes_copy[id]->custom1 = NODE_MATH_MULTIPLY;
          nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
          id++;
          /* output = ((1.0 - factor) * input_weight) <=> (input_weight - factor * input_weight) */
          nodes_copy[id] = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
          nodes_copy[id]->custom1 = NODE_MATH_SUBTRACT;
          nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
          id++;
          /* Node sanitizes the input mix factor by clamping it. */
          nodes_copy[id] = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
          nodes_copy[id]->custom1 = NODE_MATH_ADD;
          nodes_copy[id]->custom2 = SHD_MATH_CLAMP;
          nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
          ((bNodeSocketValueFloat *)ntree_shader_node_input_get(nodes_copy[id], 0)->default_value)
              ->value = 0.0f;
          /* Copy default value if no link present. */
          bNodeSocket *fac_sock = ntree_shader_node_find_input(node, "Fac");
          if (!fac_sock->link) {
            float default_value = ((bNodeSocketValueFloat *)fac_sock->default_value)->value;
            bNodeSocket *dst_sock = ntree_shader_node_input_get(nodes_copy[id], 1);
            ((bNodeSocketValueFloat *)dst_sock->default_value)->value = default_value;
          }
          id++;
          /* Reroute the weight input to the 3 processing nodes. Simplify linking later-on. */
          /* TODO(fclem): Better use some kind of reroute node? */
          nodes_copy[id] = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
          nodes_copy[id]->custom1 = NODE_MATH_ADD;
          nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
          ((bNodeSocketValueFloat *)ntree_shader_node_input_get(nodes_copy[id], 0)->default_value)
              ->value = 0.0f;
          id++;
          /* Link between nodes for the subtraction. */
          fromnode = nodes_copy[id_start];
          tonode = nodes_copy[id_start + 1];
          fromsock = ntree_shader_node_output_get(fromnode, 0);
          tosock = ntree_shader_node_input_get(tonode, 1);
          blender::bke::node_add_link(*ntree, *fromnode, *fromsock, *tonode, *tosock);
          /* Link mix input to first node. */
          fromnode = nodes_copy[id_start + 2];
          tonode = nodes_copy[id_start];
          fromsock = ntree_shader_node_output_get(fromnode, 0);
          tosock = ntree_shader_node_input_get(tonode, 1);
          blender::bke::node_add_link(*ntree, *fromnode, *fromsock, *tonode, *tosock);
          /* Link weight input to both multiply nodes. */
          fromnode = nodes_copy[id_start + 3];
          fromsock = ntree_shader_node_output_get(fromnode, 0);
          tonode = nodes_copy[id_start];
          tosock = ntree_shader_node_input_get(tonode, 0);
          blender::bke::node_add_link(*ntree, *fromnode, *fromsock, *tonode, *tosock);
          tonode = nodes_copy[id_start + 1];
          tosock = ntree_shader_node_input_get(tonode, 0);
          blender::bke::node_add_link(*ntree, *fromnode, *fromsock, *tonode, *tosock);
          break;
        }
        default:
          BLI_assert(0);
          break;
      }
    }
  }
  /* Recreate links between copied nodes. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->runtime->tmp_flag >= 0) {
      /* Naming can be confusing here. We use original node-link name for from/to prefix.
       * The final link is in reversed order. */
      int socket_index;
      LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->inputs, socket_index) {
        bNodeSocket *tosock;
        bNode *tonode;

        switch (node->type_legacy) {
          case SH_NODE_SHADERTORGB:
          case SH_NODE_OUTPUT_LIGHT:
          case SH_NODE_OUTPUT_WORLD:
          case SH_NODE_OUTPUT_MATERIAL:
          case SH_NODE_ADD_SHADER: {
            tonode = nodes_copy[node->runtime->tmp_flag];
            tosock = ntree_shader_node_output_get(tonode, 0);
            break;
          }
          case SH_NODE_MIX_SHADER: {
            if (socket_index == 0) {
              /* Mix Factor. */
              tonode = nodes_copy[node->runtime->tmp_flag + 2];
              tosock = ntree_shader_node_input_get(tonode, 1);
            }
            else if (socket_index == 1) {
              /* Shader 1. */
              tonode = nodes_copy[node->runtime->tmp_flag + 1];
              tosock = ntree_shader_node_output_get(tonode, 0);
            }
            else {
              /* Shader 2. */
              tonode = nodes_copy[node->runtime->tmp_flag];
              tosock = ntree_shader_node_output_get(tonode, 0);
            }
            break;
          }
          default:
            BLI_assert(0);
            break;
        }

        if (sock->link) {
          bNodeSocket *fromsock;
          bNode *fromnode = sock->link->fromnode;

          switch (fromnode->type_legacy) {
            case SH_NODE_ADD_SHADER: {
              fromnode = nodes_copy[fromnode->runtime->tmp_flag];
              fromsock = ntree_shader_node_input_get(fromnode, 1);
              if (fromsock->link) {
                ntree_weight_tree_merge_weight(ntree, fromnode, fromsock, &tonode, &tosock);
              }
              break;
            }
            case SH_NODE_MIX_SHADER: {
              fromnode = nodes_copy[fromnode->runtime->tmp_flag + 3];
              fromsock = ntree_shader_node_input_get(fromnode, 1);
              if (fromsock->link) {
                ntree_weight_tree_merge_weight(ntree, fromnode, fromsock, &tonode, &tosock);
              }
              break;
            }
            case SH_NODE_BACKGROUND:
            case SH_NODE_BSDF_METALLIC:
            case SH_NODE_BSDF_DIFFUSE:
            case SH_NODE_BSDF_GLASS:
            case SH_NODE_BSDF_GLOSSY:
            case SH_NODE_BSDF_HAIR_PRINCIPLED:
            case SH_NODE_BSDF_HAIR:
            case SH_NODE_BSDF_PRINCIPLED:
            case SH_NODE_BSDF_RAY_PORTAL:
            case SH_NODE_BSDF_REFRACTION:
            case SH_NODE_BSDF_TOON:
            case SH_NODE_BSDF_TRANSLUCENT:
            case SH_NODE_BSDF_TRANSPARENT:
            case SH_NODE_BSDF_SHEEN:
            case SH_NODE_EEVEE_SPECULAR:
            case SH_NODE_EMISSION:
            case SH_NODE_HOLDOUT:
            case SH_NODE_SUBSURFACE_SCATTERING:
            case SH_NODE_VOLUME_ABSORPTION:
            case SH_NODE_VOLUME_PRINCIPLED:
            case SH_NODE_VOLUME_SCATTER:
            case SH_NODE_VOLUME_COEFFICIENTS:
              fromsock = ntree_shader_node_find_input(fromnode, "Weight");
              /* Make "weight" sockets available so that links to it are available as well and are
               * not ignored in other places. */
              fromsock->flag &= ~SOCK_UNAVAIL;
              if (fromsock->link) {
                ntree_weight_tree_merge_weight(ntree, fromnode, fromsock, &tonode, &tosock);
              }
              break;
            default:
              fromsock = sock->link->fromsock;
              break;
          }

          /* Manually add the link to the socket to avoid calling:
           * `BKE_ntree_update(G.main, oop)`. */
          fromsock->link = &blender::bke::node_add_link(
              *ntree, *fromnode, *fromsock, *tonode, *tosock);
          BLI_assert(fromsock->link);
        }
      }
    }
  }
  /* Restore displacement & thickness link. */
  if (displace_link) {
    blender::bke::node_add_link(*ntree,
                                *displace_link->fromnode,
                                *displace_link->fromsock,
                                *output_node,
                                *displace_output);
  }
  if (thickness_link) {
    blender::bke::node_add_link(*ntree,
                                *thickness_link->fromnode,
                                *thickness_link->fromsock,
                                *output_node,
                                *thickness_output);
  }
  BKE_ntree_update_without_main(*ntree);
}

static bool closure_node_filter(const bNode *node)
{
  switch (node->type_legacy) {
    case SH_NODE_ADD_SHADER:
    case SH_NODE_MIX_SHADER:
    case SH_NODE_BACKGROUND:
    case SH_NODE_BSDF_METALLIC:
    case SH_NODE_BSDF_DIFFUSE:
    case SH_NODE_BSDF_GLASS:
    case SH_NODE_BSDF_GLOSSY:
    case SH_NODE_BSDF_HAIR_PRINCIPLED:
    case SH_NODE_BSDF_HAIR:
    case SH_NODE_BSDF_PRINCIPLED:
    case SH_NODE_BSDF_RAY_PORTAL:
    case SH_NODE_BSDF_REFRACTION:
    case SH_NODE_BSDF_TOON:
    case SH_NODE_BSDF_TRANSLUCENT:
    case SH_NODE_BSDF_TRANSPARENT:
    case SH_NODE_BSDF_SHEEN:
    case SH_NODE_EEVEE_SPECULAR:
    case SH_NODE_EMISSION:
    case SH_NODE_HOLDOUT:
    case SH_NODE_SUBSURFACE_SCATTERING:
    case SH_NODE_VOLUME_ABSORPTION:
    case SH_NODE_VOLUME_PRINCIPLED:
    case SH_NODE_VOLUME_SCATTER:
    case SH_NODE_VOLUME_COEFFICIENTS:
      return true;
    default:
      return false;
  }
}

/* Shader to rgba needs their associated closure duplicated and the weight tree generated for. */
static void ntree_shader_shader_to_rgba_branches(bNodeTree *ntree)
{
  Vector<bNode *> shader_to_rgba_nodes;
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_SHADERTORGB) {
      shader_to_rgba_nodes.append(node);
    }
  }

  for (bNode *shader_to_rgba : shader_to_rgba_nodes) {
    bNodeSocket *closure_input = ntree_shader_node_input_get(shader_to_rgba, 0);
    if (closure_input->link == nullptr) {
      continue;
    }
    ntree_shader_copy_branch(ntree, shader_to_rgba, closure_node_filter);
    BKE_ntree_update_without_main(*ntree);

    ntree_shader_weight_tree_invert(ntree, shader_to_rgba);
  }
}

static void iter_shader_to_rgba_depth_count(bNodeTree *ntree,
                                            bNode *node_start,
                                            int16_t &max_depth)
{
  struct StackNode {
    bNode *node;
    int16_t depth;
  };

  blender::Stack<StackNode> stack;
  blender::Stack<StackNode> zone_stack;
  stack.push({node_start, 0});

  while (!stack.is_empty() || !zone_stack.is_empty()) {
    StackNode s_node = !stack.is_empty() ? stack.pop() : zone_stack.pop();

    bNode *node = s_node.node;
    int16_t depth_level = s_node.depth;

    if (node->runtime->tmp_flag >= depth_level) {
      /* We already iterated this branch at this or a greater depth. */
      continue;
    }

    if (node->type_legacy == SH_NODE_SHADERTORGB) {
      depth_level++;
      max_depth = std::max(max_depth, depth_level);
    }

    node->runtime->tmp_flag = std::max(node->runtime->tmp_flag, depth_level);

    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      bNodeLink *link = sock->link;
      if (link == nullptr) {
        continue;
      }
      if ((link->flag & NODE_LINK_VALID) == 0) {
        /* Skip links marked as cyclic. */
        continue;
      }
      stack.push({link->fromnode, depth_level});
    }

    /* Zone input nodes are linked to their corresponding zone output nodes, even if there is no
     * bNodeLink between them. */
    if (const blender::bke::bNodeZoneType *zone_type = blender::bke::zone_type_by_node_type(
            node->type_legacy))
    {
      if (zone_type->output_type == node->type_legacy) {
        if (bNode *zone_input_node = zone_type->get_corresponding_input(*ntree, *node)) {
          zone_stack.push({zone_input_node, depth_level});
        }
      }
    }
  }
}

static void shader_node_disconnect_input(bNodeTree *ntree, bNode *node, int index)
{
  bNodeLink *link = ntree_shader_node_input_get(node, index)->link;
  if (link) {
    blender::bke::node_remove_link(ntree, *link);
  }
}

static void shader_node_disconnect_inactive_mix_branch(bNodeTree *ntree,
                                                       bNode *node,
                                                       int factor_socket_index,
                                                       int a_socket_index,
                                                       int b_socket_index,
                                                       bool clamp_factor)
{
  bNodeSocket *factor_socket = ntree_shader_node_input_get(node, factor_socket_index);
  if (factor_socket->link == nullptr) {
    float factor = 0.5;

    if (factor_socket->type == SOCK_FLOAT) {
      factor = factor_socket->default_value_typed<bNodeSocketValueFloat>()->value;
      if (clamp_factor) {
        factor = clamp_f(factor, 0.0f, 1.0f);
      }
    }
    else if (factor_socket->type == SOCK_VECTOR) {
      const float *vfactor = factor_socket->default_value_typed<bNodeSocketValueVector>()->value;
      float vfactor_copy[3];
      for (int i = 0; i < 3; i++) {
        if (clamp_factor) {
          vfactor_copy[i] = clamp_f(vfactor[i], 0.0f, 1.0f);
        }
        else {
          vfactor_copy[i] = vfactor[i];
        }
      }
      if (vfactor_copy[0] == vfactor_copy[1] && vfactor_copy[0] == vfactor_copy[2]) {
        factor = vfactor_copy[0];
      }
    }

    if (factor == 1.0f && a_socket_index >= 0) {
      shader_node_disconnect_input(ntree, node, a_socket_index);
    }
    else if (factor == 0.0f && b_socket_index >= 0) {
      shader_node_disconnect_input(ntree, node, b_socket_index);
    }
  }
}

static void ntree_shader_disconnect_inactive_mix_branches(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->typeinfo->type_legacy == SH_NODE_MIX_SHADER) {
      shader_node_disconnect_inactive_mix_branch(ntree, node, 0, 1, 2, true);
    }
    else if (node->typeinfo->type_legacy == SH_NODE_MIX) {
      const NodeShaderMix *storage = static_cast<NodeShaderMix *>(node->storage);
      if (storage->data_type == SOCK_FLOAT) {
        shader_node_disconnect_inactive_mix_branch(ntree, node, 0, 2, 3, storage->clamp_factor);
        /* Disconnect links from data_type-specific sockets that are not currently in use */
        for (int i : {1, 4, 5, 6, 7}) {
          shader_node_disconnect_input(ntree, node, i);
        }
      }
      else if (storage->data_type == SOCK_VECTOR) {
        int factor_socket = storage->factor_mode == NODE_MIX_MODE_UNIFORM ? 0 : 1;
        shader_node_disconnect_inactive_mix_branch(
            ntree, node, factor_socket, 4, 5, storage->clamp_factor);
        /* Disconnect links from data_type-specific sockets that are not currently in use */
        int unused_factor_socket = factor_socket == 0 ? 1 : 0;
        for (int i : {unused_factor_socket, 2, 3, 6, 7}) {
          shader_node_disconnect_input(ntree, node, i);
        }
      }
      else if (storage->data_type == SOCK_RGBA) {
        /* Branch A can't be optimized-out, since its alpha is always used regardless of factor */
        shader_node_disconnect_inactive_mix_branch(ntree, node, 0, -1, 7, storage->clamp_factor);
        /* Disconnect links from data_type-specific sockets that are not currently in use */
        for (int i : {1, 2, 3, 4, 5}) {
          shader_node_disconnect_input(ntree, node, i);
        }
      }
    }
  }
}

static bool ntree_branch_node_tag(bNode *fromnode, bNode *tonode, void * /*userdata*/)
{
  fromnode->runtime->tmp_flag = 1;
  tonode->runtime->tmp_flag = 1;
  return true;
}

/* Avoid adding more node execution when multiple outputs are present. */
/* NOTE(@fclem): This is also a workaround for the old EEVEE SSS implementation where only the
 * first executed SSS node gets a SSS profile. */
static void ntree_shader_pruned_unused(bNodeTree *ntree, bNode *output_node)
{
  ntree_shader_disconnect_inactive_mix_branches(ntree);

  bool changed = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->runtime->tmp_flag = 0;
  }

  /* Avoid deleting the output node if it is the only node in the tree. */
  if (output_node) {
    output_node->runtime->tmp_flag = 1;
    blender::bke::node_chain_iterator_backwards(
        ntree, output_node, ntree_branch_node_tag, nullptr, 0);
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_OUTPUT_AOV) {
      node->runtime->tmp_flag = 1;
      blender::bke::node_chain_iterator_backwards(ntree, node, ntree_branch_node_tag, nullptr, 0);
    }
  }

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    if (node->runtime->tmp_flag == 0) {
      blender::bke::node_unlink_node(*ntree, *node);
      blender::bke::node_free_node(ntree, *node);
      changed = true;
    }
  }

  if (changed) {
    BKE_ntree_update_without_main(*ntree);
  }
}

void ntreeGPUMaterialNodes(bNodeTree *localtree, GPUMaterial *mat)
{
  bNodeTreeExec *exec;

  ntree_shader_unlink_script_nodes(localtree);
  bNode *output = ntreeShaderOutputNode(localtree, SHD_OUTPUT_EEVEE);

  /* Tree is valid if it contains no undefined implicit socket type cast. */
  bool valid_tree = ntree_shader_implicit_closure_cast(localtree);

  if (valid_tree) {
    ntree_shader_pruned_unused(localtree, output);
    if (output != nullptr) {
      ntree_shader_shader_to_rgba_branches(localtree);
      ntree_shader_weight_tree_invert(localtree, output);
    }
  }

  exec = ntreeShaderBeginExecTree(localtree);
  /* Execute nodes ordered by the number of ShaderToRGB nodes found in their path,
   * so all closures can be properly evaluated. */
  int16_t max_depth = 0;
  LISTBASE_FOREACH (bNode *, node, &localtree->nodes) {
    node->runtime->tmp_flag = -1;
  }
  if (output != nullptr) {
    iter_shader_to_rgba_depth_count(localtree, output, max_depth);
  }
  LISTBASE_FOREACH (bNode *, node, &localtree->nodes) {
    if (node->type_legacy == SH_NODE_OUTPUT_AOV) {
      iter_shader_to_rgba_depth_count(localtree, node, max_depth);
    }
  }
  for (int depth = max_depth; depth >= 0; depth--) {
    ntreeExecGPUNodes(exec, mat, output, &depth);
    LISTBASE_FOREACH (bNode *, node, &localtree->nodes) {
      if (node->type_legacy == SH_NODE_OUTPUT_AOV) {
        ntreeExecGPUNodes(exec, mat, node, &depth);
      }
    }
  }
  ntreeShaderEndExecTree(exec);
}

bNodeTreeExec *ntreeShaderBeginExecTree_internal(bNodeExecContext *context,
                                                 bNodeTree *ntree,
                                                 bNodeInstanceKey parent_key)
{
  /* ensures only a single output node is enabled */
  blender::bke::node_tree_set_output(*ntree);

  /* common base initialization */
  bNodeTreeExec *exec = ntree_exec_begin(context, ntree, parent_key);

  /* allocate the thread stack listbase array */
  exec->threadstack = MEM_calloc_arrayN<ListBase>(BLENDER_MAX_THREADS, "thread stack array");

  LISTBASE_FOREACH (bNode *, node, &exec->nodetree->nodes) {
    node->runtime->need_exec = 1;
  }

  return exec;
}

bNodeTreeExec *ntreeShaderBeginExecTree(bNodeTree *ntree)
{
  bNodeExecContext context;
  bNodeTreeExec *exec;

  /* XXX hack: prevent exec data from being generated twice.
   * this should be handled by the renderer!
   */
  if (ntree->runtime->execdata) {
    return ntree->runtime->execdata;
  }

  exec = ntreeShaderBeginExecTree_internal(&context, ntree, blender::bke::NODE_INSTANCE_KEY_BASE);

  /* XXX: this should not be necessary, but is still used for compositor/shader/texture nodes,
   * which only store the `ntree` pointer. Should be fixed at some point!
   */
  ntree->runtime->execdata = exec;

  return exec;
}

void ntreeShaderEndExecTree_internal(bNodeTreeExec *exec)
{
  if (exec->threadstack) {
    for (int a = 0; a < BLENDER_MAX_THREADS; a++) {
      LISTBASE_FOREACH (bNodeThreadStack *, nts, &exec->threadstack[a]) {
        if (nts->stack) {
          MEM_freeN(nts->stack);
        }
      }
      BLI_freelistN(&exec->threadstack[a]);
    }

    MEM_freeN(exec->threadstack);
    exec->threadstack = nullptr;
  }

  ntree_exec_end(exec);
}

void ntreeShaderEndExecTree(bNodeTreeExec *exec)
{
  if (exec) {
    /* exec may get freed, so assign ntree */
    bNodeTree *ntree = exec->nodetree;
    ntreeShaderEndExecTree_internal(exec);

    /* XXX: clear node-tree back-pointer to exec data,
     * same problem as noted in #ntreeBeginExecTree. */
    ntree->runtime->execdata = nullptr;
  }
}
