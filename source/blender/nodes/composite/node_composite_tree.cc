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

#include <cstdio>

#include "DNA_color_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_tree_update.h"
#include "BKE_tracking.h"

#include "UI_resources.h"

#include "node_common.h"
#include "node_util.h"

#include "RNA_access.h"

#include "NOD_composite.h"
#include "node_composite_util.hh"

#ifdef WITH_COMPOSITOR
#  include "COM_compositor.h"
#endif

static void composite_get_from_context(const bContext *C,
                                       bNodeTreeType *UNUSED(treetype),
                                       bNodeTree **r_ntree,
                                       ID **r_id,
                                       ID **r_from)
{
  Scene *scene = CTX_data_scene(C);

  *r_from = nullptr;
  *r_id = &scene->id;
  *r_ntree = scene->nodetree;
}

static void foreach_nodeclass(Scene *UNUSED(scene), void *calldata, bNodeClassCallback func)
{
  func(calldata, NODE_CLASS_INPUT, N_("Input"));
  func(calldata, NODE_CLASS_OUTPUT, N_("Output"));
  func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
  func(calldata, NODE_CLASS_OP_VECTOR, N_("Vector"));
  func(calldata, NODE_CLASS_OP_FILTER, N_("Filter"));
  func(calldata, NODE_CLASS_CONVERTER, N_("Converter"));
  func(calldata, NODE_CLASS_MATTE, N_("Matte"));
  func(calldata, NODE_CLASS_DISTORT, N_("Distort"));
  func(calldata, NODE_CLASS_GROUP, N_("Group"));
  func(calldata, NODE_CLASS_INTERFACE, N_("Interface"));
  func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

static void free_node_cache(bNodeTree *UNUSED(ntree), bNode *node)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    if (sock->cache) {
      sock->cache = nullptr;
    }
  }
}

static void free_cache(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    free_node_cache(ntree, node);
  }
}

/* local tree then owns all compbufs */
static void localize(bNodeTree *localtree, bNodeTree *ntree)
{

  bNode *node = (bNode *)ntree->nodes.first;
  bNode *local_node = (bNode *)localtree->nodes.first;
  while (node != nullptr) {

    /* Ensure new user input gets handled ok. */
    node->need_exec = 0;
    local_node->original = node;

    /* move over the compbufs */
    /* right after #ntreeCopyTree() `oldsock` pointers are valid */

    if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
      if (node->id) {
        if (node->flag & NODE_DO_OUTPUT) {
          local_node->id = (ID *)node->id;
        }
        else {
          local_node->id = nullptr;
        }
      }
    }

    node = node->next;
    local_node = local_node->next;
  }
}

static void local_merge(Main *bmain, bNodeTree *localtree, bNodeTree *ntree)
{
  bNode *lnode;
  bNodeSocket *lsock;

  /* move over the compbufs and previews */
  BKE_node_preview_merge_tree(ntree, localtree, true);

  for (lnode = (bNode *)localtree->nodes.first; lnode; lnode = lnode->next) {
    if (bNode *orig_node = nodeFindNodebyName(ntree, lnode->name)) {
      if (ELEM(lnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
        if (lnode->id && (lnode->flag & NODE_DO_OUTPUT)) {
          /* image_merge does sanity check for pointers */
          BKE_image_merge(bmain, (Image *)orig_node->id, (Image *)lnode->id);
        }
      }
      else if (lnode->type == CMP_NODE_MOVIEDISTORTION) {
        /* special case for distortion node: distortion context is allocating in exec function
         * and to achieve much better performance on further calls this context should be
         * copied back to original node */
        if (lnode->storage) {
          if (orig_node->storage) {
            BKE_tracking_distortion_free((MovieDistortion *)orig_node->storage);
          }

          orig_node->storage = BKE_tracking_distortion_copy((MovieDistortion *)lnode->storage);
        }
      }

      for (lsock = (bNodeSocket *)lnode->outputs.first; lsock; lsock = lsock->next) {
        if (bNodeSocket *orig_socket = nodeFindSocket(orig_node, SOCK_OUT, lsock->identifier)) {
          orig_socket->cache = lsock->cache;
          lsock->cache = nullptr;
          orig_socket = nullptr;
        }
      }
    }
  }
}

static void update(bNodeTree *ntree)
{
  ntreeSetOutput(ntree);

  ntree_update_reroute_nodes(ntree);
}

static void composite_node_add_init(bNodeTree *UNUSED(bnodetree), bNode *bnode)
{
  /* Composite node will only show previews for input classes
   * by default, other will be hidden
   * but can be made visible with the show_preview option */
  if (bnode->typeinfo->nclass != NODE_CLASS_INPUT) {
    bnode->flag &= ~NODE_PREVIEW;
  }
}

static bool composite_node_tree_socket_type_valid(bNodeTreeType *UNUSED(ntreetype),
                                                  bNodeSocketType *socket_type)
{
  return nodeIsStaticSocketType(socket_type) &&
         ELEM(socket_type->type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA);
}

bNodeTreeType *ntreeType_Composite;

void register_node_tree_type_cmp()
{
  bNodeTreeType *tt = ntreeType_Composite = MEM_cnew<bNodeTreeType>(__func__);

  tt->type = NTREE_COMPOSIT;
  strcpy(tt->idname, "CompositorNodeTree");
  strcpy(tt->ui_name, N_("Compositor"));
  tt->ui_icon = ICON_NODE_COMPOSITING;
  strcpy(tt->ui_description, N_("Compositing nodes"));

  tt->free_cache = free_cache;
  tt->free_node_cache = free_node_cache;
  tt->foreach_nodeclass = foreach_nodeclass;
  tt->localize = localize;
  tt->local_merge = local_merge;
  tt->update = update;
  tt->get_from_context = composite_get_from_context;
  tt->node_add_init = composite_node_add_init;
  tt->valid_socket_type = composite_node_tree_socket_type_valid;

  tt->rna_ext.srna = &RNA_CompositorNodeTree;

  ntreeTypeAdd(tt);
}

void ntreeCompositExecTree(Scene *scene,
                           bNodeTree *ntree,
                           RenderData *rd,
                           int rendering,
                           int do_preview,
                           const ColorManagedViewSettings *view_settings,
                           const ColorManagedDisplaySettings *display_settings,
                           const char *view_name)
{
#ifdef WITH_COMPOSITOR
  COM_execute(rd, scene, ntree, rendering, view_settings, display_settings, view_name);
#else
  UNUSED_VARS(scene, ntree, rd, rendering, view_settings, display_settings, view_name);
#endif

  UNUSED_VARS(do_preview);
}

/* *********************************************** */

void ntreeCompositUpdateRLayers(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == CMP_NODE_R_LAYERS) {
      node_cmp_rlayers_outputs(ntree, node);
    }
  }
}

void ntreeCompositTagRender(Scene *scene)
{
  /* XXX Think using G_MAIN here is valid, since you want to update current file's scene nodes,
   * not the ones in temp main generated for rendering?
   * This is still rather weak though,
   * ideally render struct would store own main AND original G_MAIN. */

  for (Scene *sce_iter = (Scene *)G_MAIN->scenes.first; sce_iter;
       sce_iter = (Scene *)sce_iter->id.next) {
    if (sce_iter->nodetree) {
      LISTBASE_FOREACH (bNode *, node, &sce_iter->nodetree->nodes) {
        if (node->id == (ID *)scene || node->type == CMP_NODE_COMPOSITE) {
          BKE_ntree_update_tag_node_property(sce_iter->nodetree, node);
        }
        else if (node->type == CMP_NODE_TEXTURE) /* uses scene size_x/size_y */ {
          BKE_ntree_update_tag_node_property(sce_iter->nodetree, node);
        }
      }
    }
  }
  BKE_ntree_update_main(G_MAIN, nullptr);
}

/* XXX after render animation system gets a refresh, this call allows composite to end clean */
void ntreeCompositClearTags(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    node->need_exec = 0;
    if (node->type == NODE_GROUP) {
      ntreeCompositClearTags((bNodeTree *)node->id);
    }
  }
}
