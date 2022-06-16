/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup depsgraph
 *
 * Methods for constructing depsgraph's nodes
 */

#include "intern/builder/deg_builder_nodes.h"

#include <cstdio>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_key_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_simulation_types.h"
#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"
#include "DNA_texture_types.h"
#include "DNA_vfont_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_cachefile.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_effect.h"
#include "BKE_fcurve_driver.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_light.h"
#include "BKE_mask.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_node_runtime.hh"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"
#include "BKE_shader_fx.h"
#include "BKE_simulation.h"
#include "BKE_sound.h"
#include "BKE_tracking.h"
#include "BKE_volume.h"
#include "BKE_world.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"
#include "RNA_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "SEQ_iterator.h"
#include "SEQ_sequencer.h"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_rna.h"
#include "intern/depsgraph.h"
#include "intern/depsgraph_tag.h"
#include "intern/depsgraph_type.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

namespace blender::deg {

/* ************ */
/* Node Builder */

/* **** General purpose functions **** */

DepsgraphNodeBuilder::DepsgraphNodeBuilder(Main *bmain,
                                           Depsgraph *graph,
                                           DepsgraphBuilderCache *cache)
    : DepsgraphBuilder(bmain, graph, cache),
      scene_(nullptr),
      view_layer_(nullptr),
      view_layer_index_(-1),
      collection_(nullptr),
      is_parent_collection_visible_(true)
{
}

DepsgraphNodeBuilder::~DepsgraphNodeBuilder()
{
  for (IDInfo *id_info : id_info_hash_.values()) {
    if (id_info->id_cow != nullptr) {
      deg_free_copy_on_write_datablock(id_info->id_cow);
      MEM_freeN(id_info->id_cow);
    }
    MEM_freeN(id_info);
  }
}

IDNode *DepsgraphNodeBuilder::add_id_node(ID *id)
{
  BLI_assert(id->session_uuid != MAIN_ID_SESSION_UUID_UNSET);

  const ID_Type id_type = GS(id->name);
  IDNode *id_node = nullptr;
  ID *id_cow = nullptr;
  IDComponentsMask previously_visible_components_mask = 0;
  uint32_t previous_eval_flags = 0;
  DEGCustomDataMeshMasks previous_customdata_masks;
  IDInfo *id_info = id_info_hash_.lookup_default(id->session_uuid, nullptr);
  if (id_info != nullptr) {
    id_cow = id_info->id_cow;
    previously_visible_components_mask = id_info->previously_visible_components_mask;
    previous_eval_flags = id_info->previous_eval_flags;
    previous_customdata_masks = id_info->previous_customdata_masks;
    /* Tag ID info to not free the CoW ID pointer. */
    id_info->id_cow = nullptr;
  }
  id_node = graph_->add_id_node(id, id_cow);
  id_node->previously_visible_components_mask = previously_visible_components_mask;
  id_node->previous_eval_flags = previous_eval_flags;
  id_node->previous_customdata_masks = previous_customdata_masks;

  /* NOTE: Zero number of components indicates that ID node was just created. */
  const bool is_newly_created = id_node->components.is_empty();

  if (is_newly_created) {
    if (deg_copy_on_write_is_needed(id_type)) {
      ComponentNode *comp_cow = id_node->add_component(NodeType::COPY_ON_WRITE);
      OperationNode *op_cow = comp_cow->add_operation(
          [id_node](::Depsgraph *depsgraph) { deg_evaluate_copy_on_write(depsgraph, id_node); },
          OperationCode::COPY_ON_WRITE,
          "",
          -1);
      graph_->operations.append(op_cow);
    }

    ComponentNode *visibility_component = id_node->add_component(NodeType::VISIBILITY);
    OperationNode *visibility_operation = visibility_component->add_operation(
        nullptr, OperationCode::OPERATION, "", -1);
    /* Pin the node so that it and its relations are preserved by the unused nodes/relations
     * deletion. This is mainly to make it easier to debug visibility. */
    visibility_operation->flag |= OperationFlag::DEPSOP_FLAG_PINNED;
    graph_->operations.append(visibility_operation);
  }
  return id_node;
}

IDNode *DepsgraphNodeBuilder::find_id_node(ID *id)
{
  return graph_->find_id_node(id);
}

TimeSourceNode *DepsgraphNodeBuilder::add_time_source()
{
  return graph_->add_time_source();
}

ComponentNode *DepsgraphNodeBuilder::add_component_node(ID *id,
                                                        NodeType comp_type,
                                                        const char *comp_name)
{
  IDNode *id_node = add_id_node(id);
  ComponentNode *comp_node = id_node->add_component(comp_type, comp_name);
  comp_node->owner = id_node;
  return comp_node;
}

OperationNode *DepsgraphNodeBuilder::add_operation_node(ComponentNode *comp_node,
                                                        OperationCode opcode,
                                                        const DepsEvalOperationCb &op,
                                                        const char *name,
                                                        int name_tag)
{
  OperationNode *op_node = comp_node->find_operation(opcode, name, name_tag);
  if (op_node == nullptr) {
    op_node = comp_node->add_operation(op, opcode, name, name_tag);
    graph_->operations.append(op_node);
  }
  else {
    fprintf(stderr,
            "add_operation: Operation already exists - %s has %s at %p\n",
            comp_node->identifier().c_str(),
            op_node->identifier().c_str(),
            op_node);
    BLI_assert_msg(0, "Should not happen!");
  }
  return op_node;
}

OperationNode *DepsgraphNodeBuilder::add_operation_node(ID *id,
                                                        NodeType comp_type,
                                                        const char *comp_name,
                                                        OperationCode opcode,
                                                        const DepsEvalOperationCb &op,
                                                        const char *name,
                                                        int name_tag)
{
  ComponentNode *comp_node = add_component_node(id, comp_type, comp_name);
  return add_operation_node(comp_node, opcode, op, name, name_tag);
}

OperationNode *DepsgraphNodeBuilder::add_operation_node(ID *id,
                                                        NodeType comp_type,
                                                        OperationCode opcode,
                                                        const DepsEvalOperationCb &op,
                                                        const char *name,
                                                        int name_tag)
{
  return add_operation_node(id, comp_type, "", opcode, op, name, name_tag);
}

OperationNode *DepsgraphNodeBuilder::ensure_operation_node(ID *id,
                                                           NodeType comp_type,
                                                           const char *comp_name,
                                                           OperationCode opcode,
                                                           const DepsEvalOperationCb &op,
                                                           const char *name,
                                                           int name_tag)
{
  OperationNode *operation = find_operation_node(id, comp_type, comp_name, opcode, name, name_tag);
  if (operation != nullptr) {
    return operation;
  }
  return add_operation_node(id, comp_type, comp_name, opcode, op, name, name_tag);
}

OperationNode *DepsgraphNodeBuilder::ensure_operation_node(ID *id,
                                                           NodeType comp_type,
                                                           OperationCode opcode,
                                                           const DepsEvalOperationCb &op,
                                                           const char *name,
                                                           int name_tag)
{
  OperationNode *operation = find_operation_node(id, comp_type, opcode, name, name_tag);
  if (operation != nullptr) {
    return operation;
  }
  return add_operation_node(id, comp_type, opcode, op, name, name_tag);
}

bool DepsgraphNodeBuilder::has_operation_node(ID *id,
                                              NodeType comp_type,
                                              const char *comp_name,
                                              OperationCode opcode,
                                              const char *name,
                                              int name_tag)
{
  return find_operation_node(id, comp_type, comp_name, opcode, name, name_tag) != nullptr;
}

OperationNode *DepsgraphNodeBuilder::find_operation_node(ID *id,
                                                         NodeType comp_type,
                                                         const char *comp_name,
                                                         OperationCode opcode,
                                                         const char *name,
                                                         int name_tag)
{
  ComponentNode *comp_node = add_component_node(id, comp_type, comp_name);
  return comp_node->find_operation(opcode, name, name_tag);
}

OperationNode *DepsgraphNodeBuilder::find_operation_node(
    ID *id, NodeType comp_type, OperationCode opcode, const char *name, int name_tag)
{
  return find_operation_node(id, comp_type, "", opcode, name, name_tag);
}

ID *DepsgraphNodeBuilder::get_cow_id(const ID *id_orig) const
{
  return graph_->get_cow_id(id_orig);
}

ID *DepsgraphNodeBuilder::ensure_cow_id(ID *id_orig)
{
  if (id_orig->tag & LIB_TAG_COPIED_ON_WRITE) {
    /* ID is already remapped to copy-on-write. */
    return id_orig;
  }
  IDNode *id_node = add_id_node(id_orig);
  return id_node->id_cow;
}

/* **** Build functions for entity nodes **** */

void DepsgraphNodeBuilder::begin_build()
{
  /* Store existing copy-on-write versions of datablock, so we can re-use
   * them for new ID nodes. */
  for (IDNode *id_node : graph_->id_nodes) {
    /* It is possible that the ID does not need to have CoW version in which case id_cow is the
     * same as id_orig. Additionally, such ID might have been removed, which makes the check
     * for whether id_cow is expanded to access freed memory. In order to deal with this we
     * check whether CoW is needed based on a scalar value which does not lead to access of
     * possibly deleted memory. */
    IDInfo *id_info = (IDInfo *)MEM_mallocN(sizeof(IDInfo), "depsgraph id info");
    if (deg_copy_on_write_is_needed(id_node->id_type) &&
        deg_copy_on_write_is_expanded(id_node->id_cow) && id_node->id_orig != id_node->id_cow) {
      id_info->id_cow = id_node->id_cow;
    }
    else {
      id_info->id_cow = nullptr;
    }
    id_info->previously_visible_components_mask = id_node->visible_components_mask;
    id_info->previous_eval_flags = id_node->eval_flags;
    id_info->previous_customdata_masks = id_node->customdata_masks;
    BLI_assert(!id_info_hash_.contains(id_node->id_orig_session_uuid));
    id_info_hash_.add_new(id_node->id_orig_session_uuid, id_info);
    id_node->id_cow = nullptr;
  }

  for (OperationNode *op_node : graph_->entry_tags) {
    ComponentNode *comp_node = op_node->owner;
    IDNode *id_node = comp_node->owner;

    SavedEntryTag entry_tag;
    entry_tag.id_orig = id_node->id_orig;
    entry_tag.component_type = comp_node->type;
    entry_tag.opcode = op_node->opcode;
    entry_tag.name = op_node->name;
    entry_tag.name_tag = op_node->name_tag;
    saved_entry_tags_.append(entry_tag);
  }

  /* Make sure graph has no nodes left from previous state. */
  graph_->clear_all_nodes();
  graph_->operations.clear();
  graph_->entry_tags.clear();
}

/* Util callbacks for `BKE_library_foreach_ID_link`, used to detect when a COW ID is using ID
 * pointers that are either:
 *  - COW ID pointers that do not exist anymore in current depsgraph.
 *  - Orig ID pointers that do have now a COW version in current depsgraph.
 * In both cases, it means the COW ID user needs to be flushed, to ensure its pointers are properly
 * remapped.
 *
 * NOTE: This is split in two, a static function and a public method of the node builder, to allow
 * the code to access the builder's data more easily. */

int DepsgraphNodeBuilder::foreach_id_cow_detect_need_for_update_callback(ID *id_cow_self,
                                                                         ID *id_pointer)
{
  if (id_pointer->orig_id == nullptr) {
    /* `id_cow_self` uses a non-cow ID, if that ID has a COW copy in current depsgraph its owner
     * needs to be remapped, i.e. COW-flushed. */
    IDNode *id_node = find_id_node(id_pointer);
    if (id_node != nullptr && id_node->id_cow != nullptr) {
      graph_id_tag_update(bmain_,
                          graph_,
                          id_cow_self->orig_id,
                          ID_RECALC_COPY_ON_WRITE,
                          DEG_UPDATE_SOURCE_RELATIONS);
      return IDWALK_RET_STOP_ITER;
    }
  }
  else {
    /* `id_cow_self` uses a COW ID, if that COW copy is removed from current depsgraph its owner
     * needs to be remapped, i.e. COW-flushed. */
    /* NOTE: at that stage, old existing COW copies that are to be removed from current state of
     * evaluated depsgraph are still valid pointers, they are freed later (typically during
     * destruction of the builder itself). */
    IDNode *id_node = find_id_node(id_pointer->orig_id);
    if (id_node == nullptr) {
      graph_id_tag_update(bmain_,
                          graph_,
                          id_cow_self->orig_id,
                          ID_RECALC_COPY_ON_WRITE,
                          DEG_UPDATE_SOURCE_RELATIONS);
      return IDWALK_RET_STOP_ITER;
    }
  }
  return IDWALK_RET_NOP;
}

static int foreach_id_cow_detect_need_for_update_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID *id = *cb_data->id_pointer;
  if (id == nullptr) {
    return IDWALK_RET_NOP;
  }

  DepsgraphNodeBuilder *builder = static_cast<DepsgraphNodeBuilder *>(cb_data->user_data);
  ID *id_cow_self = cb_data->id_self;

  return builder->foreach_id_cow_detect_need_for_update_callback(id_cow_self, id);
}

void DepsgraphNodeBuilder::update_invalid_cow_pointers()
{
  /* NOTE: Currently the only ID types that depsgraph may decide to not evaluate/generate COW
   * copies for, even though they are referenced by other data-blocks, are Collections and Objects
   * (through their various visibility flags, and the ones from #LayerCollections too). However,
   * this code is kept generic as it makes it more future-proof, and optimization here would give
   * negligible performance improvements in typical cases.
   *
   * NOTE: This mechanism may also 'fix' some missing update tagging from non-depsgraph code in
   * some cases. This is slightly unfortunate (as it may hide issues in other parts of Blender
   * code), but cannot really be avoided currently. */

  for (const IDNode *id_node : graph_->id_nodes) {
    if (id_node->previously_visible_components_mask == 0) {
      /* Newly added node/ID, no need to check it. */
      continue;
    }
    if (ELEM(id_node->id_cow, id_node->id_orig, nullptr)) {
      /* Node/ID with no COW data, no need to check it. */
      continue;
    }
    if ((id_node->id_cow->recalc & ID_RECALC_COPY_ON_WRITE) != 0) {
      /* Node/ID already tagged for COW flush, no need to check it. */
      continue;
    }
    if ((id_node->id_cow->flag & LIB_EMBEDDED_DATA) != 0) {
      /* For now, we assume embedded data are managed by their owner IDs and do not need to be
       * checked here.
       *
       * NOTE: This exception somewhat weak, and ideally should not be needed. Currently however,
       * embedded data are handled as full local (private) data of their owner IDs in part of
       * Blender (like read/write code, including undo/redo), while depsgraph generally treat them
       * as regular independent IDs. This leads to inconsistencies that can lead to bad level
       * memory accesses.
       *
       * E.g. when undoing creation/deletion of a collection directly child of a scene's master
       * collection, the scene itself is re-read in place, but its master collection becomes a
       * completely new different pointer, and the existing COW of the old master collection in the
       * matching deg node is therefore pointing to fully invalid (freed) memory. */
      continue;
    }
    BKE_library_foreach_ID_link(nullptr,
                                id_node->id_cow,
                                deg::foreach_id_cow_detect_need_for_update_callback,
                                this,
                                IDWALK_IGNORE_EMBEDDED_ID | IDWALK_READONLY);
  }
}

void DepsgraphNodeBuilder::tag_previously_tagged_nodes()
{
  for (const SavedEntryTag &entry_tag : saved_entry_tags_) {
    IDNode *id_node = find_id_node(entry_tag.id_orig);
    if (id_node == nullptr) {
      continue;
    }
    ComponentNode *comp_node = id_node->find_component(entry_tag.component_type);
    if (comp_node == nullptr) {
      continue;
    }
    OperationNode *op_node = comp_node->find_operation(
        entry_tag.opcode, entry_tag.name.c_str(), entry_tag.name_tag);
    if (op_node == nullptr) {
      continue;
    }
    /* Since the tag is coming from a saved copy of entry tags, this means
     * that originally node was explicitly tagged for user update. */
    op_node->tag_update(graph_, DEG_UPDATE_SOURCE_USER_EDIT);
  }
}

void DepsgraphNodeBuilder::end_build()
{
  tag_previously_tagged_nodes();
  update_invalid_cow_pointers();
}

void DepsgraphNodeBuilder::build_id(ID *id)
{
  if (id == nullptr) {
    return;
  }

  const ID_Type id_type = GS(id->name);
  switch (id_type) {
    case ID_AC:
      build_action((bAction *)id);
      break;
    case ID_AR:
      build_armature((bArmature *)id);
      break;
    case ID_CA:
      build_camera((Camera *)id);
      break;
    case ID_GR:
      build_collection(nullptr, (Collection *)id);
      break;
    case ID_OB:
      /* TODO(sergey): Get visibility from a "parent" somehow.
       *
       * NOTE: Using `false` visibility here should be fine, since if this
       * driver affects on something invisible we don't really care if the
       * driver gets evaluated (and even don't want this to force object
       * to become visible).
       *
       * If this happened to be affecting visible object, then it is up to
       * deg_graph_build_flush_visibility() to ensure visibility of the
       * object is true. */
      build_object(-1, (Object *)id, DEG_ID_LINKED_INDIRECTLY, false);
      break;
    case ID_KE:
      build_shapekeys((Key *)id);
      break;
    case ID_LA:
      build_light((Light *)id);
      break;
    case ID_LP:
      build_lightprobe((LightProbe *)id);
      break;
    case ID_NT:
      build_nodetree((bNodeTree *)id);
      break;
    case ID_MA:
      build_material((Material *)id);
      break;
    case ID_TE:
      build_texture((Tex *)id);
      break;
    case ID_IM:
      build_image((Image *)id);
      break;
    case ID_WO:
      build_world((World *)id);
      break;
    case ID_MSK:
      build_mask((Mask *)id);
      break;
    case ID_LS:
      build_freestyle_linestyle((FreestyleLineStyle *)id);
      break;
    case ID_MC:
      build_movieclip((MovieClip *)id);
      break;
    case ID_ME:
    case ID_MB:
    case ID_CU_LEGACY:
    case ID_LT:
    case ID_GD:
    case ID_CV:
    case ID_PT:
    case ID_VO:
      build_object_data_geometry_datablock(id);
      break;
    case ID_SPK:
      build_speaker((Speaker *)id);
      break;
    case ID_SO:
      build_sound((bSound *)id);
      break;
    case ID_TXT:
      /* Not a part of dependency graph. */
      break;
    case ID_CF:
      build_cachefile((CacheFile *)id);
      break;
    case ID_SCE:
      build_scene_parameters((Scene *)id);
      break;
    case ID_SIM:
      build_simulation((Simulation *)id);
      break;
    case ID_PA:
      build_particle_settings((ParticleSettings *)id);
      break;

    case ID_LI:
    case ID_IP:
    case ID_SCR:
    case ID_VF:
    case ID_BR:
    case ID_WM:
    case ID_PAL:
    case ID_PC:
    case ID_WS:
      BLI_assert(!deg_copy_on_write_is_needed(id_type));
      build_generic_id(id);
      break;
  }
}

void DepsgraphNodeBuilder::build_generic_id(ID *id)
{
  if (built_map_.checkIsBuiltAndTag(id)) {
    return;
  }

  build_idproperties(id->properties);
  build_animdata(id);
  build_parameters(id);
}

static void build_idproperties_callback(IDProperty *id_property, void *user_data)
{
  DepsgraphNodeBuilder *builder = reinterpret_cast<DepsgraphNodeBuilder *>(user_data);
  BLI_assert(id_property->type == IDP_ID);
  builder->build_id(reinterpret_cast<ID *>(id_property->data.pointer));
}

void DepsgraphNodeBuilder::build_idproperties(IDProperty *id_property)
{
  IDP_foreach_property(id_property, IDP_TYPE_FILTER_ID, build_idproperties_callback, this);
}

void DepsgraphNodeBuilder::build_collection(LayerCollection *from_layer_collection,
                                            Collection *collection)
{
  const int visibility_flag = (graph_->mode == DAG_EVAL_VIEWPORT) ? COLLECTION_HIDE_VIEWPORT :
                                                                    COLLECTION_HIDE_RENDER;
  const bool is_collection_restricted = (collection->flag & visibility_flag);
  const bool is_collection_visible = !is_collection_restricted && is_parent_collection_visible_;
  IDNode *id_node;
  if (built_map_.checkIsBuiltAndTag(collection)) {
    id_node = find_id_node(&collection->id);
    if (is_collection_visible && id_node->is_directly_visible == false &&
        id_node->is_collection_fully_expanded == true) {
      /* Collection became visible, make sure nested collections and
       * objects are poked with the new visibility flag, since they
       * might become visible too. */
    }
    else if (from_layer_collection == nullptr && !id_node->is_collection_fully_expanded) {
      /* Initially collection was built from layer now, and was requested
       * to not recurse into object. But now it's asked to recurse into all objects. */
    }
    else {
      return;
    }
  }
  else {
    /* Collection itself. */
    id_node = add_id_node(&collection->id);
    id_node->is_directly_visible = is_collection_visible;

    build_idproperties(collection->id.properties);
    add_operation_node(&collection->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_DONE);
  }
  if (from_layer_collection != nullptr) {
    /* If we came from layer collection we don't go deeper, view layer
     * builder takes care of going deeper. */
    return;
  }
  /* Backup state. */
  Collection *current_state_collection = collection_;
  const bool is_current_parent_collection_visible = is_parent_collection_visible_;
  /* Modify state as we've entered new collection/ */
  collection_ = collection;
  is_parent_collection_visible_ = is_collection_visible;
  /* Build collection objects. */
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    build_object(-1, cob->ob, DEG_ID_LINKED_INDIRECTLY, is_collection_visible);
  }
  /* Build child collections. */
  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    build_collection(nullptr, child->collection);
  }
  /* Restore state. */
  collection_ = current_state_collection;
  is_parent_collection_visible_ = is_current_parent_collection_visible;
  id_node->is_collection_fully_expanded = true;
}

void DepsgraphNodeBuilder::build_object(int base_index,
                                        Object *object,
                                        eDepsNode_LinkedState_Type linked_state,
                                        bool is_visible)
{
  const bool has_object = built_map_.checkIsBuiltAndTag(object);

  /* When there is already object in the dependency graph accumulate visibility an linked state
   * flags. Only do it on the object itself (apart from very special cases) and leave dealing with
   * visibility of dependencies to the visibility flush step which happens at the end of the build
   * process. */
  if (has_object) {
    IDNode *id_node = find_id_node(&object->id);
    if (id_node->linked_state == DEG_ID_LINKED_INDIRECTLY) {
      build_object_flags(base_index, object, linked_state);
    }
    id_node->linked_state = max(id_node->linked_state, linked_state);
    id_node->is_directly_visible |= is_visible;
    id_node->has_base |= (base_index != -1);

    /* There is no relation path which will connect current object with all the ones which come
     * via the instanced collection, so build the collection again. Note that it will do check
     * whether visibility update is needed on its own. */
    build_object_instance_collection(object, is_visible);

    return;
  }

  /* Create ID node for object and begin init. */
  IDNode *id_node = add_id_node(&object->id);
  Object *object_cow = get_cow_datablock(object);
  id_node->linked_state = linked_state;
  /* NOTE: Scene is nullptr when building dependency graph for render pipeline.
   * Probably need to assign that to something non-nullptr, but then the logic here will still be
   * somewhat weird. */
  if (scene_ != nullptr && object == scene_->camera) {
    id_node->is_directly_visible = true;
  }
  else {
    id_node->is_directly_visible = is_visible;
  }
  id_node->has_base |= (base_index != -1);
  /* Various flags, flushing from bases/collections. */
  build_object_from_layer(base_index, object, linked_state);
  /* Transform. */
  build_object_transform(object);
  /* Parent. */
  if (object->parent != nullptr) {
    build_object(-1, object->parent, DEG_ID_LINKED_INDIRECTLY, is_visible);
  }
  /* Modifiers. */
  if (object->modifiers.first != nullptr) {
    BuilderWalkUserData data;
    data.builder = this;
    BKE_modifiers_foreach_ID_link(object, modifier_walk, &data);
  }
  /* Grease Pencil Modifiers. */
  if (object->greasepencil_modifiers.first != nullptr) {
    BuilderWalkUserData data;
    data.builder = this;
    BKE_gpencil_modifiers_foreach_ID_link(object, modifier_walk, &data);
  }
  /* Shader FX. */
  if (object->shader_fx.first != nullptr) {
    BuilderWalkUserData data;
    data.builder = this;
    BKE_shaderfx_foreach_ID_link(object, modifier_walk, &data);
  }
  /* Constraints. */
  if (object->constraints.first != nullptr) {
    BuilderWalkUserData data;
    data.builder = this;
    BKE_constraints_id_loop(&object->constraints, constraint_walk, &data);
  }
  /* Object data. */
  build_object_data(object);
  /* Parameters, used by both drivers/animation and also to inform dependency
   * from object's data. */
  build_parameters(&object->id);
  build_idproperties(object->id.properties);
  /* Build animation data,
   *
   * Do it now because it's possible object data will affect
   * on object's level animation, for example in case of rebuilding
   * pose for proxy. */
  build_animdata(&object->id);
  /* Particle systems. */
  if (object->particlesystem.first != nullptr) {
    build_particle_systems(object, is_visible);
  }
  /* Force field Texture. */
  if ((object->pd != nullptr) && (object->pd->forcefield == PFIELD_TEXTURE) &&
      (object->pd->tex != nullptr)) {
    build_texture(object->pd->tex);
  }
  /* Object dupligroup. */
  if (object->instance_collection != nullptr) {
    build_object_instance_collection(object, is_visible);
    OperationNode *op_node = add_operation_node(
        &object->id, NodeType::DUPLI, OperationCode::DUPLI);
    op_node->flag |= OperationFlag::DEPSOP_FLAG_PINNED;
  }
  /* Synchronization back to original object. */
  add_operation_node(&object->id,
                     NodeType::SYNCHRONIZATION,
                     OperationCode::SYNCHRONIZE_TO_ORIGINAL,
                     [object_cow](::Depsgraph *depsgraph) {
                       BKE_object_sync_to_original(depsgraph, object_cow);
                     });
}

void DepsgraphNodeBuilder::build_object_from_layer(int base_index,
                                                   Object *object,
                                                   eDepsNode_LinkedState_Type linked_state)
{

  OperationNode *entry_node = add_operation_node(
      &object->id, NodeType::OBJECT_FROM_LAYER, OperationCode::OBJECT_FROM_LAYER_ENTRY);
  entry_node->set_as_entry();
  OperationNode *exit_node = add_operation_node(
      &object->id, NodeType::OBJECT_FROM_LAYER, OperationCode::OBJECT_FROM_LAYER_EXIT);
  exit_node->set_as_exit();

  build_object_flags(base_index, object, linked_state);
}

void DepsgraphNodeBuilder::build_object_flags(int base_index,
                                              Object *object,
                                              eDepsNode_LinkedState_Type linked_state)
{
  if (base_index == -1) {
    return;
  }
  Scene *scene_cow = get_cow_datablock(scene_);
  Object *object_cow = get_cow_datablock(object);
  const bool is_from_set = (linked_state == DEG_ID_LINKED_VIA_SET);
  /* TODO(sergey): Is this really best component to be used? */
  add_operation_node(
      &object->id,
      NodeType::OBJECT_FROM_LAYER,
      OperationCode::OBJECT_BASE_FLAGS,
      [view_layer_index = view_layer_index_, scene_cow, object_cow, base_index, is_from_set](
          ::Depsgraph *depsgraph) {
        BKE_object_eval_eval_base_flags(
            depsgraph, scene_cow, view_layer_index, object_cow, base_index, is_from_set);
      });
}

void DepsgraphNodeBuilder::build_object_instance_collection(Object *object, bool is_object_visible)
{
  if (object->instance_collection == nullptr) {
    return;
  }
  const bool is_current_parent_collection_visible = is_parent_collection_visible_;
  is_parent_collection_visible_ = is_object_visible;
  build_collection(nullptr, object->instance_collection);
  is_parent_collection_visible_ = is_current_parent_collection_visible;
}

void DepsgraphNodeBuilder::build_object_data(Object *object)
{
  if (object->data == nullptr) {
    return;
  }
  /* type-specific data. */
  switch (object->type) {
    case OB_MESH:
    case OB_CURVES_LEGACY:
    case OB_FONT:
    case OB_SURF:
    case OB_MBALL:
    case OB_LATTICE:
    case OB_GPENCIL:
    case OB_CURVES:
    case OB_POINTCLOUD:
    case OB_VOLUME:
      build_object_data_geometry(object);
      break;
    case OB_ARMATURE:
      build_rig(object);
      break;
    case OB_LAMP:
      build_object_data_light(object);
      break;
    case OB_CAMERA:
      build_object_data_camera(object);
      break;
    case OB_LIGHTPROBE:
      build_object_data_lightprobe(object);
      break;
    case OB_SPEAKER:
      build_object_data_speaker(object);
      break;
    default: {
      ID *obdata = (ID *)object->data;
      if (!built_map_.checkIsBuilt(obdata)) {
        build_animdata(obdata);
      }
      break;
    }
  }
  /* Materials. */
  Material ***materials_ptr = BKE_object_material_array_p(object);
  if (materials_ptr != nullptr) {
    short *num_materials_ptr = BKE_object_material_len_p(object);
    build_materials(*materials_ptr, *num_materials_ptr);
  }
}

void DepsgraphNodeBuilder::build_object_data_camera(Object *object)
{
  Camera *camera = (Camera *)object->data;
  build_camera(camera);
}

void DepsgraphNodeBuilder::build_object_data_light(Object *object)
{
  Light *lamp = (Light *)object->data;
  build_light(lamp);
}

void DepsgraphNodeBuilder::build_object_data_lightprobe(Object *object)
{
  LightProbe *probe = (LightProbe *)object->data;
  build_lightprobe(probe);
  add_operation_node(&object->id, NodeType::PARAMETERS, OperationCode::LIGHT_PROBE_EVAL);
}

void DepsgraphNodeBuilder::build_object_data_speaker(Object *object)
{
  Speaker *speaker = (Speaker *)object->data;
  build_speaker(speaker);
  add_operation_node(&object->id, NodeType::AUDIO, OperationCode::SPEAKER_EVAL);
}

void DepsgraphNodeBuilder::build_object_transform(Object *object)
{
  OperationNode *op_node;
  Object *ob_cow = get_cow_datablock(object);
  /* Transform entry operation. */
  op_node = add_operation_node(&object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_INIT);
  op_node->set_as_entry();
  /* Local transforms (from transform channels - loc/rot/scale + deltas). */
  add_operation_node(
      &object->id,
      NodeType::TRANSFORM,
      OperationCode::TRANSFORM_LOCAL,
      [ob_cow](::Depsgraph *depsgraph) { BKE_object_eval_local_transform(depsgraph, ob_cow); });
  /* Object parent. */
  if (object->parent != nullptr) {
    add_operation_node(
        &object->id,
        NodeType::TRANSFORM,
        OperationCode::TRANSFORM_PARENT,
        [ob_cow](::Depsgraph *depsgraph) { BKE_object_eval_parent(depsgraph, ob_cow); });
  }
  /* Object constraints. */
  if (object->constraints.first != nullptr) {
    build_object_constraints(object);
  }
  /* Rest of transformation update. */
  add_operation_node(
      &object->id,
      NodeType::TRANSFORM,
      OperationCode::TRANSFORM_EVAL,
      [ob_cow](::Depsgraph *depsgraph) { BKE_object_eval_uber_transform(depsgraph, ob_cow); });
  /* Operation to take of rigid body simulation. soft bodies and other friends
   * in the context of point cache invalidation. */
  add_operation_node(&object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_SIMULATION_INIT);
  /* Object transform is done. */
  op_node = add_operation_node(
      &object->id,
      NodeType::TRANSFORM,
      OperationCode::TRANSFORM_FINAL,
      [ob_cow](::Depsgraph *depsgraph) { BKE_object_eval_transform_final(depsgraph, ob_cow); });
  op_node->set_as_exit();
}

/**
 * Constraints Graph Notes
 *
 * For constraints, we currently only add a operation node to the Transform
 * or Bone components (depending on whichever type of owner we have).
 * This represents the entire constraints stack, which is for now just
 * executed as a single monolithic block. At least initially, this should
 * be sufficient for ensuring that the porting/refactoring process remains
 * manageable.
 *
 * However, when the time comes for developing "node-based" constraints,
 * we'll need to split this up into pre/post nodes for "constraint stack
 * evaluation" + operation nodes for each constraint (i.e. the contents
 * of the loop body used in the current "solve_constraints()" operation).
 *
 * -- Aligorith, August 2013
 */
void DepsgraphNodeBuilder::build_object_constraints(Object *object)
{
  /* create node for constraint stack */
  Scene *scene_cow = get_cow_datablock(scene_);
  Object *object_cow = get_cow_datablock(object);
  add_operation_node(&object->id,
                     NodeType::TRANSFORM,
                     OperationCode::TRANSFORM_CONSTRAINTS,
                     [scene_cow, object_cow](::Depsgraph *depsgraph) {
                       BKE_object_eval_constraints(depsgraph, scene_cow, object_cow);
                     });
}

void DepsgraphNodeBuilder::build_object_pointcache(Object *object)
{
  if (!BKE_ptcache_object_has(scene_, object, 0)) {
    return;
  }
  Scene *scene_cow = get_cow_datablock(scene_);
  Object *object_cow = get_cow_datablock(object);
  add_operation_node(&object->id,
                     NodeType::POINT_CACHE,
                     OperationCode::POINT_CACHE_RESET,
                     [scene_cow, object_cow](::Depsgraph *depsgraph) {
                       BKE_object_eval_ptcache_reset(depsgraph, scene_cow, object_cow);
                     });
}

void DepsgraphNodeBuilder::build_animdata(ID *id)
{
  /* Special handling for animated images/sequences. */
  build_animation_images(id);
  /* Regular animation. */
  AnimData *adt = BKE_animdata_from_id(id);
  if (adt == nullptr) {
    return;
  }
  if (adt->action != nullptr) {
    build_action(adt->action);
  }
  /* Make sure ID node exists. */
  (void)add_id_node(id);
  ID *id_cow = get_cow_id(id);
  if (adt->action != nullptr || !BLI_listbase_is_empty(&adt->nla_tracks)) {
    OperationNode *operation_node;
    /* Explicit entry operation. */
    operation_node = add_operation_node(id, NodeType::ANIMATION, OperationCode::ANIMATION_ENTRY);
    operation_node->set_as_entry();
    /* All the evaluation nodes. */
    add_operation_node(
        id, NodeType::ANIMATION, OperationCode::ANIMATION_EVAL, [id_cow](::Depsgraph *depsgraph) {
          BKE_animsys_eval_animdata(depsgraph, id_cow);
        });
    /* Explicit exit operation. */
    operation_node = add_operation_node(id, NodeType::ANIMATION, OperationCode::ANIMATION_EXIT);
    operation_node->set_as_exit();
  }
  /* NLA strips contain actions. */
  LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
    build_animdata_nlastrip_targets(&nlt->strips);
  }
  /* Drivers. */
  int driver_index = 0;
  LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
    /* create driver */
    build_driver(id, fcu, driver_index++);
  }
}

void DepsgraphNodeBuilder::build_animdata_nlastrip_targets(ListBase *strips)
{
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    if (strip->act != nullptr) {
      build_action(strip->act);
    }
    else if (strip->strips.first != nullptr) {
      build_animdata_nlastrip_targets(&strip->strips);
    }
  }
}

void DepsgraphNodeBuilder::build_animation_images(ID *id)
{
  /* GPU materials might use an animated image. However, these materials have no been built yet so
   * we have to check if they might be created during evaluation. */
  bool has_image_animation = false;
  if (ELEM(GS(id->name), ID_MA, ID_WO)) {
    bNodeTree *ntree = *BKE_ntree_ptr_from_id(id);
    if (ntree != nullptr &&
        ntree->runtime->runtime_flag & NTREE_RUNTIME_FLAG_HAS_IMAGE_ANIMATION) {
      has_image_animation = true;
    }
  }

  if (has_image_animation || BKE_image_user_id_has_animation(id)) {
    ID *id_cow = get_cow_id(id);
    add_operation_node(
        id,
        NodeType::IMAGE_ANIMATION,
        OperationCode::IMAGE_ANIMATION,
        [id_cow](::Depsgraph *depsgraph) { BKE_image_user_id_eval_animation(depsgraph, id_cow); });
  }
}

void DepsgraphNodeBuilder::build_action(bAction *action)
{
  if (built_map_.checkIsBuiltAndTag(action)) {
    return;
  }
  build_idproperties(action->id.properties);
  add_operation_node(&action->id, NodeType::ANIMATION, OperationCode::ANIMATION_EVAL);
}

void DepsgraphNodeBuilder::build_driver(ID *id, FCurve *fcurve, int driver_index)
{
  /* Create data node for this driver */
  ID *id_cow = get_cow_id(id);

  /* TODO(sergey): ideally we could pass the COW of fcu, but since it
   * has not yet been allocated at this point we can't. As a workaround
   * the animation systems allocates an array so we can do a fast lookup
   * with the driver index. */
  ensure_operation_node(
      id,
      NodeType::PARAMETERS,
      OperationCode::DRIVER,
      [id_cow, driver_index, fcurve](::Depsgraph *depsgraph) {
        BKE_animsys_eval_driver(depsgraph, id_cow, driver_index, fcurve);
      },
      fcurve->rna_path ? fcurve->rna_path : "",
      fcurve->array_index);
  build_driver_variables(id, fcurve);
}

void DepsgraphNodeBuilder::build_driver_variables(ID *id, FCurve *fcurve)
{
  build_driver_id_property(id, fcurve->rna_path);
  LISTBASE_FOREACH (DriverVar *, dvar, &fcurve->driver->variables) {
    DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
      if (dtar->id == nullptr) {
        continue;
      }
      build_id(dtar->id);
      build_driver_id_property(dtar->id, dtar->rna_path);
    }
    DRIVER_TARGETS_LOOPER_END;
  }
}

void DepsgraphNodeBuilder::build_driver_id_property(ID *id, const char *rna_path)
{
  if (id == nullptr || rna_path == nullptr) {
    return;
  }
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;
  int index;
  RNA_id_pointer_create(id, &id_ptr);
  if (!RNA_path_resolve_full(&id_ptr, rna_path, &ptr, &prop, &index)) {
    return;
  }
  if (prop == nullptr) {
    return;
  }
  if (!rna_prop_affects_parameters_node(&ptr, prop)) {
    return;
  }
  const char *prop_identifier = RNA_property_identifier((PropertyRNA *)prop);
  /* Custom properties of bones are placed in their components to improve granularity. */
  if (RNA_struct_is_a(ptr.type, &RNA_PoseBone)) {
    const bPoseChannel *pchan = static_cast<const bPoseChannel *>(ptr.data);
    ensure_operation_node(
        id, NodeType::BONE, pchan->name, OperationCode::ID_PROPERTY, nullptr, prop_identifier);
  }
  else {
    ensure_operation_node(
        id, NodeType::PARAMETERS, OperationCode::ID_PROPERTY, nullptr, prop_identifier);
  }
}

void DepsgraphNodeBuilder::build_parameters(ID *id)
{
  (void)add_id_node(id);
  OperationNode *op_node;
  /* Explicit entry. */
  op_node = add_operation_node(id, NodeType::PARAMETERS, OperationCode::PARAMETERS_ENTRY);
  op_node->set_as_entry();
  /* Generic evaluation node. */

  if (ID_TYPE_SUPPORTS_PARAMS_WITHOUT_COW(GS(id->name))) {
    ID *id_cow = get_cow_id(id);
    add_operation_node(
        id,
        NodeType::PARAMETERS,
        OperationCode::PARAMETERS_EVAL,
        [id_cow, id](::Depsgraph * /*depsgraph*/) { BKE_id_eval_properties_copy(id_cow, id); });
  }
  else {
    add_operation_node(id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL);
  }

  /* Explicit exit operation. */
  op_node = add_operation_node(id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EXIT);
  op_node->set_as_exit();
}

void DepsgraphNodeBuilder::build_dimensions(Object *object)
{
  /* Object dimensions (bounding box) node. Will depend on both geometry and transform. */
  add_operation_node(&object->id, NodeType::PARAMETERS, OperationCode::DIMENSIONS);
}

/* Recursively build graph for world */
void DepsgraphNodeBuilder::build_world(World *world)
{
  if (built_map_.checkIsBuiltAndTag(world)) {
    return;
  }
  /* World itself. */
  add_id_node(&world->id);
  World *world_cow = get_cow_datablock(world);
  /* Shading update. */
  add_operation_node(
      &world->id,
      NodeType::SHADING,
      OperationCode::WORLD_UPDATE,
      [world_cow](::Depsgraph *depsgraph) { BKE_world_eval(depsgraph, world_cow); });
  build_idproperties(world->id.properties);
  /* Animation. */
  build_animdata(&world->id);
  build_parameters(&world->id);
  /* World's nodetree. */
  build_nodetree(world->nodetree);
}

/* Rigidbody Simulation - Scene Level */
void DepsgraphNodeBuilder::build_rigidbody(Scene *scene)
{
  RigidBodyWorld *rbw = scene->rigidbody_world;
  Scene *scene_cow = get_cow_datablock(scene);

  /**
   * Rigidbody Simulation Nodes
   * ==========================
   *
   * There are 3 nodes related to Rigidbody Simulation:
   * 1) "Initialize/Rebuild World" - this is called sparingly, only when the
   *    simulation needs to be rebuilt (mainly after file reload, or moving
   *    back to start frame)
   * 2) "Do Simulation" - perform a simulation step - interleaved between the
   *    evaluation steps for clusters of objects (i.e. between those affected
   *    and/or not affected by the sim for instance).
   *
   * 3) "Pull Results" - grab the specific transforms applied for a specific
   *    object - performed as part of object's transform-stack building. */

  /* Create nodes --------------------------------------------------------- */

  /* XXX: is this the right component, or do we want to use another one
   * instead? */

  /* Init/rebuild operation. */
  add_operation_node(
      &scene->id,
      NodeType::TRANSFORM,
      OperationCode::RIGIDBODY_REBUILD,
      [scene_cow](::Depsgraph *depsgraph) { BKE_rigidbody_rebuild_sim(depsgraph, scene_cow); });
  /* Do-sim operation. */
  OperationNode *sim_node = add_operation_node(&scene->id,
                                               NodeType::TRANSFORM,
                                               OperationCode::RIGIDBODY_SIM,
                                               [scene_cow](::Depsgraph *depsgraph) {
                                                 BKE_rigidbody_eval_simulation(depsgraph,
                                                                               scene_cow);
                                               });
  sim_node->set_as_entry();
  sim_node->set_as_exit();
  sim_node->owner->entry_operation = sim_node;
  /* Objects - simulation participants. */
  if (rbw->group != nullptr) {
    build_collection(nullptr, rbw->group);
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, object) {
      if (object->type != OB_MESH) {
        continue;
      }
      if (object->rigidbody_object == nullptr) {
        continue;
      }

      if (object->rigidbody_object->type == RBO_TYPE_PASSIVE) {
        continue;
      }

      /* Create operation for flushing results. */
      /* Object's transform component - where the rigidbody operation
       * lives. */
      Object *object_cow = get_cow_datablock(object);
      add_operation_node(&object->id,
                         NodeType::TRANSFORM,
                         OperationCode::RIGIDBODY_TRANSFORM_COPY,
                         [scene_cow, object_cow](::Depsgraph *depsgraph) {
                           BKE_rigidbody_object_sync_transforms(depsgraph, scene_cow, object_cow);
                         });
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
  /* Constraints. */
  if (rbw->constraints != nullptr) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->constraints, object) {
      RigidBodyCon *rbc = object->rigidbody_constraint;
      if (rbc == nullptr || rbc->ob1 == nullptr || rbc->ob2 == nullptr) {
        /* When either ob1 or ob2 is nullptr, the constraint doesn't work. */
        continue;
      }
      /* Make sure indirectly linked objects are fully built. */
      build_object(-1, object, DEG_ID_LINKED_INDIRECTLY, false);
      build_object(-1, rbc->ob1, DEG_ID_LINKED_INDIRECTLY, false);
      build_object(-1, rbc->ob2, DEG_ID_LINKED_INDIRECTLY, false);
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
}

void DepsgraphNodeBuilder::build_particle_systems(Object *object, bool is_object_visible)
{
  /**
   * Particle Systems Nodes
   * ======================
   *
   * There are two types of nodes associated with representing
   * particle systems:
   *  1) Component (EVAL_PARTICLES) - This is the particle-system
   *     evaluation context for an object. It acts as the container
   *     for all the nodes associated with a particular set of particle
   *     systems.
   *  2) Particle System Evaluation Operation - This operation node acts as a
   *     black-box evaluation step for one particle system referenced by
   *     the particle systems stack. All dependencies link to this operation. */
  /* Component for all particle systems. */
  ComponentNode *psys_comp = add_component_node(&object->id, NodeType::PARTICLE_SYSTEM);

  Object *ob_cow = get_cow_datablock(object);
  OperationNode *op_node;
  op_node = add_operation_node(
      psys_comp, OperationCode::PARTICLE_SYSTEM_INIT, [ob_cow](::Depsgraph *depsgraph) {
        BKE_particle_system_eval_init(depsgraph, ob_cow);
      });
  op_node->set_as_entry();
  /* Build all particle systems. */
  LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
    ParticleSettings *part = psys->part;
    /* Build particle settings operations.
     *
     * NOTE: The call itself ensures settings are only build once. */
    build_particle_settings(part);
    /* Particle system evaluation. */
    add_operation_node(psys_comp, OperationCode::PARTICLE_SYSTEM_EVAL, nullptr, psys->name);
    /* Keyed particle targets. */
    if (ELEM(part->phystype, PART_PHYS_KEYED, PART_PHYS_BOIDS)) {
      LISTBASE_FOREACH (ParticleTarget *, particle_target, &psys->targets) {
        if (ELEM(particle_target->ob, nullptr, object)) {
          continue;
        }
        build_object(-1, particle_target->ob, DEG_ID_LINKED_INDIRECTLY, is_object_visible);
      }
    }
    /* Visualization of particle system. */
    switch (part->ren_as) {
      case PART_DRAW_OB:
        if (part->instance_object != nullptr) {
          build_object(-1, part->instance_object, DEG_ID_LINKED_INDIRECTLY, is_object_visible);
        }
        break;
      case PART_DRAW_GR:
        if (part->instance_collection != nullptr) {
          build_collection(nullptr, part->instance_collection);
        }
        break;
    }
  }
  op_node = add_operation_node(psys_comp, OperationCode::PARTICLE_SYSTEM_DONE);
  op_node->set_as_exit();
}

void DepsgraphNodeBuilder::build_particle_settings(ParticleSettings *particle_settings)
{
  if (built_map_.checkIsBuiltAndTag(particle_settings)) {
    return;
  }
  /* Make sure we've got proper copied ID pointer. */
  add_id_node(&particle_settings->id);
  ParticleSettings *particle_settings_cow = get_cow_datablock(particle_settings);
  /* Animation data. */
  build_animdata(&particle_settings->id);
  build_parameters(&particle_settings->id);
  /* Parameters change. */
  OperationNode *op_node;
  op_node = add_operation_node(
      &particle_settings->id, NodeType::PARTICLE_SETTINGS, OperationCode::PARTICLE_SETTINGS_INIT);
  op_node->set_as_entry();
  add_operation_node(&particle_settings->id,
                     NodeType::PARTICLE_SETTINGS,
                     OperationCode::PARTICLE_SETTINGS_RESET,
                     [particle_settings_cow](::Depsgraph *depsgraph) {
                       BKE_particle_settings_eval_reset(depsgraph, particle_settings_cow);
                     });
  op_node = add_operation_node(
      &particle_settings->id, NodeType::PARTICLE_SETTINGS, OperationCode::PARTICLE_SETTINGS_EVAL);
  op_node->set_as_exit();
  /* Texture slots. */
  for (MTex *mtex : particle_settings->mtex) {
    if (mtex == nullptr || mtex->tex == nullptr) {
      continue;
    }
    build_texture(mtex->tex);
  }
}

/* Shape-keys. */
void DepsgraphNodeBuilder::build_shapekeys(Key *key)
{
  if (built_map_.checkIsBuiltAndTag(key)) {
    return;
  }
  build_idproperties(key->id.properties);
  build_animdata(&key->id);
  build_parameters(&key->id);
  /* This is an exit operation for the entire key datablock, is what is used
   * as dependency for modifiers evaluation. */
  add_operation_node(&key->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_SHAPEKEY);
  /* Create per-key block properties, allowing tricky inter-dependencies for
   * drivers evaluation. */
  LISTBASE_FOREACH (KeyBlock *, key_block, &key->block) {
    add_operation_node(
        &key->id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL, nullptr, key_block->name);
  }
}

/* ObData Geometry Evaluation */
/* XXX: what happens if the datablock is shared! */
void DepsgraphNodeBuilder::build_object_data_geometry(Object *object)
{
  OperationNode *op_node;
  Scene *scene_cow = get_cow_datablock(scene_);
  Object *object_cow = get_cow_datablock(object);
  /* Entry operation, takes care of initialization, and some other
   * relations which needs to be run prior actual geometry evaluation. */
  op_node = add_operation_node(&object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_INIT);
  op_node->set_as_entry();
  /* Geometry evaluation. */
  op_node = add_operation_node(&object->id,
                               NodeType::GEOMETRY,
                               OperationCode::GEOMETRY_EVAL,
                               [scene_cow, object_cow](::Depsgraph *depsgraph) {
                                 BKE_object_eval_uber_data(depsgraph, scene_cow, object_cow);
                               });
  op_node->set_as_exit();
  /* Geometry caching. */
  op_node = add_operation_node(&object->id,
                               NodeType::GEOMETRY,
                               OperationCode::GEOMETRY_WRITE_CACHE,
                               [scene_cow, object_cow](::Depsgraph *depsgraph) {
                                 BKE_object_write_geometry_cache(depsgraph, scene_cow, object_cow);
                               });
  op_node->set_as_exit();
  /* Materials. */
  build_materials(object->mat, object->totcol);
  /* Point caches. */
  build_object_pointcache(object);
  /* Geometry. */
  build_object_data_geometry_datablock((ID *)object->data);
  build_dimensions(object);
  /* Batch cache. */
  add_operation_node(
      &object->id,
      NodeType::BATCH_CACHE,
      OperationCode::GEOMETRY_SELECT_UPDATE,
      [object_cow](::Depsgraph *depsgraph) { BKE_object_select_update(depsgraph, object_cow); });
}

void DepsgraphNodeBuilder::build_object_data_geometry_datablock(ID *obdata)
{
  if (built_map_.checkIsBuiltAndTag(obdata)) {
    return;
  }
  OperationNode *op_node;
  /* Make sure we've got an ID node before requesting CoW pointer. */
  (void)add_id_node((ID *)obdata);
  ID *obdata_cow = get_cow_id(obdata);
  build_idproperties(obdata->properties);
  /* Animation. */
  build_animdata(obdata);
  /* ShapeKeys */
  Key *key = BKE_key_from_id(obdata);
  if (key) {
    build_shapekeys(key);
  }
  /* Nodes for result of obdata's evaluation, and geometry
   * evaluation on object. */
  const ID_Type id_type = GS(obdata->name);
  switch (id_type) {
    case ID_ME: {
      op_node = add_operation_node(obdata,
                                   NodeType::GEOMETRY,
                                   OperationCode::GEOMETRY_EVAL,
                                   [obdata_cow](::Depsgraph *depsgraph) {
                                     BKE_mesh_eval_geometry(depsgraph, (Mesh *)obdata_cow);
                                   });
      op_node->set_as_entry();
      break;
    }
    case ID_MB: {
      op_node = add_operation_node(obdata, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
      op_node->set_as_entry();
      break;
    }
    case ID_CU_LEGACY: {
      op_node = add_operation_node(obdata,
                                   NodeType::GEOMETRY,
                                   OperationCode::GEOMETRY_EVAL,
                                   [obdata_cow](::Depsgraph *depsgraph) {
                                     BKE_curve_eval_geometry(depsgraph, (Curve *)obdata_cow);
                                   });
      op_node->set_as_entry();
      Curve *cu = (Curve *)obdata;
      if (cu->bevobj != nullptr) {
        build_object(-1, cu->bevobj, DEG_ID_LINKED_INDIRECTLY, false);
      }
      if (cu->taperobj != nullptr) {
        build_object(-1, cu->taperobj, DEG_ID_LINKED_INDIRECTLY, false);
      }
      if (cu->textoncurve != nullptr) {
        build_object(-1, cu->textoncurve, DEG_ID_LINKED_INDIRECTLY, false);
      }
      break;
    }
    case ID_LT: {
      op_node = add_operation_node(obdata,
                                   NodeType::GEOMETRY,
                                   OperationCode::GEOMETRY_EVAL,
                                   [obdata_cow](::Depsgraph *depsgraph) {
                                     BKE_lattice_eval_geometry(depsgraph, (Lattice *)obdata_cow);
                                   });
      op_node->set_as_entry();
      break;
    }

    case ID_GD: {
      /* GPencil evaluation operations. */
      op_node = add_operation_node(obdata,
                                   NodeType::GEOMETRY,
                                   OperationCode::GEOMETRY_EVAL,
                                   [obdata_cow](::Depsgraph *depsgraph) {
                                     BKE_gpencil_frame_active_set(depsgraph,
                                                                  (bGPdata *)obdata_cow);
                                   });
      op_node->set_as_entry();
      break;
    }
    case ID_CV: {
      op_node = add_operation_node(obdata, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
      op_node->set_as_entry();
      break;
    }
    case ID_PT: {
      op_node = add_operation_node(obdata, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
      op_node->set_as_entry();
      break;
    }
    case ID_VO: {
      /* Volume frame update. */
      op_node = add_operation_node(obdata,
                                   NodeType::GEOMETRY,
                                   OperationCode::GEOMETRY_EVAL,
                                   [obdata_cow](::Depsgraph *depsgraph) {
                                     BKE_volume_eval_geometry(depsgraph, (Volume *)obdata_cow);
                                   });
      op_node->set_as_entry();
      break;
    }
    default:
      BLI_assert_msg(0, "Should not happen");
      break;
  }
  op_node = add_operation_node(obdata, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_DONE);
  op_node->set_as_exit();
  /* Parameters for driver sources. */
  build_parameters(obdata);
  /* Batch cache. */
  add_operation_node(obdata,
                     NodeType::BATCH_CACHE,
                     OperationCode::GEOMETRY_SELECT_UPDATE,
                     [obdata_cow](::Depsgraph *depsgraph) {
                       BKE_object_data_select_update(depsgraph, obdata_cow);
                     });
}

void DepsgraphNodeBuilder::build_armature(bArmature *armature)
{
  if (built_map_.checkIsBuiltAndTag(armature)) {
    return;
  }
  build_idproperties(armature->id.properties);
  build_animdata(&armature->id);
  build_parameters(&armature->id);
  /* Make sure pose is up-to-date with armature updates. */
  bArmature *armature_cow = (bArmature *)get_cow_id(&armature->id);
  add_operation_node(&armature->id,
                     NodeType::ARMATURE,
                     OperationCode::ARMATURE_EVAL,
                     [armature_cow](::Depsgraph *depsgraph) {
                       BKE_armature_refresh_layer_used(depsgraph, armature_cow);
                     });
  build_armature_bones(&armature->bonebase);
}

void DepsgraphNodeBuilder::build_armature_bones(ListBase *bones)
{
  LISTBASE_FOREACH (Bone *, bone, bones) {
    build_idproperties(bone->prop);
    build_armature_bones(&bone->childbase);
  }
}

void DepsgraphNodeBuilder::build_camera(Camera *camera)
{
  if (built_map_.checkIsBuiltAndTag(camera)) {
    return;
  }
  build_idproperties(camera->id.properties);
  build_animdata(&camera->id);
  build_parameters(&camera->id);
  if (camera->dof.focus_object != nullptr) {
    build_object(-1, camera->dof.focus_object, DEG_ID_LINKED_INDIRECTLY, false);
  }
}

void DepsgraphNodeBuilder::build_light(Light *lamp)
{
  if (built_map_.checkIsBuiltAndTag(lamp)) {
    return;
  }
  build_idproperties(lamp->id.properties);
  build_animdata(&lamp->id);
  build_parameters(&lamp->id);
  /* light's nodetree */
  build_nodetree(lamp->nodetree);

  Light *lamp_cow = get_cow_datablock(lamp);
  add_operation_node(&lamp->id,
                     NodeType::SHADING,
                     OperationCode::LIGHT_UPDATE,
                     [lamp_cow](::Depsgraph *depsgraph) { BKE_light_eval(depsgraph, lamp_cow); });
}

void DepsgraphNodeBuilder::build_nodetree_socket(bNodeSocket *socket)
{
  build_idproperties(socket->prop);

  if (socket->type == SOCK_OBJECT) {
    build_id((ID *)((bNodeSocketValueObject *)socket->default_value)->value);
  }
  else if (socket->type == SOCK_IMAGE) {
    build_id((ID *)((bNodeSocketValueImage *)socket->default_value)->value);
  }
  else if (socket->type == SOCK_COLLECTION) {
    build_id((ID *)((bNodeSocketValueCollection *)socket->default_value)->value);
  }
  else if (socket->type == SOCK_TEXTURE) {
    build_id((ID *)((bNodeSocketValueTexture *)socket->default_value)->value);
  }
  else if (socket->type == SOCK_MATERIAL) {
    build_id((ID *)((bNodeSocketValueMaterial *)socket->default_value)->value);
  }
}

void DepsgraphNodeBuilder::build_nodetree(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }
  if (built_map_.checkIsBuiltAndTag(ntree)) {
    return;
  }
  /* nodetree itself */
  add_id_node(&ntree->id);
  /* General parameters. */
  build_parameters(&ntree->id);
  build_idproperties(ntree->id.properties);
  /* Animation, */
  build_animdata(&ntree->id);
  /* Output update. */
  add_operation_node(&ntree->id, NodeType::NTREE_OUTPUT, OperationCode::NTREE_OUTPUT);
  /* nodetree's nodes... */
  LISTBASE_FOREACH (bNode *, bnode, &ntree->nodes) {
    build_idproperties(bnode->prop);
    LISTBASE_FOREACH (bNodeSocket *, socket, &bnode->inputs) {
      build_nodetree_socket(socket);
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &bnode->outputs) {
      build_nodetree_socket(socket);
    }

    ID *id = bnode->id;
    if (id == nullptr) {
      continue;
    }
    ID_Type id_type = GS(id->name);
    if (id_type == ID_MA) {
      build_material((Material *)id);
    }
    else if (id_type == ID_TE) {
      build_texture((Tex *)id);
    }
    else if (id_type == ID_IM) {
      build_image((Image *)id);
    }
    else if (id_type == ID_OB) {
      /* TODO(sergey): Use visibility of owner of the node tree. */
      build_object(-1, (Object *)id, DEG_ID_LINKED_INDIRECTLY, true);
    }
    else if (id_type == ID_SCE) {
      Scene *node_scene = (Scene *)id;
      build_scene_parameters(node_scene);
      /* Camera is used by defocus node.
       *
       * On the one hand it's annoying to always pull it in, but on another hand it's also annoying
       * to have hardcoded node-type exception here. */
      if (node_scene->camera != nullptr) {
        /* TODO(sergey): Use visibility of owner of the node tree. */
        build_object(-1, node_scene->camera, DEG_ID_LINKED_INDIRECTLY, true);
      }
    }
    else if (id_type == ID_TXT) {
      /* Ignore script nodes. */
    }
    else if (id_type == ID_MSK) {
      build_mask((Mask *)id);
    }
    else if (id_type == ID_MC) {
      build_movieclip((MovieClip *)id);
    }
    else if (id_type == ID_VF) {
      build_vfont((VFont *)id);
    }
    else if (ELEM(bnode->type, NODE_GROUP, NODE_CUSTOM_GROUP)) {
      bNodeTree *group_ntree = (bNodeTree *)id;
      build_nodetree(group_ntree);
    }
    else {
      BLI_assert_msg(0, "Unknown ID type used for node");
    }
  }

  LISTBASE_FOREACH (bNodeSocket *, socket, &ntree->inputs) {
    build_idproperties(socket->prop);
  }
  LISTBASE_FOREACH (bNodeSocket *, socket, &ntree->outputs) {
    build_idproperties(socket->prop);
  }

  /* TODO: link from nodetree to owner_component? */
}

/* Recursively build graph for material */
void DepsgraphNodeBuilder::build_material(Material *material)
{
  if (built_map_.checkIsBuiltAndTag(material)) {
    return;
  }
  /* Material itself. */
  add_id_node(&material->id);
  Material *material_cow = get_cow_datablock(material);
  /* Shading update. */
  add_operation_node(
      &material->id,
      NodeType::SHADING,
      OperationCode::MATERIAL_UPDATE,
      [material_cow](::Depsgraph *depsgraph) { BKE_material_eval(depsgraph, material_cow); });
  build_idproperties(material->id.properties);
  /* Material animation. */
  build_animdata(&material->id);
  build_parameters(&material->id);
  /* Material's nodetree. */
  build_nodetree(material->nodetree);
}

void DepsgraphNodeBuilder::build_materials(Material **materials, int num_materials)
{
  for (int i = 0; i < num_materials; i++) {
    if (materials[i] == nullptr) {
      continue;
    }
    build_material(materials[i]);
  }
}

/* Recursively build graph for texture */
void DepsgraphNodeBuilder::build_texture(Tex *texture)
{
  if (built_map_.checkIsBuiltAndTag(texture)) {
    return;
  }
  /* Texture itself. */
  add_id_node(&texture->id);
  build_idproperties(texture->id.properties);
  build_animdata(&texture->id);
  build_parameters(&texture->id);
  /* Texture's nodetree. */
  build_nodetree(texture->nodetree);
  /* Special cases for different IDs which texture uses. */
  if (texture->type == TEX_IMAGE) {
    if (texture->ima != nullptr) {
      build_image(texture->ima);
    }
  }
  add_operation_node(
      &texture->id, NodeType::GENERIC_DATABLOCK, OperationCode::GENERIC_DATABLOCK_UPDATE);
}

void DepsgraphNodeBuilder::build_image(Image *image)
{
  if (built_map_.checkIsBuiltAndTag(image)) {
    return;
  }
  build_parameters(&image->id);
  build_idproperties(image->id.properties);
  add_operation_node(
      &image->id, NodeType::GENERIC_DATABLOCK, OperationCode::GENERIC_DATABLOCK_UPDATE);
}

void DepsgraphNodeBuilder::build_cachefile(CacheFile *cache_file)
{
  if (built_map_.checkIsBuiltAndTag(cache_file)) {
    return;
  }
  ID *cache_file_id = &cache_file->id;
  add_id_node(cache_file_id);
  CacheFile *cache_file_cow = get_cow_datablock(cache_file);
  build_idproperties(cache_file_id->properties);
  /* Animation, */
  build_animdata(cache_file_id);
  build_parameters(cache_file_id);
  /* Cache evaluation itself. */
  add_operation_node(cache_file_id,
                     NodeType::CACHE,
                     OperationCode::FILE_CACHE_UPDATE,
                     [bmain = bmain_, cache_file_cow](::Depsgraph *depsgraph) {
                       BKE_cachefile_eval(bmain, depsgraph, cache_file_cow);
                     });
}

void DepsgraphNodeBuilder::build_mask(Mask *mask)
{
  if (built_map_.checkIsBuiltAndTag(mask)) {
    return;
  }
  ID *mask_id = &mask->id;
  Mask *mask_cow = (Mask *)ensure_cow_id(mask_id);
  build_idproperties(mask->id.properties);
  /* F-Curve based animation. */
  build_animdata(mask_id);
  build_parameters(mask_id);
  /* Animation based on mask's shapes. */
  add_operation_node(
      mask_id,
      NodeType::ANIMATION,
      OperationCode::MASK_ANIMATION,
      [mask_cow](::Depsgraph *depsgraph) { BKE_mask_eval_animation(depsgraph, mask_cow); });
  /* Final mask evaluation. */
  add_operation_node(
      mask_id, NodeType::PARAMETERS, OperationCode::MASK_EVAL, [mask_cow](::Depsgraph *depsgraph) {
        BKE_mask_eval_update(depsgraph, mask_cow);
      });
  /* Build parents. */
  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask->masklayers) {
    LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
      for (int i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];
        MaskParent *parent = &point->parent;
        if (parent == nullptr || parent->id == nullptr) {
          continue;
        }
        build_id(parent->id);
      }
    }
  }
}

void DepsgraphNodeBuilder::build_freestyle_linestyle(FreestyleLineStyle *linestyle)
{
  if (built_map_.checkIsBuiltAndTag(linestyle)) {
    return;
  }

  ID *linestyle_id = &linestyle->id;
  build_parameters(linestyle_id);
  build_idproperties(linestyle->id.properties);
  build_animdata(linestyle_id);
  build_nodetree(linestyle->nodetree);
}

void DepsgraphNodeBuilder::build_movieclip(MovieClip *clip)
{
  if (built_map_.checkIsBuiltAndTag(clip)) {
    return;
  }
  ID *clip_id = &clip->id;
  MovieClip *clip_cow = (MovieClip *)ensure_cow_id(clip_id);
  build_idproperties(clip_id->properties);
  /* Animation. */
  build_animdata(clip_id);
  build_parameters(clip_id);
  /* Movie clip evaluation. */
  add_operation_node(clip_id,
                     NodeType::PARAMETERS,
                     OperationCode::MOVIECLIP_EVAL,
                     [bmain = bmain_, clip_cow](::Depsgraph *depsgraph) {
                       BKE_movieclip_eval_update(depsgraph, bmain, clip_cow);
                     });

  add_operation_node(clip_id,
                     NodeType::BATCH_CACHE,
                     OperationCode::MOVIECLIP_SELECT_UPDATE,
                     [clip_cow](::Depsgraph *depsgraph) {
                       BKE_movieclip_eval_selection_update(depsgraph, clip_cow);
                     });
}

void DepsgraphNodeBuilder::build_lightprobe(LightProbe *probe)
{
  if (built_map_.checkIsBuiltAndTag(probe)) {
    return;
  }
  /* Placeholder so we can add relations and tag ID node for update. */
  add_operation_node(&probe->id, NodeType::PARAMETERS, OperationCode::LIGHT_PROBE_EVAL);
  build_idproperties(probe->id.properties);
  build_animdata(&probe->id);
  build_parameters(&probe->id);
}

void DepsgraphNodeBuilder::build_speaker(Speaker *speaker)
{
  if (built_map_.checkIsBuiltAndTag(speaker)) {
    return;
  }
  /* Placeholder so we can add relations and tag ID node for update. */
  add_operation_node(&speaker->id, NodeType::AUDIO, OperationCode::SPEAKER_EVAL);
  build_idproperties(speaker->id.properties);
  build_animdata(&speaker->id);
  build_parameters(&speaker->id);
  if (speaker->sound != nullptr) {
    build_sound(speaker->sound);
  }
}

void DepsgraphNodeBuilder::build_sound(bSound *sound)
{
  if (built_map_.checkIsBuiltAndTag(sound)) {
    return;
  }
  add_id_node(&sound->id);
  bSound *sound_cow = get_cow_datablock(sound);
  add_operation_node(&sound->id,
                     NodeType::AUDIO,
                     OperationCode::SOUND_EVAL,
                     [bmain = bmain_, sound_cow](::Depsgraph *depsgraph) {
                       BKE_sound_evaluate(depsgraph, bmain, sound_cow);
                     });
  build_idproperties(sound->id.properties);
  build_animdata(&sound->id);
  build_parameters(&sound->id);
}

void DepsgraphNodeBuilder::build_simulation(Simulation *simulation)
{
  if (built_map_.checkIsBuiltAndTag(simulation)) {
    return;
  }
  add_id_node(&simulation->id);
  build_idproperties(simulation->id.properties);
  build_animdata(&simulation->id);
  build_parameters(&simulation->id);
  build_nodetree(simulation->nodetree);

  Simulation *simulation_cow = get_cow_datablock(simulation);
  Scene *scene_cow = get_cow_datablock(scene_);

  add_operation_node(&simulation->id,
                     NodeType::SIMULATION,
                     OperationCode::SIMULATION_EVAL,
                     [scene_cow, simulation_cow](::Depsgraph *depsgraph) {
                       BKE_simulation_data_update(depsgraph, scene_cow, simulation_cow);
                     });
}

void DepsgraphNodeBuilder::build_vfont(VFont *vfont)
{
  if (built_map_.checkIsBuiltAndTag(vfont)) {
    return;
  }
  build_parameters(&vfont->id);
  build_idproperties(vfont->id.properties);
  add_operation_node(
      &vfont->id, NodeType::GENERIC_DATABLOCK, OperationCode::GENERIC_DATABLOCK_UPDATE);
}

static bool seq_node_build_cb(Sequence *seq, void *user_data)
{
  DepsgraphNodeBuilder *nb = (DepsgraphNodeBuilder *)user_data;
  nb->build_idproperties(seq->prop);
  if (seq->sound != nullptr) {
    nb->build_sound(seq->sound);
  }
  if (seq->scene != nullptr) {
    nb->build_scene_parameters(seq->scene);
  }
  if (seq->type == SEQ_TYPE_SCENE && seq->scene != nullptr) {
    if (seq->flag & SEQ_SCENE_STRIPS) {
      nb->build_scene_sequencer(seq->scene);
    }
    ViewLayer *sequence_view_layer = BKE_view_layer_default_render(seq->scene);
    nb->build_scene_speakers(seq->scene, sequence_view_layer);
  }
  /* TODO(sergey): Movie clip, scene, camera, mask. */
  return true;
}

void DepsgraphNodeBuilder::build_scene_sequencer(Scene *scene)
{
  if (scene->ed == nullptr) {
    return;
  }
  if (built_map_.checkIsBuiltAndTag(scene, BuilderMap::TAG_SCENE_SEQUENCER)) {
    return;
  }
  build_scene_audio(scene);
  Scene *scene_cow = get_cow_datablock(scene);
  add_operation_node(&scene->id,
                     NodeType::SEQUENCER,
                     OperationCode::SEQUENCES_EVAL,
                     [scene_cow](::Depsgraph *depsgraph) {
                       SEQ_eval_sequences(depsgraph, scene_cow, &scene_cow->ed->seqbase);
                     });
  /* Make sure data for sequences is in the graph. */
  SEQ_for_each_callback(&scene->ed->seqbase, seq_node_build_cb, this);
}

void DepsgraphNodeBuilder::build_scene_audio(Scene *scene)
{
  if (built_map_.checkIsBuiltAndTag(scene, BuilderMap::TAG_SCENE_AUDIO)) {
    return;
  }

  OperationNode *audio_entry_node = add_operation_node(
      &scene->id, NodeType::AUDIO, OperationCode::AUDIO_ENTRY);
  audio_entry_node->set_as_entry();

  add_operation_node(&scene->id, NodeType::AUDIO, OperationCode::SOUND_EVAL);

  Scene *scene_cow = get_cow_datablock(scene);
  add_operation_node(&scene->id,
                     NodeType::AUDIO,
                     OperationCode::AUDIO_VOLUME,
                     [scene_cow](::Depsgraph *depsgraph) {
                       BKE_scene_update_tag_audio_volume(depsgraph, scene_cow);
                     });
}

void DepsgraphNodeBuilder::build_scene_speakers(Scene * /*scene*/, ViewLayer *view_layer)
{
  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    Object *object = base->object;
    if (object->type != OB_SPEAKER || !need_pull_base_into_graph(base)) {
      continue;
    }
    /* NOTE: Can not use base because it does not belong to a current view layer. */
    build_object(-1, base->object, DEG_ID_LINKED_INDIRECTLY, true);
  }
}

/* **** ID traversal callbacks functions **** */

void DepsgraphNodeBuilder::modifier_walk(void *user_data,
                                         struct Object * /*object*/,
                                         struct ID **idpoin,
                                         int /*cb_flag*/)
{
  BuilderWalkUserData *data = (BuilderWalkUserData *)user_data;
  ID *id = *idpoin;
  if (id == nullptr) {
    return;
  }
  switch (GS(id->name)) {
    case ID_OB:
      data->builder->build_object(-1, (Object *)id, DEG_ID_LINKED_INDIRECTLY, false);
      break;
    default:
      data->builder->build_id(id);
      break;
  }
}

void DepsgraphNodeBuilder::constraint_walk(bConstraint * /*con*/,
                                           ID **idpoin,
                                           bool /*is_reference*/,
                                           void *user_data)
{
  BuilderWalkUserData *data = (BuilderWalkUserData *)user_data;
  ID *id = *idpoin;
  if (id == nullptr) {
    return;
  }
  switch (GS(id->name)) {
    case ID_OB:
      data->builder->build_object(-1, (Object *)id, DEG_ID_LINKED_INDIRECTLY, false);
      break;
    default:
      data->builder->build_id(id);
      break;
  }
}

}  // namespace blender::deg
