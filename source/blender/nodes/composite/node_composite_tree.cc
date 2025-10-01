/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_tracking.h"

#include "UI_resources.hh"

#include "SEQ_modifier.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"

#include "node_common.h"

#include "RNA_prototypes.hh"

#include "NOD_composite.hh"
#include "node_composite_util.hh"

static void composite_get_from_context(const bContext *C,
                                       blender::bke::bNodeTreeType * /*treetype*/,
                                       bNodeTree **r_ntree,
                                       ID **r_id,
                                       ID **r_from)
{
  using namespace blender;
  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode->node_tree_sub_type == SNODE_COMPOSITOR_SEQUENCER) {
    Scene *sequencer_scene = CTX_data_sequencer_scene(C);
    if (!sequencer_scene) {
      *r_ntree = nullptr;
      return;
    }
    Editing *ed = seq::editing_get(sequencer_scene);
    if (!ed) {
      *r_ntree = nullptr;
      return;
    }
    Strip *strip = seq::select_active_get(sequencer_scene);
    if (!strip) {
      *r_ntree = nullptr;
      return;
    }
    StripModifierData *smd = seq::modifier_get_active(strip);
    if (!smd) {
      *r_ntree = nullptr;
      return;
    }
    if (smd->type != eSeqModifierType_Compositor) {
      *r_ntree = nullptr;
      return;
    }
    SequencerCompositorModifierData *scmd = reinterpret_cast<SequencerCompositorModifierData *>(
        smd);
    *r_from = nullptr;
    *r_id = &sequencer_scene->id;
    *r_ntree = scmd->node_group;
    return;
  }

  Scene *scene = CTX_data_scene(C);

  *r_from = nullptr;
  *r_id = &scene->id;
  *r_ntree = scene->compositing_node_group;
}

static void foreach_nodeclass(void *calldata, blender::bke::bNodeClassCallback func)
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
    /* right after #blender::bke::node_tree_copy_tree() `oldsock` pointers are valid */

    node = node->next;
    local_node = local_node->next;
  }
}

static void local_merge(Main * /*bmain*/, bNodeTree *localtree, bNodeTree *ntree)
{
  /* move over the compbufs and previews */
  blender::bke::node_preview_merge_tree(ntree, localtree, true);

  LISTBASE_FOREACH (bNode *, lnode, &localtree->nodes) {
    if (bNode *orig_node = blender::bke::node_find_node_by_name(*ntree, lnode->name)) {
      if (lnode->type_legacy == CMP_NODE_MOVIEDISTORTION) {
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
  blender::bke::node_tree_set_output(*ntree);

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

static bool composite_node_tree_socket_type_valid(blender::bke::bNodeTreeType * /*ntreetype*/,
                                                  blender::bke::bNodeSocketType *socket_type)
{
  return blender::bke::node_is_static_socket_type(*socket_type) && ELEM(socket_type->type,
                                                                        SOCK_FLOAT,
                                                                        SOCK_INT,
                                                                        SOCK_BOOLEAN,
                                                                        SOCK_VECTOR,
                                                                        SOCK_RGBA,
                                                                        SOCK_MENU,
                                                                        SOCK_STRING);
}

/**
 * Keep consistent with the #is_conversion_supported function in #compositor::ConversionOperation
 * on the compositor side.
 */
static bool composite_validate_link(eNodeSocketDatatype from_type, eNodeSocketDatatype to_type)
{
  /* Basic math types can be implicitly converted to each other. */
  if (ELEM(from_type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA, SOCK_BOOLEAN, SOCK_INT) &&
      ELEM(to_type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA, SOCK_BOOLEAN, SOCK_INT))
  {
    return true;
  }

  return from_type == to_type;
}

blender::bke::bNodeTreeType *ntreeType_Composite;

void register_node_tree_type_cmp()
{
  blender::bke::bNodeTreeType *tt = ntreeType_Composite = MEM_new<blender::bke::bNodeTreeType>(
      __func__);

  tt->type = NTREE_COMPOSIT;
  tt->idname = "CompositorNodeTree";
  tt->group_idname = "CompositorNodeGroup";
  tt->ui_name = N_("Compositor");
  tt->ui_icon = ICON_NODE_COMPOSITING;
  tt->ui_description = N_("Create effects and post-process renders, images, and the 3D Viewport");

  tt->foreach_nodeclass = foreach_nodeclass;
  tt->localize = localize;
  tt->local_merge = local_merge;
  tt->update = update;
  tt->get_from_context = composite_get_from_context;
  tt->node_add_init = composite_node_add_init;
  tt->validate_link = composite_validate_link;
  tt->valid_socket_type = composite_node_tree_socket_type_valid;

  tt->rna_ext.srna = &RNA_CompositorNodeTree;

  blender::bke::node_tree_type_add(*tt);
}

/* *********************************************** */

void ntreeCompositUpdateRLayers(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }

  for (bNode *node : ntree->all_nodes()) {
    if (node->type_legacy == CMP_NODE_R_LAYERS) {
      node_cmp_rlayers_outputs(ntree, node);
    }
    else if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
             node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER)
    {
      node->typeinfo->updatefunc(ntree, node);
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
    if (sce_iter->compositing_node_group) {
      for (bNode *node : sce_iter->compositing_node_group->all_nodes()) {
        if (node->id == (ID *)scene) {
          BKE_ntree_update_tag_node_property(sce_iter->compositing_node_group, node);
        }
      }
    }
  }
  BKE_ntree_update(*G_MAIN);
}

void ntreeCompositClearTags(bNodeTree *ntree)
{
  /* XXX: after render animation system gets a refresh, this call allows composite to end clean. */

  if (ntree == nullptr) {
    return;
  }

  for (bNode *node : ntree->all_nodes()) {
    node->runtime->need_exec = 0;
    if (node->is_group()) {
      ntreeCompositClearTags((bNodeTree *)node->id);
    }
  }
}

void ntreeCompositTagNeedExec(bNode *node)
{
  node->runtime->need_exec = true;
}
