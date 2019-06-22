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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup nodes
 */

#include <string.h>

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_workspace_types.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_alloca.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_linestyle.h"
#include "BKE_node.h"
#include "BKE_scene.h"

#include "RNA_access.h"

#include "GPU_material.h"

#include "RE_shader_ext.h"

#include "NOD_common.h"

#include "node_common.h"
#include "node_exec.h"
#include "node_util.h"
#include "node_shader_util.h"

typedef struct nTreeTags {
  float ssr_id, sss_id;
} nTreeTags;

static void ntree_shader_tag_nodes(bNodeTree *ntree, bNode *output_node, nTreeTags *tags);

static bool shader_tree_poll(const bContext *C, bNodeTreeType *UNUSED(treetype))
{
  Scene *scene = CTX_data_scene(C);
  const char *engine_id = scene->r.engine;

  /* Allow empty engine string too,
   * this is from older versions that didn't have registerable engines yet. */
  return (engine_id[0] == '\0' || STREQ(engine_id, RE_engine_id_CYCLES) ||
          !BKE_scene_use_shading_nodes_custom(scene));
}

static void shader_get_from_context(const bContext *C,
                                    bNodeTreeType *UNUSED(treetype),
                                    bNodeTree **r_ntree,
                                    ID **r_id,
                                    ID **r_from)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);

  if (snode->shaderfrom == SNODE_SHADER_OBJECT) {
    if (ob) {
      *r_from = &ob->id;
      if (ob->type == OB_LAMP) {
        *r_id = ob->data;
        *r_ntree = ((Light *)ob->data)->nodetree;
      }
      else {
        Material *ma = give_current_material(ob, ob->actcol);
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
      *r_from = NULL;
      *r_id = &linestyle->id;
      *r_ntree = linestyle->nodetree;
    }
  }
#endif
  else { /* SNODE_SHADER_WORLD */
    if (scene->world) {
      *r_from = NULL;
      *r_id = &scene->world->id;
      *r_ntree = scene->world->nodetree;
    }
  }
}

static void foreach_nodeclass(Scene *UNUSED(scene), void *calldata, bNodeClassCallback func)
{
  func(calldata, NODE_CLASS_INPUT, N_("Input"));
  func(calldata, NODE_CLASS_OUTPUT, N_("Output"));
  func(calldata, NODE_CLASS_SHADER, N_("Shader"));
  func(calldata, NODE_CLASS_TEXTURE, N_("Texture"));
  func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
  func(calldata, NODE_CLASS_OP_VECTOR, N_("Vector"));
  func(calldata, NODE_CLASS_CONVERTOR, N_("Convertor"));
  func(calldata, NODE_CLASS_SCRIPT, N_("Script"));
  func(calldata, NODE_CLASS_GROUP, N_("Group"));
  func(calldata, NODE_CLASS_INTERFACE, N_("Interface"));
  func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

static void localize(bNodeTree *localtree, bNodeTree *UNUSED(ntree))
{
  bNode *node, *node_next;

  /* replace muted nodes and reroute nodes by internal links */
  for (node = localtree->nodes.first; node; node = node_next) {
    node_next = node->next;

    if (node->flag & NODE_MUTED || node->type == NODE_REROUTE) {
      nodeInternalRelink(localtree, node);
      ntreeFreeLocalNode(localtree, node);
    }
  }
}

static void local_sync(bNodeTree *localtree, bNodeTree *ntree)
{
  BKE_node_preview_sync_tree(ntree, localtree);
}

static void local_merge(Main *UNUSED(bmain), bNodeTree *localtree, bNodeTree *ntree)
{
  BKE_node_preview_merge_tree(ntree, localtree, true);
}

static void update(bNodeTree *ntree)
{
  ntreeSetOutput(ntree);

  ntree_update_reroute_nodes(ntree);

  if (ntree->update & NTREE_UPDATE_NODES) {
    /* clean up preview cache, in case nodes have been removed */
    BKE_node_preview_remove_unused(ntree);
  }
}

static bool shader_validate_link(bNodeTree *UNUSED(ntree), bNodeLink *link)
{
  /* Can't connect shader into other socket types, other way around is fine
   * since it will be interpreted as emission. */
  if (link->fromsock->type == SOCK_SHADER) {
    return (link->tosock->type == SOCK_SHADER);
  }
  return true;
}

bNodeTreeType *ntreeType_Shader;

void register_node_tree_type_sh(void)
{
  bNodeTreeType *tt = ntreeType_Shader = MEM_callocN(sizeof(bNodeTreeType),
                                                     "shader node tree type");

  tt->type = NTREE_SHADER;
  strcpy(tt->idname, "ShaderNodeTree");
  strcpy(tt->ui_name, N_("Shader Editor"));
  tt->ui_icon = 0; /* defined in drawnode.c */
  strcpy(tt->ui_description, N_("Shader nodes"));

  tt->foreach_nodeclass = foreach_nodeclass;
  tt->localize = localize;
  tt->local_sync = local_sync;
  tt->local_merge = local_merge;
  tt->update = update;
  tt->poll = shader_tree_poll;
  tt->get_from_context = shader_get_from_context;
  tt->validate_link = shader_validate_link;

  tt->ext.srna = &RNA_ShaderNodeTree;

  ntreeTypeAdd(tt);
}

/* GPU material from shader nodes */

static void ntree_shader_link_builtin_normal(bNodeTree *ntree,
                                             bNode *node_from,
                                             bNodeSocket *socket_from,
                                             bNode *displacement_node,
                                             bNodeSocket *displacement_socket);

static bNodeSocket *ntree_shader_node_find_input(bNode *node, const char *identifier);

static bNode *ntree_group_output_node(bNodeTree *ntree);

static bNode *ntree_shader_relink_output_from_group(bNodeTree *ntree,
                                                    bNode *group_node,
                                                    bNode *sh_output_node,
                                                    int target)
{
  int i;
  bNodeTree *group_ntree = (bNodeTree *)group_node->id;

  int sock_len = BLI_listbase_count(&sh_output_node->inputs);
  bNodeSocket **group_surface_sockets = BLI_array_alloca(group_surface_sockets, sock_len);

  /* Create output sockets to plug output connection to. */
  i = 0;
  for (bNodeSocket *sock = sh_output_node->inputs.first; sock; sock = sock->next, ++i) {
    group_surface_sockets[i] = ntreeAddSocketInterface(
        group_ntree, SOCK_OUT, sock->typeinfo->idname, sock->name);
  }

  bNode *group_output_node = ntree_group_output_node(group_ntree);

  /* If no group output node is present, we need to create one. */
  if (group_output_node == NULL) {
    group_output_node = nodeAddStaticNode(NULL, group_ntree, NODE_GROUP_OUTPUT);
  }

  /* Need to update tree so all node instances nodes gets proper sockets. */
  node_group_update(ntree, group_node);
  node_group_output_update(group_ntree, group_output_node);
  ntreeUpdateTree(G.main, group_ntree);

  /* Remove other shader output nodes so that only the new one can be selected as active. */
  for (bNode *node = ntree->nodes.first; node; node = node->next) {
    if (ELEM(node->type, SH_NODE_OUTPUT_MATERIAL, SH_NODE_OUTPUT_WORLD, SH_NODE_OUTPUT_LIGHT)) {
      ntreeFreeLocalNode(ntree, node);
    }
  }

  /* Create new shader output node outside the group. */
  bNode *new_output_node = nodeAddStaticNode(NULL, ntree, sh_output_node->type);
  new_output_node->custom1 = target;

  i = 0;
  for (bNodeSocket *sock = sh_output_node->inputs.first; sock; sock = sock->next, ++i) {
    if (sock->link != NULL) {
      /* Link the shader output node incoming link to the group output sockets */
      bNodeSocket *group_output_node_surface_input_sock = nodeFindSocket(
          group_output_node, SOCK_IN, group_surface_sockets[i]->identifier);
      nodeAddLink(group_ntree,
                  sock->link->fromnode,
                  sock->link->fromsock,
                  group_output_node,
                  group_output_node_surface_input_sock);

      /* Link the group output sockets to the new shader output node. */
      bNodeSocket *group_node_surface_output = nodeFindSocket(
          group_node, SOCK_OUT, group_surface_sockets[i]->identifier);
      bNodeSocket *output_node_surface_input = ntree_shader_node_find_input(new_output_node,
                                                                            sock->name);

      nodeAddLink(ntree,
                  group_node,
                  group_node_surface_output,
                  new_output_node,
                  output_node_surface_input);
    }
  }

  ntreeUpdateTree(G.main, group_ntree);
  ntreeUpdateTree(G.main, ntree);

  return new_output_node;
}

static bNode *ntree_shader_output_node_from_group(bNodeTree *ntree, int target)
{
  bNode *output_node = NULL;

  /* Search if node groups do not contain valid output nodes (recursively). */
  for (bNode *node = ntree->nodes.first; node; node = node->next) {
    if (!ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) {
      continue;
    }
    if (node->id != NULL) {
      output_node = ntree_shader_output_node_from_group((bNodeTree *)node->id, target);

      if (output_node == NULL) {
        output_node = ntreeShaderOutputNode((bNodeTree *)node->id, target);
      }

      if (output_node != NULL) {
        /* Output is inside this group node. Create relink to make the output outside the group. */
        output_node = ntree_shader_relink_output_from_group(ntree, node, output_node, target);
        break;
      }
    }
  }
  return output_node;
}

/* Find an output node of the shader tree.
 *
 * NOTE: it will only return output which is NOT in the group, which isn't how
 * render engines works but it's how the GPU shader compilation works. This we
 * can change in the future and make it a generic function, but for now it stays
 * private here.
 */
bNode *ntreeShaderOutputNode(bNodeTree *ntree, int target)
{
  /* Make sure we only have single node tagged as output. */
  ntreeSetOutput(ntree);

  /* Find output node that matches type and target. If there are
   * multiple, we prefer exact target match and active nodes. */
  bNode *output_node = NULL;

  for (bNode *node = ntree->nodes.first; node; node = node->next) {
    if (!ELEM(node->type, SH_NODE_OUTPUT_MATERIAL, SH_NODE_OUTPUT_WORLD, SH_NODE_OUTPUT_LIGHT)) {
      continue;
    }

    if (node->custom1 == SHD_OUTPUT_ALL) {
      if (output_node == NULL) {
        output_node = node;
      }
      else if (output_node->custom1 == SHD_OUTPUT_ALL) {
        if ((node->flag & NODE_DO_OUTPUT) && !(output_node->flag & NODE_DO_OUTPUT)) {
          output_node = node;
        }
      }
    }
    else if (node->custom1 == target) {
      if (output_node == NULL) {
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

/* Find the active output node of a group nodetree.
 *
 * Does not return the shading output node but the group output node.
 */
static bNode *ntree_group_output_node(bNodeTree *ntree)
{
  /* Make sure we only have single node tagged as output. */
  ntreeSetOutput(ntree);

  /* Find output node that matches type and target. If there are
   * multiple, we prefer exact target match and active nodes. */
  bNode *output_node = NULL;

  for (bNode *node = ntree->nodes.first; node; node = node->next) {
    if ((node->type == NODE_GROUP_OUTPUT) && (node->flag & NODE_DO_OUTPUT)) {
      output_node = node;
    }
  }

  return output_node;
}

/* Find socket with a specified identifier. */
static bNodeSocket *ntree_shader_node_find_socket(ListBase *sockets, const char *identifier)
{
  for (bNodeSocket *sock = sockets->first; sock != NULL; sock = sock->next) {
    if (STREQ(sock->identifier, identifier)) {
      return sock;
    }
  }
  return NULL;
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

static void ntree_shader_unlink_hidden_value_sockets(bNode *group_node, bNodeSocket *isock)
{
  bNodeTree *group_ntree = (bNodeTree *)group_node->id;
  bNode *node;
  bool removed_link = false;

  for (node = group_ntree->nodes.first; node; node = node->next) {
    for (bNodeSocket *sock = node->inputs.first; sock; sock = sock->next) {
      if ((sock->flag & SOCK_HIDE_VALUE) == 0) {
        continue;
      }
      /* If socket is linked to a group input node and sockets id match. */
      if (sock && sock->link && sock->link->fromnode->type == NODE_GROUP_INPUT) {
        if (STREQ(isock->identifier, sock->link->fromsock->identifier)) {
          nodeRemLink(group_ntree, sock->link);
          removed_link = true;
        }
      }
    }
  }

  if (removed_link) {
    ntreeUpdateTree(G.main, group_ntree);
  }
}

/* Node groups once expanded looses their input sockets values.
 * To fix this, link value/rgba nodes into the sockets and copy the group sockets values. */
static void ntree_shader_groups_expand_inputs(bNodeTree *localtree)
{
  bNode *value_node, *group_node;
  bNodeSocket *value_socket;
  bNodeSocketValueVector *src_vector;
  bNodeSocketValueRGBA *src_rgba, *dst_rgba;
  bNodeSocketValueFloat *src_float, *dst_float;
  bNodeSocketValueInt *src_int;
  bool link_added = false;

  for (group_node = localtree->nodes.first; group_node; group_node = group_node->next) {

    if (!(ELEM(group_node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) || group_node->id == NULL) {
      continue;
    }

    /* Do it recursively. */
    ntree_shader_groups_expand_inputs((bNodeTree *)group_node->id);

    bNodeSocket *group_socket = group_node->inputs.first;
    for (; group_socket; group_socket = group_socket->next) {
      if (group_socket->link != NULL) {
        continue;
      }

      /* Detect the case where an input is plugged into a hidden value socket.
       * In this case we should just remove the link to trigger the socket default override. */
      ntree_shader_unlink_hidden_value_sockets(group_node, group_socket);

      switch (group_socket->type) {
        case SOCK_VECTOR:
          value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_RGB);
          value_socket = ntree_shader_node_find_output(value_node, "Color");
          BLI_assert(value_socket != NULL);
          src_vector = group_socket->default_value;
          dst_rgba = value_socket->default_value;
          copy_v3_v3(dst_rgba->value, src_vector->value);
          dst_rgba->value[3] = 1.0f; /* should never be read */
          break;
        case SOCK_RGBA:
          value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_RGB);
          value_socket = ntree_shader_node_find_output(value_node, "Color");
          BLI_assert(value_socket != NULL);
          src_rgba = group_socket->default_value;
          dst_rgba = value_socket->default_value;
          copy_v4_v4(dst_rgba->value, src_rgba->value);
          break;
        case SOCK_INT:
          /* HACK: Support as float. */
          value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_VALUE);
          value_socket = ntree_shader_node_find_output(value_node, "Value");
          BLI_assert(value_socket != NULL);
          src_int = group_socket->default_value;
          dst_float = value_socket->default_value;
          dst_float->value = (float)(src_int->value);
          break;
        case SOCK_FLOAT:
          value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_VALUE);
          value_socket = ntree_shader_node_find_output(value_node, "Value");
          BLI_assert(value_socket != NULL);
          src_float = group_socket->default_value;
          dst_float = value_socket->default_value;
          dst_float->value = src_float->value;
          break;
        default:
          continue;
      }

      nodeAddLink(localtree, value_node, value_socket, group_node, group_socket);

      link_added = true;
    }
  }

  if (link_added) {
    ntreeUpdateTree(G.main, localtree);
  }
}

/* Check whether shader has a displacement.
 *
 * Will also return a node and it's socket which is connected to a displacement
 * output. Additionally, link which is attached to the displacement output is
 * also returned.
 */
static bool ntree_shader_has_displacement(bNodeTree *ntree,
                                          bNode *output_node,
                                          bNode **r_node,
                                          bNodeSocket **r_socket,
                                          bNodeLink **r_link)
{
  if (output_node == NULL) {
    /* We can't have displacement without output node, apparently. */
    return false;
  }
  /* Make sure sockets links pointers are correct. */
  ntreeUpdateTree(G.main, ntree);
  bNodeSocket *displacement = ntree_shader_node_find_input(output_node, "Displacement");

  if (displacement == NULL) {
    /* Non-cycles node is used as an output. */
    return false;
  }
  if (displacement->link != NULL) {
    *r_node = displacement->link->fromnode;
    *r_socket = displacement->link->fromsock;
    *r_link = displacement->link;
  }
  return displacement->link != NULL;
}

static bool ntree_shader_relink_node_normal(bNodeTree *ntree,
                                            bNode *node,
                                            bNode *node_from,
                                            bNodeSocket *socket_from)
{
  bNodeSocket *sock = ntree_shader_node_find_input(node, "Normal");
  /* TODO(sergey): Can we do something smarter here than just a name-based
   * matching?
   */
  if (sock == NULL) {
    /* There's no Normal input, nothing to link. */
    return false;
  }
  if (sock->link != NULL) {
    /* Something is linked to the normal input already. can't
     * use other input for that.
     */
    return false;
  }
  /* Create connection between specified node and the normal input. */
  nodeAddLink(ntree, node_from, socket_from, node, sock);
  return true;
}

static void ntree_shader_link_builtin_group_normal(bNodeTree *ntree,
                                                   bNode *group_node,
                                                   bNode *node_from,
                                                   bNodeSocket *socket_from,
                                                   bNode *displacement_node,
                                                   bNodeSocket *displacement_socket)
{
  bNodeTree *group_ntree = (bNodeTree *)group_node->id;
  /* Create input socket to plug displacement connection to. */
  bNodeSocket *group_normal_socket = ntreeAddSocketInterface(
      group_ntree, SOCK_IN, "NodeSocketVector", "Normal");
  /* Need to update tree so all node instances nodes gets proper sockets. */
  bNode *group_input_node = ntreeFindType(group_ntree, NODE_GROUP_INPUT);
  node_group_update(ntree, group_node);
  if (group_input_node) {
    node_group_input_update(group_ntree, group_input_node);
  }
  ntreeUpdateTree(G.main, group_ntree);
  /* Assumes sockets are always added at the end. */
  bNodeSocket *group_node_normal_socket = group_node->inputs.last;
  if (displacement_node == group_node) {
    /* If displacement is coming from this node group we need to perform
     * some internal re-linking in order to avoid cycles.
     */
    bNode *group_output_node = ntreeFindType(group_ntree, NODE_GROUP_OUTPUT);
    if (group_output_node == NULL) {
      return;
    }
    bNodeSocket *group_output_node_displacement_socket = nodeFindSocket(
        group_output_node, SOCK_IN, displacement_socket->identifier);
    bNodeLink *group_displacement_link = group_output_node_displacement_socket->link;
    if (group_displacement_link == NULL) {
      /* Displacement output is not connected to anything, can just stop
       * right away.
       */
      return;
    }
    /* This code is similar to ntree_shader_relink_displacement() */
    bNode *group_displacement_node = group_displacement_link->fromnode;
    bNodeSocket *group_displacement_socket = group_displacement_link->fromsock;
    /* Create and link bump node.
     * Can't re-use bump node from parent tree because it'll cause cycle.
     */
    bNode *bump_node = nodeAddStaticNode(NULL, group_ntree, SH_NODE_BUMP);
    bNodeSocket *bump_input_socket = ntree_shader_node_find_input(bump_node, "Height");
    bNodeSocket *bump_output_socket = ntree_shader_node_find_output(bump_node, "Normal");
    BLI_assert(bump_input_socket != NULL);
    BLI_assert(bump_output_socket != NULL);
    nodeAddLink(group_ntree,
                group_displacement_node,
                group_displacement_socket,
                bump_node,
                bump_input_socket);
    /* Relink normals inside of the instanced tree. */
    ntree_shader_link_builtin_normal(group_ntree,
                                     bump_node,
                                     bump_output_socket,
                                     group_displacement_node,
                                     group_displacement_socket);
    ntreeUpdateTree(G.main, group_ntree);
  }
  else if (group_input_node) {
    /* Connect group node normal input. */
    nodeAddLink(ntree, node_from, socket_from, group_node, group_node_normal_socket);
    BLI_assert(group_input_node != NULL);
    bNodeSocket *group_input_node_normal_socket = nodeFindSocket(
        group_input_node, SOCK_OUT, group_normal_socket->identifier);
    BLI_assert(group_input_node_normal_socket != NULL);
    /* Relink normals inside of the instanced tree. */
    ntree_shader_link_builtin_normal(group_ntree,
                                     group_input_node,
                                     group_input_node_normal_socket,
                                     displacement_node,
                                     displacement_socket);
    ntreeUpdateTree(G.main, group_ntree);
  }
}

/* Use specified node and socket as an input for unconnected normal sockets. */
static void ntree_shader_link_builtin_normal(bNodeTree *ntree,
                                             bNode *node_from,
                                             bNodeSocket *socket_from,
                                             bNode *displacement_node,
                                             bNodeSocket *displacement_socket)
{
  for (bNode *node = ntree->nodes.first; node != NULL; node = node->next) {
    if (node == node_from) {
      /* Don't connect node itself! */
      continue;
    }
    if ((ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) && node->id) {
      /* Special re-linking for group nodes. */
      ntree_shader_link_builtin_group_normal(
          ntree, node, node_from, socket_from, displacement_node, displacement_socket);
      continue;
    }
    if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
      /* Group inputs and outputs needs nothing special. */
      continue;
    }
    ntree_shader_relink_node_normal(ntree, node, node_from, socket_from);
  }
}

/* Re-link displacement output to unconnected normal sockets via bump node.
 * This way material with have proper displacement in the viewport.
 */
static void ntree_shader_relink_displacement(bNodeTree *ntree, bNode *output_node)
{
  bNode *displacement_node;
  bNodeSocket *displacement_socket;
  bNodeLink *displacement_link;
  if (!ntree_shader_has_displacement(
          ntree, output_node, &displacement_node, &displacement_socket, &displacement_link)) {
    /* There is no displacement output connected, nothing to re-link. */
    return;
  }
  /* We have to disconnect displacement output socket, otherwise we'll have
   * cycles in the Cycles material :)
   */
  nodeRemLink(ntree, displacement_link);

  /* Convert displacement vector to bump height. */
  bNode *dot_node = nodeAddStaticNode(NULL, ntree, SH_NODE_VECT_MATH);
  bNode *geo_node = nodeAddStaticNode(NULL, ntree, SH_NODE_NEW_GEOMETRY);
  dot_node->custom1 = 3; /* dot product */

  nodeAddLink(ntree, displacement_node, displacement_socket, dot_node, dot_node->inputs.first);
  nodeAddLink(ntree,
              geo_node,
              ntree_shader_node_find_output(geo_node, "Normal"),
              dot_node,
              dot_node->inputs.last);
  displacement_node = dot_node;
  displacement_socket = ntree_shader_node_find_output(dot_node, "Value");

  /* We can't connect displacement to normal directly, use bump node for that
   * and hope that it gives good enough approximation.
   */
  bNode *bump_node = nodeAddStaticNode(NULL, ntree, SH_NODE_BUMP);
  bNodeSocket *bump_input_socket = ntree_shader_node_find_input(bump_node, "Height");
  bNodeSocket *bump_output_socket = ntree_shader_node_find_output(bump_node, "Normal");
  BLI_assert(bump_input_socket != NULL);
  BLI_assert(bump_output_socket != NULL);
  /* Connect bump node to where displacement output was originally
   * connected to.
   */
  nodeAddLink(ntree, displacement_node, displacement_socket, bump_node, bump_input_socket);
  /* Connect all free-standing Normal inputs. */
  ntree_shader_link_builtin_normal(
      ntree, bump_node, bump_output_socket, displacement_node, displacement_socket);
  /* TODO(sergey): Reconnect Geometry Info->Normal sockets to the new
   * bump node.
   */
  /* We modified the tree, it needs to be updated now. */
  ntreeUpdateTree(G.main, ntree);
}

static bool ntree_tag_bsdf_cb(bNode *fromnode,
                              bNode *UNUSED(tonode),
                              void *userdata,
                              const bool UNUSED(reversed))
{
  /* Don't evaluate nodes more than once. */
  if (fromnode->tmp_flag) {
    return true;
  }
  fromnode->tmp_flag = 1;

  switch (fromnode->type) {
    case NODE_GROUP:
    case NODE_CUSTOM_GROUP:
      /* Recursive */
      if (fromnode->id != NULL) {
        bNodeTree *ntree = (bNodeTree *)fromnode->id;
        bNode *group_output = ntree_group_output_node(ntree);
        ntree_shader_tag_nodes(ntree, group_output, (nTreeTags *)userdata);
      }
      break;
    case SH_NODE_BSDF_ANISOTROPIC:
    case SH_NODE_EEVEE_SPECULAR:
    case SH_NODE_BSDF_GLOSSY:
    case SH_NODE_BSDF_GLASS:
      fromnode->ssr_id = ((nTreeTags *)userdata)->ssr_id;
      ((nTreeTags *)userdata)->ssr_id += 1;
      break;
    case SH_NODE_SUBSURFACE_SCATTERING:
      fromnode->sss_id = ((nTreeTags *)userdata)->sss_id;
      ((nTreeTags *)userdata)->sss_id += 1;
      break;
    case SH_NODE_BSDF_PRINCIPLED:
      fromnode->ssr_id = ((nTreeTags *)userdata)->ssr_id;
      fromnode->sss_id = ((nTreeTags *)userdata)->sss_id;
      ((nTreeTags *)userdata)->sss_id += 1;
      ((nTreeTags *)userdata)->ssr_id += 1;
      break;
    default:
      /* We could return false here but since we
       * allow the use of Closure as RGBA, we can have
       * Bsdf nodes linked to other Bsdf nodes. */
      break;
  }

  return true;
}

/* EEVEE: Scan the ntree to set the Screen Space Reflection
 * layer id of every specular node AND the Subsurface Scattering id of every SSS node.
 */
void ntree_shader_tag_nodes(bNodeTree *ntree, bNode *output_node, nTreeTags *tags)
{
  if (output_node == NULL) {
    return;
  }
  /* Make sure sockets links pointers are correct. */
  ntreeUpdateTree(G.main, ntree);

  /* Reset visit flag. */
  for (bNode *node = ntree->nodes.first; node; node = node->next) {
    node->tmp_flag = 0;
  }

  nodeChainIter(ntree, output_node, ntree_tag_bsdf_cb, tags, true);
}

/* This one needs to work on a local tree. */
void ntreeGPUMaterialNodes(bNodeTree *localtree,
                           GPUMaterial *mat,
                           bool *has_surface_output,
                           bool *has_volume_output)
{
  bNodeTreeExec *exec;

  /* Extract output nodes from inside nodegroups. */
  ntree_shader_output_node_from_group(localtree, SHD_OUTPUT_EEVEE);

  bNode *output = ntreeShaderOutputNode(localtree, SHD_OUTPUT_EEVEE);

  ntree_shader_groups_expand_inputs(localtree);

  /* Perform all needed modifications on the tree in order to support
   * displacement/bump mapping.
   */
  ntree_shader_relink_displacement(localtree, output);

  /* TODO(fclem): consider moving this to the gpu shader tree evaluation. */
  nTreeTags tags = {
      .ssr_id = 1.0,
      .sss_id = 1.0,
  };
  ntree_shader_tag_nodes(localtree, output, &tags);

  exec = ntreeShaderBeginExecTree(localtree);
  ntreeExecGPUNodes(exec, mat, output);
  ntreeShaderEndExecTree(exec);

  /* EEVEE: Find which material domain was used (volume, surface ...). */
  *has_surface_output = false;
  *has_volume_output = false;

  if (output != NULL) {
    bNodeSocket *surface_sock = ntree_shader_node_find_input(output, "Surface");
    bNodeSocket *volume_sock = ntree_shader_node_find_input(output, "Volume");

    if (surface_sock != NULL) {
      *has_surface_output = (nodeCountSocketLinks(localtree, surface_sock) > 0);
    }

    if (volume_sock != NULL) {
      *has_volume_output = (nodeCountSocketLinks(localtree, volume_sock) > 0);
    }
  }
}

bNodeTreeExec *ntreeShaderBeginExecTree_internal(bNodeExecContext *context,
                                                 bNodeTree *ntree,
                                                 bNodeInstanceKey parent_key)
{
  bNodeTreeExec *exec;
  bNode *node;

  /* ensures only a single output node is enabled */
  ntreeSetOutput(ntree);

  /* common base initialization */
  exec = ntree_exec_begin(context, ntree, parent_key);

  /* allocate the thread stack listbase array */
  exec->threadstack = MEM_callocN(BLENDER_MAX_THREADS * sizeof(ListBase), "thread stack array");

  for (node = exec->nodetree->nodes.first; node; node = node->next) {
    node->need_exec = 1;
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
  if (ntree->execdata) {
    return ntree->execdata;
  }

  context.previews = ntree->previews;

  exec = ntreeShaderBeginExecTree_internal(&context, ntree, NODE_INSTANCE_KEY_BASE);

  /* XXX this should not be necessary, but is still used for cmp/sha/tex nodes,
   * which only store the ntree pointer. Should be fixed at some point!
   */
  ntree->execdata = exec;

  return exec;
}

void ntreeShaderEndExecTree_internal(bNodeTreeExec *exec)
{
  bNodeThreadStack *nts;
  int a;

  if (exec->threadstack) {
    for (a = 0; a < BLENDER_MAX_THREADS; a++) {
      for (nts = exec->threadstack[a].first; nts; nts = nts->next) {
        if (nts->stack) {
          MEM_freeN(nts->stack);
        }
      }
      BLI_freelistN(&exec->threadstack[a]);
    }

    MEM_freeN(exec->threadstack);
    exec->threadstack = NULL;
  }

  ntree_exec_end(exec);
}

void ntreeShaderEndExecTree(bNodeTreeExec *exec)
{
  if (exec) {
    /* exec may get freed, so assign ntree */
    bNodeTree *ntree = exec->nodetree;
    ntreeShaderEndExecTree_internal(exec);

    /* XXX clear nodetree backpointer to exec data, same problem as noted in ntreeBeginExecTree */
    ntree->execdata = NULL;
  }
}

/* TODO: left over from Blender Internal, could reuse for new texture nodes. */
bool ntreeShaderExecTree(bNodeTree *ntree, int thread)
{
  ShaderCallData scd;
  bNodeThreadStack *nts = NULL;
  bNodeTreeExec *exec = ntree->execdata;
  int compat;

  /* ensure execdata is only initialized once */
  if (!exec) {
    BLI_thread_lock(LOCK_NODES);
    if (!ntree->execdata) {
      ntree->execdata = ntreeShaderBeginExecTree(ntree);
    }
    BLI_thread_unlock(LOCK_NODES);

    exec = ntree->execdata;
  }

  nts = ntreeGetThreadStack(exec, thread);
  compat = ntreeExecThreadNodes(exec, nts, &scd, thread);
  ntreeReleaseThreadStack(nts);

  /* if compat is zero, it has been using non-compatible nodes */
  return compat;
}
