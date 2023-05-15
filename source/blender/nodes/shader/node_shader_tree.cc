/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation */

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
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "BLI_array.hh"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_linestyle.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_scene.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "GPU_material.h"

#include "RE_texture.h"

#include "UI_resources.h"

#include "NOD_common.h"

#include "node_common.h"
#include "node_exec.hh"
#include "node_shader_util.hh"
#include "node_util.hh"

using blender::Array;
using blender::Vector;

static bool shader_tree_poll(const bContext *C, bNodeTreeType * /*treetype*/)
{
  Scene *scene = CTX_data_scene(C);
  const char *engine_id = scene->r.engine;

  /* Allow empty engine string too,
   * this is from older versions that didn't have registerable engines yet. */
  return (engine_id[0] == '\0' || STREQ(engine_id, RE_engine_id_CYCLES) ||
          !BKE_scene_use_shading_nodes_custom(scene));
}

static void shader_get_from_context(
    const bContext *C, bNodeTreeType * /*treetype*/, bNodeTree **r_ntree, ID **r_id, ID **r_from)
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

static void foreach_nodeclass(Scene * /*scene*/, void *calldata, bNodeClassCallback func)
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
    if (node->flag & NODE_MUTED || node->type == NODE_REROUTE) {
      blender::bke::nodeInternalRelink(localtree, node);
      blender::bke::ntreeFreeLocalNode(localtree, node);
    }
  }
}

static void update(bNodeTree *ntree)
{
  ntreeSetOutput(ntree);

  ntree_update_reroute_nodes(ntree);
}

static bool shader_validate_link(eNodeSocketDatatype from, eNodeSocketDatatype to)
{
  /* Can't connect shader into other socket types, other way around is fine
   * since it will be interpreted as emission. */
  if (from == SOCK_SHADER) {
    return to == SOCK_SHADER;
  }
  return true;
}

static bool shader_node_tree_socket_type_valid(bNodeTreeType * /*ntreetype*/,
                                               bNodeSocketType *socket_type)
{
  return blender::bke::nodeIsStaticSocketType(socket_type) &&
         ELEM(socket_type->type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA, SOCK_SHADER);
}

bNodeTreeType *ntreeType_Shader;

void register_node_tree_type_sh()
{
  bNodeTreeType *tt = ntreeType_Shader = MEM_cnew<bNodeTreeType>("shader node tree type");

  tt->type = NTREE_SHADER;
  strcpy(tt->idname, "ShaderNodeTree");
  strcpy(tt->group_idname, "ShaderNodeGroup");
  strcpy(tt->ui_name, N_("Shader Editor"));
  tt->ui_icon = ICON_NODE_MATERIAL;
  strcpy(tt->ui_description, N_("Shader nodes"));

  tt->foreach_nodeclass = foreach_nodeclass;
  tt->localize = localize;
  tt->update = update;
  tt->poll = shader_tree_poll;
  tt->get_from_context = shader_get_from_context;
  tt->validate_link = shader_validate_link;
  tt->valid_socket_type = shader_node_tree_socket_type_valid;

  tt->rna_ext.srna = &RNA_ShaderNodeTree;

  ntreeTypeAdd(tt);
}

/* GPU material from shader nodes */

bNode *ntreeShaderOutputNode(bNodeTree *ntree, int target)
{
  /* Make sure we only have single node tagged as output. */
  ntreeSetOutput(ntree);

  /* Find output node that matches type and target. If there are
   * multiple, we prefer exact target match and active nodes. */
  bNode *output_node = nullptr;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (!ELEM(node->type, SH_NODE_OUTPUT_MATERIAL, SH_NODE_OUTPUT_WORLD, SH_NODE_OUTPUT_LIGHT)) {
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

/* Return true on success. */
static bool ntree_shader_expand_socket_default(bNodeTree *localtree,
                                               bNode *node,
                                               bNodeSocket *socket)
{
  bNode *value_node;
  bNodeSocket *value_socket;
  bNodeSocketValueVector *src_vector;
  bNodeSocketValueRGBA *src_rgba, *dst_rgba;
  bNodeSocketValueFloat *src_float, *dst_float;
  bNodeSocketValueInt *src_int;

  switch (socket->type) {
    case SOCK_VECTOR:
      value_node = nodeAddStaticNode(nullptr, localtree, SH_NODE_RGB);
      value_socket = ntree_shader_node_find_output(value_node, "Color");
      BLI_assert(value_socket != nullptr);
      src_vector = static_cast<bNodeSocketValueVector *>(socket->default_value);
      dst_rgba = static_cast<bNodeSocketValueRGBA *>(value_socket->default_value);
      copy_v3_v3(dst_rgba->value, src_vector->value);
      dst_rgba->value[3] = 1.0f; /* should never be read */
      break;
    case SOCK_RGBA:
      value_node = nodeAddStaticNode(nullptr, localtree, SH_NODE_RGB);
      value_socket = ntree_shader_node_find_output(value_node, "Color");
      BLI_assert(value_socket != nullptr);
      src_rgba = static_cast<bNodeSocketValueRGBA *>(socket->default_value);
      dst_rgba = static_cast<bNodeSocketValueRGBA *>(value_socket->default_value);
      copy_v4_v4(dst_rgba->value, src_rgba->value);
      break;
    case SOCK_INT:
      /* HACK: Support as float. */
      value_node = nodeAddStaticNode(nullptr, localtree, SH_NODE_VALUE);
      value_socket = ntree_shader_node_find_output(value_node, "Value");
      BLI_assert(value_socket != nullptr);
      src_int = static_cast<bNodeSocketValueInt *>(socket->default_value);
      dst_float = static_cast<bNodeSocketValueFloat *>(value_socket->default_value);
      dst_float->value = float(src_int->value);
      break;
    case SOCK_FLOAT:
      value_node = nodeAddStaticNode(nullptr, localtree, SH_NODE_VALUE);
      value_socket = ntree_shader_node_find_output(value_node, "Value");
      BLI_assert(value_socket != nullptr);
      src_float = static_cast<bNodeSocketValueFloat *>(socket->default_value);
      dst_float = static_cast<bNodeSocketValueFloat *>(value_socket->default_value);
      dst_float->value = src_float->value;
      break;
    default:
      return false;
  }
  nodeAddLink(localtree, value_node, value_socket, node, socket);
  return true;
}

static void ntree_shader_unlink_hidden_value_sockets(bNode *group_node, bNodeSocket *isock)
{
  bNodeTree *group_ntree = (bNodeTree *)group_node->id;
  bool removed_link = false;

  LISTBASE_FOREACH (bNode *, node, &group_ntree->nodes) {
    const bool is_group = ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && (node->id != nullptr);

    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      if (!is_group && (sock->flag & SOCK_HIDE_VALUE) == 0) {
        continue;
      }
      /* If socket is linked to a group input node and sockets id match. */
      if (sock && sock->link && sock->link->fromnode->type == NODE_GROUP_INPUT) {
        if (STREQ(isock->identifier, sock->link->fromsock->identifier)) {
          if (is_group) {
            /* Recursively unlink sockets within the nested group. */
            ntree_shader_unlink_hidden_value_sockets(node, sock);
          }
          else {
            nodeRemLink(group_ntree, sock->link);
            removed_link = true;
          }
        }
      }
    }
  }

  if (removed_link) {
    BKE_ntree_update_main_tree(G.main, group_ntree, nullptr);
  }
}

/* Node groups once expanded looses their input sockets values.
 * To fix this, link value/rgba nodes into the sockets and copy the group sockets values. */
static void ntree_shader_groups_expand_inputs(bNodeTree *localtree)
{
  bool link_added = false;

  LISTBASE_FOREACH (bNode *, node, &localtree->nodes) {
    const bool is_group = ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && (node->id != nullptr);
    const bool is_group_output = node->type == NODE_GROUP_OUTPUT && (node->flag & NODE_DO_OUTPUT);

    if (is_group) {
      /* Do it recursively. */
      ntree_shader_groups_expand_inputs((bNodeTree *)node->id);
    }

    if (is_group || is_group_output) {
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        if (socket->link != nullptr && !(socket->link->flag & NODE_LINK_MUTED)) {
          bNodeLink *link = socket->link;
          /* Fix the case where the socket is actually converting the data. (see #71374)
           * We only do the case of lossy conversion to float. */
          if ((socket->type == SOCK_FLOAT) && (link->fromsock->type != link->tosock->type)) {
            if (link->fromsock->type == SOCK_RGBA) {
              bNode *tmp = nodeAddStaticNode(nullptr, localtree, SH_NODE_RGBTOBW);
              nodeAddLink(localtree,
                          link->fromnode,
                          link->fromsock,
                          tmp,
                          static_cast<bNodeSocket *>(tmp->inputs.first));
              nodeAddLink(
                  localtree, tmp, static_cast<bNodeSocket *>(tmp->outputs.first), node, socket);
            }
            else if (link->fromsock->type == SOCK_VECTOR) {
              bNode *tmp = nodeAddStaticNode(nullptr, localtree, SH_NODE_VECTOR_MATH);
              tmp->custom1 = NODE_VECTOR_MATH_DOT_PRODUCT;
              bNodeSocket *dot_input1 = static_cast<bNodeSocket *>(tmp->inputs.first);
              bNodeSocket *dot_input2 = static_cast<bNodeSocket *>(dot_input1->next);
              bNodeSocketValueVector *input2_socket_value = static_cast<bNodeSocketValueVector *>(
                  dot_input2->default_value);
              copy_v3_fl(input2_socket_value->value, 1.0f / 3.0f);
              nodeAddLink(localtree, link->fromnode, link->fromsock, tmp, dot_input1);
              nodeAddLink(
                  localtree, tmp, static_cast<bNodeSocket *>(tmp->outputs.last), node, socket);
            }
          }
          continue;
        }

        if (is_group) {
          /* Detect the case where an input is plugged into a hidden value socket.
           * In this case we should just remove the link to trigger the socket default override. */
          ntree_shader_unlink_hidden_value_sockets(node, socket);
        }

        if (ntree_shader_expand_socket_default(localtree, node, socket)) {
          link_added = true;
        }
      }
    }
  }

  if (link_added) {
    BKE_ntree_update_main_tree(G.main, localtree, nullptr);
  }
}

static void ntree_shader_groups_remove_muted_links(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == NODE_GROUP) {
      if (node->id != nullptr) {
        ntree_shader_groups_remove_muted_links(reinterpret_cast<bNodeTree *>(node->id));
      }
    }
  }
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->flag & NODE_LINK_MUTED) {
      nodeRemLink(ntree, link);
    }
  }
}

static void flatten_group_do(bNodeTree *ntree, bNode *gnode)
{
  LinkNode *group_interface_nodes = nullptr;
  bNodeTree *ngroup = (bNodeTree *)gnode->id;

  /* Add the nodes into the ntree */
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ngroup->nodes) {
    /* Remove interface nodes.
     * This also removes remaining links to and from interface nodes.
     * We must delay removal since sockets will reference this node. see: #52092 */
    if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
      BLI_linklist_prepend(&group_interface_nodes, node);
    }
    /* migrate node */
    BLI_remlink(&ngroup->nodes, node);
    BLI_addtail(&ntree->nodes, node);
    nodeUniqueID(ntree, node);
    /* ensure unique node name in the node tree */
    /* This is very slow and it has no use for GPU nodetree. (see #70609) */
    // nodeUniqueName(ntree, node);
  }
  ngroup->runtime->nodes_by_id.clear();

  /* Save first and last link to iterate over flattened group links. */
  bNodeLink *glinks_first = static_cast<bNodeLink *>(ntree->links.last);

  /* Add internal links to the ntree */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ngroup->links) {
    BLI_remlink(&ngroup->links, link);
    BLI_addtail(&ntree->links, link);
  }

  bNodeLink *glinks_last = static_cast<bNodeLink *>(ntree->links.last);

  /* restore external links to and from the gnode */
  if (glinks_first != nullptr) {
    /* input links */
    for (bNodeLink *link = glinks_first->next; link != glinks_last->next; link = link->next) {
      if (link->fromnode->type == NODE_GROUP_INPUT) {
        const char *identifier = link->fromsock->identifier;
        /* find external links to this input */
        for (bNodeLink *tlink = static_cast<bNodeLink *>(ntree->links.first);
             tlink != glinks_first->next;
             tlink = tlink->next)
        {
          if (tlink->tonode == gnode && STREQ(tlink->tosock->identifier, identifier)) {
            nodeAddLink(ntree, tlink->fromnode, tlink->fromsock, link->tonode, link->tosock);
          }
        }
      }
    }
    /* Also iterate over the new links to cover passthrough links. */
    glinks_last = static_cast<bNodeLink *>(ntree->links.last);
    /* output links */
    for (bNodeLink *tlink = static_cast<bNodeLink *>(ntree->links.first);
         tlink != glinks_first->next;
         tlink = tlink->next)
    {
      if (tlink->fromnode == gnode) {
        const char *identifier = tlink->fromsock->identifier;
        /* find internal links to this output */
        for (bNodeLink *link = glinks_first->next; link != glinks_last->next; link = link->next) {
          /* only use active output node */
          if (link->tonode->type == NODE_GROUP_OUTPUT && (link->tonode->flag & NODE_DO_OUTPUT)) {
            if (STREQ(link->tosock->identifier, identifier)) {
              nodeAddLink(ntree, link->fromnode, link->fromsock, tlink->tonode, tlink->tosock);
            }
          }
        }
      }
    }
  }

  while (group_interface_nodes) {
    bNode *node = static_cast<bNode *>(BLI_linklist_pop(&group_interface_nodes));
    blender::bke::ntreeFreeLocalNode(ntree, node);
  }

  BKE_ntree_update_tag_all(ntree);
}

/* Flatten group to only have a simple single tree */
static void ntree_shader_groups_flatten(bNodeTree *localtree)
{
  /* This is effectively recursive as the flattened groups will add
   * nodes at the end of the list, which will also get evaluated. */
  for (bNode *node = static_cast<bNode *>(localtree->nodes.first), *node_next; node;
       node = node_next)
  {
    if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && node->id != nullptr) {
      flatten_group_do(localtree, node);
      /* Continue even on new flattened nodes. */
      node_next = node->next;
      /* delete the group instance and its localtree. */
      bNodeTree *ngroup = (bNodeTree *)node->id;
      blender::bke::ntreeFreeLocalNode(localtree, node);
      blender::bke::ntreeFreeTree(ngroup);
      BLI_assert(!ngroup->id.py_instance); /* Or call #BKE_libblock_free_data_py. */
      MEM_freeN(ngroup);
    }
    else {
      node_next = node->next;
    }
  }

  BKE_ntree_update_main_tree(G.main, localtree, nullptr);
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
      (iter->node_filter == nullptr || iter->node_filter(tonode))) {
    tonode->runtime->tmp_flag = iter->node_count;
    iter->node_count++;
  }
  return true;
}

/* Create a copy of a branch starting from a given node.
 * callback is executed once for every copied node.
 * Returns input node copy. */
static bNode *ntree_shader_copy_branch(bNodeTree *ntree,
                                       bNode *start_node,
                                       bool (*node_filter)(const bNode *node),
                                       void (*callback)(bNode *node, int user_data),
                                       int user_data)
{
  /* Initialize `runtime->tmp_flag`. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->runtime->tmp_flag = -1;
  }
  /* Count and tag all nodes inside the displacement branch of the tree. */
  start_node->runtime->tmp_flag = 0;
  branchIterData iter_data;
  iter_data.node_filter = node_filter;
  iter_data.node_count = 1;
  blender::bke::nodeChainIterBackwards(ntree, start_node, ntree_branch_count_and_tag_nodes, &iter_data, 1);
  /* Make a full copy of the branch */
  Array<bNode *> nodes_copy(iter_data.node_count);
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->runtime->tmp_flag >= 0) {
      int id = node->runtime->tmp_flag;
      /* Avoid creating unique names in the new tree, since it is very slow. The names on the new
       * nodes will be invalid. But identifiers must be created for the `bNodeTree::all_nodes()`
       * vector, though they won't match the original. */
      nodes_copy[id] = blender::bke::node_copy(
          ntree, *node, LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_MAIN, false);
      nodeUniqueID(ntree, nodes_copy[id]);

      nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
      nodes_copy[id]->runtime->original = node->runtime->original;
      /* Make sure to clear all sockets links as they are invalid. */
      LISTBASE_FOREACH (bNodeSocket *, sock, &nodes_copy[id]->inputs) {
        sock->link = nullptr;
      }
      LISTBASE_FOREACH (bNodeSocket *, sock, &nodes_copy[id]->outputs) {
        sock->link = nullptr;
      }
    }
  }
  /* Recreate links between copied nodes AND incoming links to the copied nodes. */
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->tonode->runtime->tmp_flag >= 0) {
      bool from_node_copied = link->fromnode->runtime->tmp_flag >= 0;
      bNode *fromnode = from_node_copied ? nodes_copy[link->fromnode->runtime->tmp_flag] :
                                           link->fromnode;
      bNode *tonode = nodes_copy[link->tonode->runtime->tmp_flag];
      bNodeSocket *fromsock = ntree_shader_node_find_output(fromnode, link->fromsock->identifier);
      bNodeSocket *tosock = ntree_shader_node_find_input(tonode, link->tosock->identifier);
      nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);
    }
  }
  /* Per node callback. */
  if (callback) {
    for (int i = 0; i < iter_data.node_count; i++) {
      callback(nodes_copy[i], user_data);
    }
  }
  bNode *start_node_copy = nodes_copy[start_node->runtime->tmp_flag];
  return start_node_copy;
}

/* Generate emission node to convert regular data to closure sockets.
 * Returns validity of the tree.
 */
static bool ntree_shader_implicit_closure_cast(bNodeTree *ntree)
{
  bool modified = false;
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if ((link->fromsock->type != SOCK_SHADER) && (link->tosock->type == SOCK_SHADER)) {
      bNode *emission_node = nodeAddStaticNode(nullptr, ntree, SH_NODE_EMISSION);
      bNodeSocket *in_sock = ntree_shader_node_find_input(emission_node, "Color");
      bNodeSocket *out_sock = ntree_shader_node_find_output(emission_node, "Emission");
      nodeAddLink(ntree, link->fromnode, link->fromsock, emission_node, in_sock);
      nodeAddLink(ntree, emission_node, out_sock, link->tonode, link->tosock);
      nodeRemLink(ntree, link);
      modified = true;
    }
    else if ((link->fromsock->type == SOCK_SHADER) && (link->tosock->type != SOCK_SHADER)) {
      /* Meh. Not directly visible to the user. But better than nothing. */
      fprintf(stderr, "Shader Nodetree Error: Invalid implicit socket conversion\n");
      BKE_ntree_update_main_tree(G.main, ntree, nullptr);
      return false;
    }
  }
  if (modified) {
    BKE_ntree_update_main_tree(G.main, ntree, nullptr);
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
  bNode *addnode = nodeAddStaticNode(nullptr, ntree, SH_NODE_MATH);
  addnode->custom1 = NODE_MATH_ADD;
  addnode->runtime->tmp_flag = -2; /* Copy */
  bNodeSocket *addsock_out = ntree_shader_node_output_get(addnode, 0);
  bNodeSocket *addsock_in0 = ntree_shader_node_input_get(addnode, 0);
  bNodeSocket *addsock_in1 = ntree_shader_node_input_get(addnode, 1);
  bNodeLink *oldlink = fromsock->link;
  nodeAddLink(ntree, oldlink->fromnode, oldlink->fromsock, addnode, addsock_in0);
  nodeAddLink(ntree, *tonode, *tosock, addnode, addsock_in1);
  nodeRemLink(ntree, oldlink);
  *tonode = addnode;
  *tosock = addsock_out;
}

static bool ntree_weight_tree_tag_nodes(bNode *fromnode, bNode *tonode, void *userdata)
{
  int *node_count = (int *)userdata;
  bool to_node_from_weight_tree = ELEM(tonode->type,
                                       SH_NODE_ADD_SHADER,
                                       SH_NODE_MIX_SHADER,
                                       SH_NODE_OUTPUT_WORLD,
                                       SH_NODE_OUTPUT_MATERIAL,
                                       SH_NODE_SHADERTORGB);
  if (tonode->runtime->tmp_flag == -1 && to_node_from_weight_tree) {
    tonode->runtime->tmp_flag = *node_count;
    *node_count += (tonode->type == SH_NODE_MIX_SHADER) ? 4 : 1;
  }
  if (fromnode->runtime->tmp_flag == -1 &&
      ELEM(fromnode->type, SH_NODE_ADD_SHADER, SH_NODE_MIX_SHADER))
  {
    fromnode->runtime->tmp_flag = *node_count;
    *node_count += (fromnode->type == SH_NODE_MIX_SHADER) ? 4 : 1;
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
  blender::bke::nodeChainIterBackwards(ntree, output_node, ntree_weight_tree_tag_nodes, &node_count, 0);
  /* Make a mirror copy of the weight tree. */
  Array<bNode *> nodes_copy(node_count);
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->runtime->tmp_flag >= 0) {
      int id = node->runtime->tmp_flag;

      switch (node->type) {
        case SH_NODE_SHADERTORGB:
        case SH_NODE_OUTPUT_LIGHT:
        case SH_NODE_OUTPUT_WORLD:
        case SH_NODE_OUTPUT_MATERIAL: {
          /* Start the tree with full weight. */
          nodes_copy[id] = nodeAddStaticNode(nullptr, ntree, SH_NODE_VALUE);
          nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
          ((bNodeSocketValueFloat *)ntree_shader_node_output_get(nodes_copy[id], 0)->default_value)
              ->value = 1.0f;
          break;
        }
        case SH_NODE_ADD_SHADER: {
          /* Simple passthrough node. Each original inputs will get the same weight. */
          /* TODO(fclem): Better use some kind of reroute node? */
          nodes_copy[id] = nodeAddStaticNode(nullptr, ntree, SH_NODE_MATH);
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
          nodes_copy[id] = nodeAddStaticNode(nullptr, ntree, SH_NODE_MATH);
          nodes_copy[id]->custom1 = NODE_MATH_MULTIPLY;
          nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
          id++;
          /* output = ((1.0 - factor) * input_weight) <=> (input_weight - factor * input_weight) */
          nodes_copy[id] = nodeAddStaticNode(nullptr, ntree, SH_NODE_MATH);
          nodes_copy[id]->custom1 = NODE_MATH_SUBTRACT;
          nodes_copy[id]->runtime->tmp_flag = -2; /* Copy */
          id++;
          /* Node sanitizes the input mix factor by clamping it. */
          nodes_copy[id] = nodeAddStaticNode(nullptr, ntree, SH_NODE_MATH);
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
          nodes_copy[id] = nodeAddStaticNode(nullptr, ntree, SH_NODE_MATH);
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
          nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);
          /* Link mix input to first node. */
          fromnode = nodes_copy[id_start + 2];
          tonode = nodes_copy[id_start];
          fromsock = ntree_shader_node_output_get(fromnode, 0);
          tosock = ntree_shader_node_input_get(tonode, 1);
          nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);
          /* Link weight input to both multiply nodes. */
          fromnode = nodes_copy[id_start + 3];
          fromsock = ntree_shader_node_output_get(fromnode, 0);
          tonode = nodes_copy[id_start];
          tosock = ntree_shader_node_input_get(tonode, 0);
          nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);
          tonode = nodes_copy[id_start + 1];
          tosock = ntree_shader_node_input_get(tonode, 0);
          nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);
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
      /* Naming can be confusing here. We use original nodelink name for from/to prefix.
       * The final link is in reversed order. */
      int socket_index;
      LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->inputs, socket_index) {
        bNodeSocket *tosock;
        bNode *tonode;

        switch (node->type) {
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

          switch (fromnode->type) {
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
            case SH_NODE_BSDF_ANISOTROPIC:
            case SH_NODE_BSDF_DIFFUSE:
            case SH_NODE_BSDF_GLASS:
            case SH_NODE_BSDF_GLOSSY:
            case SH_NODE_BSDF_HAIR_PRINCIPLED:
            case SH_NODE_BSDF_HAIR:
            case SH_NODE_BSDF_PRINCIPLED:
            case SH_NODE_BSDF_REFRACTION:
            case SH_NODE_BSDF_TOON:
            case SH_NODE_BSDF_TRANSLUCENT:
            case SH_NODE_BSDF_TRANSPARENT:
            case SH_NODE_BSDF_VELVET:
            case SH_NODE_EEVEE_SPECULAR:
            case SH_NODE_EMISSION:
            case SH_NODE_HOLDOUT:
            case SH_NODE_SUBSURFACE_SCATTERING:
            case SH_NODE_VOLUME_ABSORPTION:
            case SH_NODE_VOLUME_PRINCIPLED:
            case SH_NODE_VOLUME_SCATTER:
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
           * `BKE_ntree_update_main_tree(G.main, oop, nullptr)`. */
          fromsock->link = nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);
          BLI_assert(fromsock->link);
        }
      }
    }
  }
  /* Restore displacement & thickness link. */
  if (displace_link) {
    nodeAddLink(
        ntree, displace_link->fromnode, displace_link->fromsock, output_node, displace_output);
  }
  if (thickness_link) {
    nodeAddLink(
        ntree, thickness_link->fromnode, thickness_link->fromsock, output_node, thickness_output);
  }
  BKE_ntree_update_main_tree(G.main, ntree, nullptr);
}

static bool closure_node_filter(const bNode *node)
{
  switch (node->type) {
    case SH_NODE_ADD_SHADER:
    case SH_NODE_MIX_SHADER:
    case SH_NODE_BACKGROUND:
    case SH_NODE_BSDF_ANISOTROPIC:
    case SH_NODE_BSDF_DIFFUSE:
    case SH_NODE_BSDF_GLASS:
    case SH_NODE_BSDF_GLOSSY:
    case SH_NODE_BSDF_HAIR_PRINCIPLED:
    case SH_NODE_BSDF_HAIR:
    case SH_NODE_BSDF_PRINCIPLED:
    case SH_NODE_BSDF_REFRACTION:
    case SH_NODE_BSDF_TOON:
    case SH_NODE_BSDF_TRANSLUCENT:
    case SH_NODE_BSDF_TRANSPARENT:
    case SH_NODE_BSDF_VELVET:
    case SH_NODE_EEVEE_SPECULAR:
    case SH_NODE_EMISSION:
    case SH_NODE_HOLDOUT:
    case SH_NODE_SUBSURFACE_SCATTERING:
    case SH_NODE_VOLUME_ABSORPTION:
    case SH_NODE_VOLUME_PRINCIPLED:
    case SH_NODE_VOLUME_SCATTER:
      return true;
    default:
      return false;
  }
}

static bool shader_to_rgba_node_gather(bNode * /*fromnode*/, bNode *tonode, void *userdata)
{
  Vector<bNode *> &shader_to_rgba_nodes = *(Vector<bNode *> *)userdata;
  if (tonode->runtime->tmp_flag == -1 && tonode->type == SH_NODE_SHADERTORGB) {
    tonode->runtime->tmp_flag = 0;
    shader_to_rgba_nodes.append(tonode);
  }
  return true;
}

/* Shader to rgba needs their associated closure duplicated and the weight tree generated for. */
static void ntree_shader_shader_to_rgba_branch(bNodeTree *ntree, bNode *output_node)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->runtime->tmp_flag = -1;
  }
  /* First gather the shader_to_rgba nodes linked to the output. This is separate to avoid
   * conflicting usage of the `node->runtime->tmp_flag`. */
  Vector<bNode *> shader_to_rgba_nodes;
  blender::bke::nodeChainIterBackwards(ntree, output_node, shader_to_rgba_node_gather, &shader_to_rgba_nodes, 0);

  for (bNode *shader_to_rgba : shader_to_rgba_nodes) {
    bNodeSocket *closure_input = ntree_shader_node_input_get(shader_to_rgba, 0);
    if (closure_input->link == nullptr) {
      continue;
    }
    bNode *start_node = closure_input->link->fromnode;
    bNode *start_node_copy = ntree_shader_copy_branch(
        ntree, start_node, closure_node_filter, nullptr, 0);
    /* Replace node copy link. This assumes that every node possibly connected to the closure input
     * has only one output. */
    bNodeSocket *closure_output = ntree_shader_node_output_get(start_node_copy, 0);
    nodeRemLink(ntree, closure_input->link);
    nodeAddLink(ntree, start_node_copy, closure_output, shader_to_rgba, closure_input);
    BKE_ntree_update_main_tree(G.main, ntree, nullptr);

    ntree_shader_weight_tree_invert(ntree, shader_to_rgba);
  }
}

static void shader_node_disconnect_input(bNodeTree *ntree, bNode *node, int index)
{
  bNodeLink *link = ntree_shader_node_input_get(node, index)->link;
  if (link) {
    nodeRemLink(ntree, link);
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
    if (node->typeinfo->type == SH_NODE_MIX_SHADER) {
      shader_node_disconnect_inactive_mix_branch(ntree, node, 0, 1, 2, true);
    }
    else if (node->typeinfo->type == SH_NODE_MIX) {
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
  output_node->runtime->tmp_flag = 1;

  blender::bke::nodeChainIterBackwards(ntree, output_node, ntree_branch_node_tag, nullptr, 0);

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == SH_NODE_OUTPUT_AOV) {
      node->runtime->tmp_flag = 1;
      blender::bke::nodeChainIterBackwards(ntree, node, ntree_branch_node_tag, nullptr, 0);
    }
  }

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    if (node->runtime->tmp_flag == 0) {
      blender::bke::ntreeFreeLocalNode(ntree, node);
      changed = true;
    }
  }

  if (changed) {
    BKE_ntree_update_main_tree(G.main, ntree, nullptr);
  }
}

void ntreeGPUMaterialNodes(bNodeTree *localtree, GPUMaterial *mat)
{
  bNodeTreeExec *exec;

  ntree_shader_groups_remove_muted_links(localtree);
  ntree_shader_groups_expand_inputs(localtree);
  ntree_shader_groups_flatten(localtree);

  bNode *output = ntreeShaderOutputNode(localtree, SHD_OUTPUT_EEVEE);

  /* Tree is valid if it contains no undefined implicit socket type cast. */
  bool valid_tree = ntree_shader_implicit_closure_cast(localtree);

  if (valid_tree && output != nullptr) {
    ntree_shader_pruned_unused(localtree, output);
    ntree_shader_shader_to_rgba_branch(localtree, output);
    ntree_shader_weight_tree_invert(localtree, output);
  }

  exec = ntreeShaderBeginExecTree(localtree);
  ntreeExecGPUNodes(exec, mat, output);
  LISTBASE_FOREACH (bNode *, node, &localtree->nodes) {
    if (node->type == SH_NODE_OUTPUT_AOV) {
      ntreeExecGPUNodes(exec, mat, node);
    }
  }
  ntreeShaderEndExecTree(exec);
}

bNodeTreeExec *ntreeShaderBeginExecTree_internal(bNodeExecContext *context,
                                                 bNodeTree *ntree,
                                                 bNodeInstanceKey parent_key)
{
  /* ensures only a single output node is enabled */
  ntreeSetOutput(ntree);

  /* common base initialization */
  bNodeTreeExec *exec = ntree_exec_begin(context, ntree, parent_key);

  /* allocate the thread stack listbase array */
  exec->threadstack = static_cast<ListBase *>(
      MEM_callocN(BLENDER_MAX_THREADS * sizeof(ListBase), "thread stack array"));

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

  context.previews = ntree->previews;

  exec = ntreeShaderBeginExecTree_internal(&context, ntree, NODE_INSTANCE_KEY_BASE);

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
