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
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "BLI_alloca.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_linestyle.h"
#include "BKE_node.h"
#include "BKE_scene.h"

#include "RNA_access.h"

#include "GPU_material.h"

#include "RE_texture.h"

#include "NOD_common.h"

#include "node_common.h"
#include "node_exec.h"
#include "node_shader_util.h"
#include "node_util.h"

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

  tt->rna_ext.srna = &RNA_ShaderNodeTree;

  ntreeTypeAdd(tt);
}

/* GPU material from shader nodes */

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

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
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
      value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_RGB);
      value_socket = ntree_shader_node_find_output(value_node, "Color");
      BLI_assert(value_socket != NULL);
      src_vector = socket->default_value;
      dst_rgba = value_socket->default_value;
      copy_v3_v3(dst_rgba->value, src_vector->value);
      dst_rgba->value[3] = 1.0f; /* should never be read */
      break;
    case SOCK_RGBA:
      value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_RGB);
      value_socket = ntree_shader_node_find_output(value_node, "Color");
      BLI_assert(value_socket != NULL);
      src_rgba = socket->default_value;
      dst_rgba = value_socket->default_value;
      copy_v4_v4(dst_rgba->value, src_rgba->value);
      break;
    case SOCK_INT:
      /* HACK: Support as float. */
      value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_VALUE);
      value_socket = ntree_shader_node_find_output(value_node, "Value");
      BLI_assert(value_socket != NULL);
      src_int = socket->default_value;
      dst_float = value_socket->default_value;
      dst_float->value = (float)(src_int->value);
      break;
    case SOCK_FLOAT:
      value_node = nodeAddStaticNode(NULL, localtree, SH_NODE_VALUE);
      value_socket = ntree_shader_node_find_output(value_node, "Value");
      BLI_assert(value_socket != NULL);
      src_float = socket->default_value;
      dst_float = value_socket->default_value;
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
  bNode *node;
  bool removed_link = false;

  for (node = group_ntree->nodes.first; node; node = node->next) {
    const bool is_group = ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && (node->id != NULL);

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
    ntreeUpdateTree(G.main, group_ntree);
  }
}

/* Node groups once expanded looses their input sockets values.
 * To fix this, link value/rgba nodes into the sockets and copy the group sockets values. */
static void ntree_shader_groups_expand_inputs(bNodeTree *localtree)
{
  bool link_added = false;

  LISTBASE_FOREACH (bNode *, node, &localtree->nodes) {
    const bool is_group = ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && (node->id != NULL);
    const bool is_group_output = node->type == NODE_GROUP_OUTPUT && (node->flag & NODE_DO_OUTPUT);

    if (is_group) {
      /* Do it recursively. */
      ntree_shader_groups_expand_inputs((bNodeTree *)node->id);
    }

    if (is_group || is_group_output) {
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        if (socket->link != NULL && !(socket->link->flag & NODE_LINK_MUTED)) {
          bNodeLink *link = socket->link;
          /* Fix the case where the socket is actually converting the data. (see T71374)
           * We only do the case of lossy conversion to float.*/
          if ((socket->type == SOCK_FLOAT) && (link->fromsock->type != link->tosock->type)) {
            if (link->fromsock->type == SOCK_RGBA) {
              bNode *tmp = nodeAddStaticNode(NULL, localtree, SH_NODE_RGBTOBW);
              nodeAddLink(localtree, link->fromnode, link->fromsock, tmp, tmp->inputs.first);
              nodeAddLink(localtree, tmp, tmp->outputs.first, node, socket);
            }
            else if (link->fromsock->type == SOCK_VECTOR) {
              bNode *tmp = nodeAddStaticNode(NULL, localtree, SH_NODE_VECTOR_MATH);
              tmp->custom1 = NODE_VECTOR_MATH_DOT_PRODUCT;
              bNodeSocket *dot_input1 = tmp->inputs.first;
              bNodeSocket *dot_input2 = dot_input1->next;
              bNodeSocketValueVector *input2_socket_value = dot_input2->default_value;
              copy_v3_fl(input2_socket_value->value, 1.0f / 3.0f);
              nodeAddLink(localtree, link->fromnode, link->fromsock, tmp, dot_input1);
              nodeAddLink(localtree, tmp, tmp->outputs.last, node, socket);
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
    ntreeUpdateTree(G.main, localtree);
  }
}

static void flatten_group_do(bNodeTree *ntree, bNode *gnode)
{
  bNodeLink *link, *linkn, *tlink;
  bNode *node, *nextnode;
  bNodeTree *ngroup;
  LinkNode *group_interface_nodes = NULL;

  ngroup = (bNodeTree *)gnode->id;

  /* Add the nodes into the ntree */
  for (node = ngroup->nodes.first; node; node = nextnode) {
    nextnode = node->next;
    /* Remove interface nodes.
     * This also removes remaining links to and from interface nodes.
     * We must delay removal since sockets will reference this node. see: T52092 */
    if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
      BLI_linklist_prepend(&group_interface_nodes, node);
    }
    /* migrate node */
    BLI_remlink(&ngroup->nodes, node);
    BLI_addtail(&ntree->nodes, node);
    /* ensure unique node name in the node tree */
    /* This is very slow and it has no use for GPU nodetree. (see T70609) */
    // nodeUniqueName(ntree, node);
  }

  /* Save first and last link to iterate over flattened group links. */
  bNodeLink *glinks_first = ntree->links.last;

  /* Add internal links to the ntree */
  for (link = ngroup->links.first; link; link = linkn) {
    linkn = link->next;
    BLI_remlink(&ngroup->links, link);
    BLI_addtail(&ntree->links, link);
  }

  bNodeLink *glinks_last = ntree->links.last;

  /* restore external links to and from the gnode */
  if (glinks_first != NULL) {
    /* input links */
    for (link = glinks_first->next; link != glinks_last->next; link = link->next) {
      if (link->fromnode->type == NODE_GROUP_INPUT) {
        const char *identifier = link->fromsock->identifier;
        /* find external links to this input */
        for (tlink = ntree->links.first; tlink != glinks_first->next; tlink = tlink->next) {
          if (tlink->tonode == gnode && STREQ(tlink->tosock->identifier, identifier)) {
            nodeAddLink(ntree, tlink->fromnode, tlink->fromsock, link->tonode, link->tosock);
          }
        }
      }
    }
    /* Also iterate over the new links to cover passthrough links. */
    glinks_last = ntree->links.last;
    /* output links */
    for (tlink = ntree->links.first; tlink != glinks_first->next; tlink = tlink->next) {
      if (tlink->fromnode == gnode) {
        const char *identifier = tlink->fromsock->identifier;
        /* find internal links to this output */
        for (link = glinks_first->next; link != glinks_last->next; link = link->next) {
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
    node = BLI_linklist_pop(&group_interface_nodes);
    ntreeFreeLocalNode(ntree, node);
  }

  ntree->update |= NTREE_UPDATE_NODES | NTREE_UPDATE_LINKS;
}

/* Flatten group to only have a simple single tree */
static void ntree_shader_groups_flatten(bNodeTree *localtree)
{
  /* This is effectively recursive as the flattened groups will add
   * nodes at the end of the list, which will also get evaluated. */
  for (bNode *node = localtree->nodes.first, *node_next; node; node = node_next) {
    if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && node->id != NULL) {
      flatten_group_do(localtree, node);
      /* Continue even on new flattened nodes. */
      node_next = node->next;
      /* delete the group instance and its localtree. */
      bNodeTree *ngroup = (bNodeTree *)node->id;
      ntreeFreeLocalNode(localtree, node);
      ntreeFreeTree(ngroup);
      MEM_freeN(ngroup);
    }
    else {
      node_next = node->next;
    }
  }

  ntreeUpdateTree(G.main, localtree);
}

/* Check whether shader has a displacement.
 *
 * Will also return a node and its socket which is connected to a displacement
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

  if ((displacement->link != NULL) && !(displacement->link->flag & NODE_LINK_MUTED)) {
    *r_node = displacement->link->fromnode;
    *r_socket = displacement->link->fromsock;
    *r_link = displacement->link;
    return true;
  }
  return false;
}

static void ntree_shader_relink_node_normal(bNodeTree *ntree,
                                            bNode *node,
                                            bNode *node_from,
                                            bNodeSocket *socket_from)
{
  /* TODO(sergey): Can we do something smarter here than just a name-based
   * matching?
   */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STREQ(sock->identifier, "Normal") && sock->link == NULL) {
      /* It's a normal input and nothing is connected to it. */
      nodeAddLink(ntree, node_from, socket_from, node, sock);
    }
    else if (sock->link) {
      bNodeLink *link = sock->link;
      if (ELEM(link->fromnode->type, SH_NODE_NEW_GEOMETRY, SH_NODE_TEX_COORD) &&
          STREQ(link->fromsock->identifier, "Normal")) {
        /* Linked to a geometry node normal output. */
        nodeAddLink(ntree, node_from, socket_from, node, sock);
      }
    }
  }
}

/* Use specified node and socket as an input for unconnected normal sockets. */
static void ntree_shader_link_builtin_normal(bNodeTree *ntree,
                                             bNode *node_from,
                                             bNodeSocket *socket_from)
{
  for (bNode *node = ntree->nodes.first; node != NULL; node = node->next) {
    if (node == node_from) {
      /* Don't connect node itself! */
      continue;
    }
    if (node->tmp_flag == -2) {
      /* This node is used inside the displacement tree. Skip to avoid cycles. */
      continue;
    }
    ntree_shader_relink_node_normal(ntree, node, node_from, socket_from);
  }
}

static void ntree_shader_bypass_bump_link(bNodeTree *ntree, bNode *bump_node, bNodeLink *bump_link)
{
  /* Bypass bump nodes. This replicates cycles "implicit" behavior. */
  bNodeSocket *bump_normal_input = ntree_shader_node_find_input(bump_node, "Normal");
  bNode *fromnode;
  bNodeSocket *fromsock;
  /* Default to builtin normals if there is no link. */
  if (bump_normal_input->link) {
    fromsock = bump_normal_input->link->fromsock;
    fromnode = bump_normal_input->link->fromnode;
  }
  else {
    fromnode = nodeAddStaticNode(NULL, ntree, SH_NODE_NEW_GEOMETRY);
    fromsock = ntree_shader_node_find_output(fromnode, "Normal");
  }
  /* Bypass the bump node by creating a link between the previous and next node. */
  nodeAddLink(ntree, fromnode, fromsock, bump_link->tonode, bump_link->tosock);
  nodeRemLink(ntree, bump_link);
}

static void ntree_shader_bypass_tagged_bump_nodes(bNodeTree *ntree)
{
  /* Bypass bump links inside copied nodes */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    bNode *node = link->fromnode;
    /* If node is a copy. */
    if (node->tmp_flag == -2 && node->type == SH_NODE_BUMP) {
      ntree_shader_bypass_bump_link(ntree, node, link);
    }
  }
  ntreeUpdateTree(G.main, ntree);
}

static bool ntree_branch_count_and_tag_nodes(bNode *fromnode, bNode *tonode, void *userdata)
{
  int *node_count = (int *)userdata;
  if (fromnode->tmp_flag == -1) {
    fromnode->tmp_flag = *node_count;
    (*node_count)++;
  }
  if (tonode->tmp_flag == -1) {
    tonode->tmp_flag = *node_count;
    (*node_count)++;
  }
  return true;
}

/* Create a copy of a branch starting from a given node.
 * callback is executed once for every copied node.
 * Returns input node copy. */
static bNode *ntree_shader_copy_branch(bNodeTree *ntree,
                                       bNode *start_node,
                                       void (*callback)(bNode *node, int user_data),
                                       int user_data)
{
  /* Init tmp flag. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->tmp_flag = -1;
  }
  /* Count and tag all nodes inside the displacement branch of the tree. */
  start_node->tmp_flag = 0;
  int node_count = 1;
  nodeChainIterBackwards(ntree, start_node, ntree_branch_count_and_tag_nodes, &node_count, 1);
  /* Make a full copy of the branch */
  bNode **nodes_copy = MEM_mallocN(sizeof(bNode *) * node_count, __func__);
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->tmp_flag >= 0) {
      int id = node->tmp_flag;
      nodes_copy[id] = BKE_node_copy_ex(
          ntree, node, LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_MAIN, false);
      nodes_copy[id]->tmp_flag = -2; /* Copy */
      /* Make sure to clear all sockets links as they are invalid. */
      LISTBASE_FOREACH (bNodeSocket *, sock, &nodes_copy[id]->inputs) {
        sock->link = NULL;
      }
      LISTBASE_FOREACH (bNodeSocket *, sock, &nodes_copy[id]->outputs) {
        sock->link = NULL;
      }
    }
  }
  /* Recreate links between copied nodes. */
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->fromnode->tmp_flag >= 0 && link->tonode->tmp_flag >= 0) {
      bNode *fromnode = nodes_copy[link->fromnode->tmp_flag];
      bNode *tonode = nodes_copy[link->tonode->tmp_flag];
      bNodeSocket *fromsock = ntree_shader_node_find_output(fromnode, link->fromsock->identifier);
      bNodeSocket *tosock = ntree_shader_node_find_input(tonode, link->tosock->identifier);
      nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);
    }
  }
  /* Per node callback. */
  if (callback) {
    for (int i = 0; i < node_count; i++) {
      callback(nodes_copy[i], user_data);
    }
  }
  bNode *start_node_copy = nodes_copy[start_node->tmp_flag];
  MEM_freeN(nodes_copy);
  return start_node_copy;
}

static void ntree_shader_copy_branch_displacement(bNodeTree *ntree,
                                                  bNode *displacement_node,
                                                  bNodeSocket *displacement_socket,
                                                  bNodeLink *displacement_link)
{
  /* Replace displacement socket/node/link. */
  bNode *tonode = displacement_link->tonode;
  bNodeSocket *tosock = displacement_link->tosock;
  displacement_node = ntree_shader_copy_branch(ntree, displacement_node, NULL, 0);
  displacement_socket = ntree_shader_node_find_output(displacement_node,
                                                      displacement_socket->identifier);
  nodeRemLink(ntree, displacement_link);
  nodeAddLink(ntree, displacement_node, displacement_socket, tonode, tosock);

  ntreeUpdateTree(G.main, ntree);
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

  /* Copy the whole displacement branch to avoid cyclic dependency
   * and issue when bypassing bump nodes. */
  ntree_shader_copy_branch_displacement(
      ntree, displacement_node, displacement_socket, displacement_link);
  /* Bypass bump nodes inside the copied branch to mimic cycles behavior. */
  ntree_shader_bypass_tagged_bump_nodes(ntree);

  /* Displacement Node may have changed because of branch copy and bump bypass. */
  ntree_shader_has_displacement(
      ntree, output_node, &displacement_node, &displacement_socket, &displacement_link);

  /* We have to disconnect displacement output socket, otherwise we'll have
   * cycles in the Cycles material :)
   */
  nodeRemLink(ntree, displacement_link);

  /* Convert displacement vector to bump height. */
  bNode *dot_node = nodeAddStaticNode(NULL, ntree, SH_NODE_VECTOR_MATH);
  bNode *geo_node = nodeAddStaticNode(NULL, ntree, SH_NODE_NEW_GEOMETRY);
  bNodeSocket *normal_socket = ntree_shader_node_find_output(geo_node, "Normal");
  bNodeSocket *dot_input1 = dot_node->inputs.first;
  bNodeSocket *dot_input2 = dot_input1->next;
  dot_node->custom1 = NODE_VECTOR_MATH_DOT_PRODUCT;

  nodeAddLink(ntree, displacement_node, displacement_socket, dot_node, dot_input1);
  nodeAddLink(ntree, geo_node, normal_socket, dot_node, dot_input2);
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

  /* Tag as part of the new displacmeent tree. */
  dot_node->tmp_flag = -2;
  geo_node->tmp_flag = -2;
  bump_node->tmp_flag = -2;

  ntreeUpdateTree(G.main, ntree);

  /* Connect all free-standing Normal inputs and relink geometry/coordinate nodes. */
  ntree_shader_link_builtin_normal(ntree, bump_node, bump_output_socket);
  /* We modified the tree, it needs to be updated now. */
  ntreeUpdateTree(G.main, ntree);
}

static void node_tag_branch_as_derivative(bNode *node, int dx)
{
  if (dx) {
    node->branch_tag = 1;
  }
  else {
    node->branch_tag = 2;
  }
}

static bool ntree_shader_bump_branches(bNode *fromnode, bNode *UNUSED(tonode), void *userdata)
{
  bNodeTree *ntree = (bNodeTree *)userdata;

  if (fromnode->type == SH_NODE_BUMP) {
    bNodeSocket *height_dx_sock, *height_dy_sock, *bump_socket, *bump_dx_socket, *bump_dy_socket;
    bNode *bump = fromnode;
    bump_socket = ntree_shader_node_find_input(bump, "Height");
    bump_dx_socket = ntree_shader_node_find_input(bump, "Height_dx");
    bump_dy_socket = ntree_shader_node_find_input(bump, "Height_dy");
    if (bump_dx_socket->link) {
      /* Avoid reconnecting the same bump twice. */
    }
    else if (bump_socket && bump_socket->link) {
      bNodeLink *link = bump_socket->link;
      bNode *height = link->fromnode;
      bNode *height_dx = ntree_shader_copy_branch(ntree, height, node_tag_branch_as_derivative, 1);
      bNode *height_dy = ntree_shader_copy_branch(ntree, height, node_tag_branch_as_derivative, 0);
      height_dx_sock = ntree_shader_node_find_output(height_dx, link->fromsock->identifier);
      height_dy_sock = ntree_shader_node_find_output(height_dy, link->fromsock->identifier);
      nodeAddLink(ntree, height_dx, height_dx_sock, bump, bump_dx_socket);
      nodeAddLink(ntree, height_dy, height_dy_sock, bump, bump_dy_socket);
      /* We could end iter here, but other bump node could be plugged into other input sockets. */
    }
  }
  return true;
}

static bool ntree_tag_bsdf_cb(bNode *fromnode, bNode *UNUSED(tonode), void *userdata)
{
  switch (fromnode->type) {
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

  nodeChainIterBackwards(ntree, output_node, ntree_tag_bsdf_cb, tags, 0);
}

/* This one needs to work on a local tree. */
void ntreeGPUMaterialNodes(bNodeTree *localtree,
                           GPUMaterial *mat,
                           bool *has_surface_output,
                           bool *has_volume_output)
{
  bNodeTreeExec *exec;

  bNode *output = ntreeShaderOutputNode(localtree, SHD_OUTPUT_EEVEE);

  ntree_shader_groups_expand_inputs(localtree);

  ntree_shader_groups_flatten(localtree);

  if (output == NULL) {
    /* Search again, now including flattened nodes. */
    output = ntreeShaderOutputNode(localtree, SHD_OUTPUT_EEVEE);
  }

  /* Perform all needed modifications on the tree in order to support
   * displacement/bump mapping.
   */
  ntree_shader_relink_displacement(localtree, output);

  /* Duplicate bump height branches for manual derivatives.
   */
  nodeChainIterBackwards(localtree, output, ntree_shader_bump_branches, localtree, 0);
  LISTBASE_FOREACH (bNode *, node, &localtree->nodes) {
    if (node->type == SH_NODE_OUTPUT_AOV) {
      nodeChainIterBackwards(localtree, node, ntree_shader_bump_branches, localtree, 0);
      nTreeTags tags = {
          .ssr_id = 1.0,
          .sss_id = 1.0,
      };
      ntree_shader_tag_nodes(localtree, node, &tags);
    }
  }

  /* TODO(fclem): consider moving this to the gpu shader tree evaluation. */
  nTreeTags tags = {
      .ssr_id = 1.0,
      .sss_id = 1.0,
  };
  ntree_shader_tag_nodes(localtree, output, &tags);

  exec = ntreeShaderBeginExecTree(localtree);
  ntreeExecGPUNodes(exec, mat, output);
  LISTBASE_FOREACH (bNode *, node, &localtree->nodes) {
    if (node->type == SH_NODE_OUTPUT_AOV) {
      ntreeExecGPUNodes(exec, mat, node);
    }
  }
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
