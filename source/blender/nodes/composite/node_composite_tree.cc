/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include <cstdio>

#include "BLI_string.h"

#include "DNA_color_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.h"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_tracking.h"

#include "UI_resources.hh"

#include "node_common.h"

#include "RNA_prototypes.h"

#include "NOD_composite.hh"
#include "node_composite_util.hh"

#ifdef WITH_COMPOSITOR_CPU
#  include "COM_compositor.hh"
#endif

static void composite_get_from_context(
    const bContext *C, bNodeTreeType * /*treetype*/, bNodeTree **r_ntree, ID **r_id, ID **r_from)
{
  Scene *scene = CTX_data_scene(C);

  *r_from = nullptr;
  *r_id = &scene->id;
  *r_ntree = scene->nodetree;
}

static void foreach_nodeclass(Scene * /*scene*/, void *calldata, bNodeClassCallback func)
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

/* local tree then owns all compbufs */
static void localize(bNodeTree *localtree, bNodeTree *ntree)
{

  bNode *node = (bNode *)ntree->nodes.first;
  bNode *local_node = (bNode *)localtree->nodes.first;
  while (node != nullptr) {

    /* Ensure new user input gets handled ok. */
    node->runtime->need_exec = 0;
    local_node->runtime->original = node;

    /* move over the compbufs */
    /* right after #blender::bke::ntreeCopyTree() `oldsock` pointers are valid */

    if (node->type == CMP_NODE_VIEWER) {
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
  /* move over the compbufs and previews */
  blender::bke::node_preview_merge_tree(ntree, localtree, true);

  LISTBASE_FOREACH (bNode *, lnode, &localtree->nodes) {
    if (bNode *orig_node = nodeFindNodebyName(ntree, lnode->name)) {
      if (lnode->type == CMP_NODE_VIEWER) {
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
    }
  }
}

static void update(bNodeTree *ntree)
{
  ntreeSetOutput(ntree);

  ntree_update_reroute_nodes(ntree);
}

static void composite_node_add_init(bNodeTree * /*bnodetree*/, bNode *bnode)
{
  /* Composite node will only show previews for input classes
   * by default, other will be hidden
   * but can be made visible with the show_preview option */
  if (bnode->typeinfo->nclass != NODE_CLASS_INPUT) {
    bnode->flag &= ~NODE_PREVIEW;
  }
}

static bool composite_node_tree_socket_type_valid(bNodeTreeType * /*ntreetype*/,
                                                  bNodeSocketType *socket_type)
{
  return blender::bke::nodeIsStaticSocketType(socket_type) &&
         ELEM(socket_type->type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA);
}

bNodeTreeType *ntreeType_Composite;

void register_node_tree_type_cmp()
{
  bNodeTreeType *tt = ntreeType_Composite = MEM_cnew<bNodeTreeType>(__func__);

  tt->type = NTREE_COMPOSIT;
  STRNCPY(tt->idname, "CompositorNodeTree");
  STRNCPY(tt->group_idname, "CompositorNodeGroup");
  STRNCPY(tt->ui_name, N_("Compositor"));
  tt->ui_icon = ICON_NODE_COMPOSITING;
  STRNCPY(tt->ui_description, N_("Compositing nodes"));

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

void ntreeCompositExecTree(Render *render,
                           Scene *scene,
                           bNodeTree *ntree,
                           RenderData *rd,
                           bool rendering,
                           int do_preview,
                           const char *view_name,
                           blender::realtime_compositor::RenderContext *render_context,
                           blender::compositor::ProfilerData &profiler_data)
{
#ifdef WITH_COMPOSITOR_CPU
  COM_execute(render, rd, scene, ntree, rendering, view_name, render_context, profiler_data);
#else
  UNUSED_VARS(render, scene, ntree, rd, rendering, view_name, render_context, profiler_data);
#endif

  UNUSED_VARS(do_preview);
}

/* *********************************************** */

void ntreeCompositUpdateRLayers(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }

  for (bNode *node : ntree->all_nodes()) {
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
   * ideally render struct would store its own main AND original G_MAIN. */

  for (Scene *sce_iter = (Scene *)G_MAIN->scenes.first; sce_iter;
       sce_iter = (Scene *)sce_iter->id.next)
  {
    if (sce_iter->nodetree) {
      for (bNode *node : sce_iter->nodetree->all_nodes()) {
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

void ntreeCompositClearTags(bNodeTree *ntree)
{
  /* XXX: after render animation system gets a refresh, this call allows composite to end clean. */

  if (ntree == nullptr) {
    return;
  }

  for (bNode *node : ntree->all_nodes()) {
    node->runtime->need_exec = 0;
    if (node->type == NODE_GROUP) {
      ntreeCompositClearTags((bNodeTree *)node->id);
    }
  }
}

void ntreeCompositTagNeedExec(bNode *node)
{
  node->runtime->need_exec = true;
}
