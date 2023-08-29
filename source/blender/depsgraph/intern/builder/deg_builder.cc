/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder.h"

#include <cstring>

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_layer_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_stack.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_collection.h"

#include "RNA_prototypes.h"

#include "intern/builder/deg_builder_cache.h"
#include "intern/builder/deg_builder_remove_noop.h"
#include "intern/depsgraph.h"
#include "intern/depsgraph_relation.h"
#include "intern/depsgraph_tag.h"
#include "intern/depsgraph_type.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/eval/deg_eval_visibility.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

#include "DEG_depsgraph.h"

namespace blender::deg {

bool deg_check_id_in_depsgraph(const Depsgraph *graph, ID *id_orig)
{
  IDNode *id_node = graph->find_id_node(id_orig);
  return id_node != nullptr;
}

bool deg_check_base_in_depsgraph(const Depsgraph *graph, Base *base)
{
  Object *object_orig = base->base_orig->object;
  IDNode *id_node = graph->find_id_node(&object_orig->id);
  if (id_node == nullptr) {
    return false;
  }
  return id_node->has_base;
}

/* -------------------------------------------------------------------- */
/** \name Base Class for Builders
 * \{ */

DepsgraphBuilder::DepsgraphBuilder(Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache)
    : bmain_(bmain), graph_(graph), cache_(cache)
{
}

bool DepsgraphBuilder::need_pull_base_into_graph(const Base *base)
{
  /* Simple check: enabled bases are always part of dependency graph. */
  const int base_flag = (graph_->mode == DAG_EVAL_VIEWPORT) ? BASE_ENABLED_VIEWPORT :
                                                              BASE_ENABLED_RENDER;
  if (base->flag & base_flag) {
    return true;
  }

  /* More involved check: since we don't support dynamic changes in dependency graph topology and
   * all visible objects are to be part of dependency graph, we pull all objects which has animated
   * visibility. */
  return is_object_visibility_animated(base->object);
}

bool DepsgraphBuilder::is_object_visibility_animated(const Object *object)
{
  AnimatedPropertyID property_id;
  if (graph_->mode == DAG_EVAL_VIEWPORT) {
    property_id = AnimatedPropertyID(&object->id, &RNA_Object, "hide_viewport");
  }
  else if (graph_->mode == DAG_EVAL_RENDER) {
    property_id = AnimatedPropertyID(&object->id, &RNA_Object, "hide_render");
  }
  else {
    BLI_assert_msg(0, "Unknown evaluation mode.");
    return false;
  }
  return cache_->isPropertyAnimated(&object->id, property_id);
}

bool DepsgraphBuilder::is_modifier_visibility_animated(const Object *object,
                                                       const ModifierData *modifier)
{
  AnimatedPropertyID property_id;
  if (graph_->mode == DAG_EVAL_VIEWPORT) {
    property_id = AnimatedPropertyID(
        &object->id, &RNA_Modifier, (void *)modifier, "show_viewport");
  }
  else if (graph_->mode == DAG_EVAL_RENDER) {
    property_id = AnimatedPropertyID(&object->id, &RNA_Modifier, (void *)modifier, "show_render");
  }
  else {
    BLI_assert_msg(0, "Unknown evaluation mode.");
    return false;
  }
  return cache_->isPropertyAnimated(&object->id, property_id);
}

bool DepsgraphBuilder::check_pchan_has_bbone(const Object *object, const bPoseChannel *pchan)
{
  BLI_assert(object->type == OB_ARMATURE);
  if (pchan == nullptr || pchan->bone == nullptr) {
    return false;
  }
  /* We don't really care whether segments are higher than 1 due to static user input (as in,
   * rigger entered value like 3 manually), or due to animation. In either way we need to create
   * special evaluation. */
  if (pchan->bone->segments > 1) {
    return true;
  }
  bArmature *armature = static_cast<bArmature *>(object->data);
  AnimatedPropertyID property_id(&armature->id, &RNA_Bone, pchan->bone, "bbone_segments");
  /* Check both Object and Armature animation data, because drivers modifying Armature
   * state could easily be created in the Object AnimData. */
  return cache_->isPropertyAnimated(&object->id, property_id) ||
         cache_->isPropertyAnimated(&armature->id, property_id);
}

bool DepsgraphBuilder::check_pchan_has_bbone_segments(const Object *object,
                                                      const bPoseChannel *pchan)
{
  return check_pchan_has_bbone(object, pchan);
}

bool DepsgraphBuilder::check_pchan_has_bbone_segments(const Object *object, const char *bone_name)
{
  const bPoseChannel *pchan = BKE_pose_channel_find_name(object->pose, bone_name);
  return check_pchan_has_bbone_segments(object, pchan);
}

const char *DepsgraphBuilder::get_rna_path_relative_to_scene_camera(const Scene *scene,
                                                                    const PointerRNA &target_prop,
                                                                    const char *rna_path)
{
  if (rna_path == nullptr || target_prop.data != scene || target_prop.type != &RNA_Scene ||
      !BLI_str_startswith(rna_path, "camera"))
  {
    return nullptr;
  }

  /* Return the part of the path relative to the camera. */
  switch (rna_path[6]) {
    case '.':
      return rna_path + 7;
    case '[':
      return rna_path + 6;
    default:
      return nullptr;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Builder Finalizer.
 * \{ */

void deg_graph_build_finalize(Main *bmain, Depsgraph *graph)
{
  deg_graph_flush_visibility_flags(graph);
  deg_graph_remove_unused_noops(graph);

  /* Re-tag IDs for update if it was tagged before the relations
   * update tag. */
  for (IDNode *id_node : graph->id_nodes) {
    ID *id_orig = id_node->id_orig;
    id_node->finalize_build(graph);
    int flag = 0;
    /* Tag rebuild if special evaluation flags changed. */
    if (id_node->eval_flags != id_node->previous_eval_flags) {
      flag |= ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY;
    }
    /* Tag rebuild if the custom data mask changed. */
    if (id_node->customdata_masks != id_node->previous_customdata_masks) {
      flag |= ID_RECALC_GEOMETRY;
    }
    if (!deg_copy_on_write_is_expanded(id_node->id_cow)) {
      flag |= ID_RECALC_COPY_ON_WRITE;
      /* This means ID is being added to the dependency graph first
       * time, which is similar to "ob-visible-change" */
      if (GS(id_orig->name) == ID_OB) {
        flag |= ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY;
      }
      if (GS(id_orig->name) == ID_NT) {
        flag |= ID_RECALC_NTREE_OUTPUT;
      }
    }
    else {
      /* Collection content might have changed (children collection might have been added or
       * removed from the graph based on their inclusion and visibility flags). */
      const ID_Type id_type = GS(id_node->id_cow->name);
      if (id_type == ID_GR) {
        BKE_collection_object_cache_free(reinterpret_cast<Collection *>(id_node->id_cow));
      }
    }
    /* Restore recalc flags from original ID, which could possibly contain recalc flags set by
     * an operator and then were carried on by the undo system. */
    flag |= id_orig->recalc;
    if (flag != 0) {
      graph_id_tag_update(bmain, graph, id_node->id_orig, flag, DEG_UPDATE_SOURCE_RELATIONS);
    }
  }
}

/** \} */

}  // namespace blender::deg
