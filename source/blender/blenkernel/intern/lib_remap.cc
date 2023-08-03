/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Contains management of ID's for remapping.
 */

#include "CLG_log.h"

#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"

#include "BKE_armature.h"
#include "BKE_collection.h"
#include "BKE_curve.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_multires.hh"
#include "BKE_node.h"
#include "BKE_node_tree_update.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "lib_intern.h" /* own include */

static CLG_LogRef LOG = {"bke.lib_remap"};

BKE_library_free_notifier_reference_cb free_notifier_reference_cb = nullptr;

void BKE_library_callback_free_notifier_reference_set(BKE_library_free_notifier_reference_cb func)
{
  free_notifier_reference_cb = func;
}

BKE_library_remap_editor_id_reference_cb remap_editor_id_reference_cb = nullptr;

void BKE_library_callback_remap_editor_id_reference_set(
    BKE_library_remap_editor_id_reference_cb func)
{
  remap_editor_id_reference_cb = func;
}

struct IDRemap {
  eIDRemapType type;
  Main *bmain; /* Only used to trigger depsgraph updates in the right bmain. */

  IDRemapper *id_remapper;

  /** The ID in which we are replacing old_id by new_id usages. */
  ID *id_owner;
  int flag;
};

/* IDRemap->flag enums defined in BKE_lib.h */

static void foreach_libblock_remap_callback_skip(const ID * /*id_owner*/,
                                                 ID **id_ptr,
                                                 const int cb_flag,
                                                 const bool is_indirect,
                                                 const bool is_reference,
                                                 const bool violates_never_null,
                                                 const bool /*is_obj*/,
                                                 const bool is_obj_editmode)
{
  ID *id = *id_ptr;
  BLI_assert(id != nullptr);

  if (is_indirect) {
    id->runtime.remap.skipped_indirect++;
  }
  else if (violates_never_null || is_obj_editmode || is_reference) {
    id->runtime.remap.skipped_direct++;
  }
  else {
    BLI_assert_unreachable();
  }

  if (cb_flag & IDWALK_CB_USER) {
    id->runtime.remap.skipped_refcounted++;
  }
  else if (cb_flag & IDWALK_CB_USER_ONE) {
    /* No need to count number of times this happens, just a flag is enough. */
    id->runtime.remap.status |= ID_REMAP_IS_USER_ONE_SKIPPED;
  }
}

static void foreach_libblock_remap_callback_apply(ID *id_owner,
                                                  ID *id_self,
                                                  ID **id_ptr,
                                                  IDRemap *id_remap_data,
                                                  const IDRemapper *mappings,
                                                  const IDRemapperApplyOptions id_remapper_options,
                                                  const int cb_flag,
                                                  const bool is_indirect,
                                                  const bool violates_never_null)
{
  const bool skip_update_tagging = (id_remap_data->flag & ID_REMAP_SKIP_UPDATE_TAGGING) != 0;
  const bool skip_user_refcount = (id_remap_data->flag & ID_REMAP_SKIP_USER_REFCOUNT) != 0;
  const bool force_user_refcount = (id_remap_data->flag & ID_REMAP_FORCE_USER_REFCOUNT) != 0;
  BLI_assert(!skip_user_refcount || !force_user_refcount);

  ID *old_id = *id_ptr;
  if (!violates_never_null) {
    BKE_id_remapper_apply_ex(mappings, id_ptr, id_remapper_options, id_self);
    if (!skip_update_tagging) {
      if (id_remap_data->bmain != nullptr) {
        DEG_id_tag_update_ex(id_remap_data->bmain,
                             id_self,
                             ID_RECALC_COPY_ON_WRITE | ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
        if (id_self != id_owner) {
          DEG_id_tag_update_ex(id_remap_data->bmain,
                               id_owner,
                               ID_RECALC_COPY_ON_WRITE | ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
        }
      }
      if (GS(id_owner->name) == ID_NT) {
        /* Make sure that the node tree is updated after a property in it changed. Ideally, we
         * would know which nodes property was changed so that only this node is tagged. */
        BKE_ntree_update_tag_all((bNodeTree *)id_owner);
      }
    }
  }
  /* Get the new_id pointer. When the mapping is violating never null we should use a nullptr
   * pointer otherwise the incorrect users are decreased and increased on the same instance. */
  ID *new_id = violates_never_null ? nullptr : *id_ptr;

  if (!is_indirect && new_id) {
    new_id->runtime.remap.status |= ID_REMAP_IS_LINKED_DIRECT;
  }

  if (skip_user_refcount) {
    return;
  }

  if (cb_flag & IDWALK_CB_USER) {
    /* NOTE: by default we don't user-count IDs which are not in the main database.
     * This is because in certain conditions we can have data-blocks in
     * the main which are referencing data-blocks outside of it.
     * For example, BKE_mesh_new_from_object() called on an evaluated
     * object will cause such situation.
     */
    if (force_user_refcount || (old_id->tag & LIB_TAG_NO_MAIN) == 0) {
      id_us_min(old_id);
    }
    if (new_id != nullptr && (force_user_refcount || (new_id->tag & LIB_TAG_NO_MAIN) == 0)) {
      /* Do not handle LIB_TAG_INDIRECT/LIB_TAG_EXTERN here. */
      id_us_plus_no_lib(new_id);
    }
  }
  else if (cb_flag & IDWALK_CB_USER_ONE) {
    id_us_ensure_real(new_id);
    /* We cannot affect old_id->us directly, LIB_TAG_EXTRAUSER(_SET)
     * are assumed to be set as needed, that extra user is processed in final handling. */
  }
}

static int foreach_libblock_remap_callback(LibraryIDLinkCallbackData *cb_data)
{
  const int cb_flag = cb_data->cb_flag;

  /* NOTE: Support remapping of `IDWALK_CB_EMBEDDED_NON_OWNING` pointers, this is necessary in some
   * complex low-level ID manipulation cases (e.g. in ID swapping, see #BKE_lib_id_swap & co).
   */
  if (cb_flag & IDWALK_CB_EMBEDDED) {
    return IDWALK_RET_NOP;
  }

  ID *id_owner = cb_data->owner_id;
  ID *id_self = cb_data->self_id;
  ID **id_p = cb_data->id_pointer;
  IDRemap *id_remap_data = static_cast<IDRemap *>(cb_data->user_data);

  /* Those asserts ensure the general sanity of ID tags regarding 'embedded' ID data (root
   * node-trees and co). */
  BLI_assert(id_owner == id_remap_data->id_owner);
  BLI_assert(id_self == id_owner || (id_self->flag & LIB_EMBEDDED_DATA) != 0);

  /* Early exit when id pointer isn't set. */
  if (*id_p == nullptr) {
    return IDWALK_RET_NOP;
  }

  IDRemapper *id_remapper = id_remap_data->id_remapper;
  IDRemapperApplyOptions id_remapper_options = ID_REMAP_APPLY_DEFAULT;

  /* Used to cleanup all IDs used by a specific one. */
  if (id_remap_data->type == ID_REMAP_TYPE_CLEANUP) {
    /* Clearing existing instance to reduce potential lookup times for IDs referencing many other
     * IDs. This makes sure that there will only be a single rule in the id_remapper. */
    BKE_id_remapper_clear(id_remapper);
    BKE_id_remapper_add(id_remapper, *id_p, nullptr);
  }

  /* Better remap to nullptr than not remapping at all,
   * then we can handle it as a regular remap-to-nullptr case. */
  if (cb_flag & IDWALK_CB_NEVER_SELF) {
    id_remapper_options |= ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF;
  }

  const IDRemapperApplyResult expected_mapping_result = BKE_id_remapper_get_mapping_result(
      id_remapper, *id_p, id_remapper_options, id_self);
  /* Exit, when no modifications will be done; ensuring id->runtime counters won't changed. */
  if (ELEM(expected_mapping_result,
           ID_REMAP_RESULT_SOURCE_UNAVAILABLE,
           ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE))
  {
    BLI_assert_msg(id_remap_data->type == ID_REMAP_TYPE_REMAP,
                   "Cleanup should always do unassign.");
    return IDWALK_RET_NOP;
  }

  const bool is_reference = (cb_flag & IDWALK_CB_OVERRIDE_LIBRARY_REFERENCE) != 0;
  const bool is_indirect = (cb_flag & IDWALK_CB_INDIRECT_USAGE) != 0;
  const bool skip_indirect = (id_remap_data->flag & ID_REMAP_SKIP_INDIRECT_USAGE) != 0;
  const bool is_obj = (GS(id_owner->name) == ID_OB);
  /* NOTE: Edit Mode is a 'skip direct' case, unless specifically requested, obdata should not be
   * remapped in this situation. */
  const bool is_obj_editmode = (is_obj && BKE_object_is_in_editmode((Object *)id_owner) &&
                                (id_remap_data->flag & ID_REMAP_FORCE_OBDATA_IN_EDITMODE) == 0);
  const bool violates_never_null = ((cb_flag & IDWALK_CB_NEVER_NULL) &&
                                    (expected_mapping_result ==
                                     ID_REMAP_RESULT_SOURCE_UNASSIGNED) &&
                                    (id_remap_data->flag & ID_REMAP_FORCE_NEVER_NULL_USAGE) == 0);
  const bool skip_reference = (id_remap_data->flag & ID_REMAP_SKIP_OVERRIDE_LIBRARY) != 0;
  const bool skip_never_null = (id_remap_data->flag & ID_REMAP_SKIP_NEVER_NULL_USAGE) != 0;

#ifdef DEBUG_PRINT
  printf(
      "In %s (lib %p): Remapping %s (%p) remap operation: %s "
      "(is_indirect: %d, skip_indirect: %d, is_reference: %d, skip_reference: %d)\n",
      id_owner->name,
      id_owner->lib,
      (*id_p)->name,
      *id_p,
      BKE_id_remapper_result_string(expected_mapping_result),
      is_indirect,
      skip_indirect,
      is_reference,
      skip_reference);
#endif

  if ((id_remap_data->flag & ID_REMAP_FLAG_NEVER_NULL_USAGE) && (cb_flag & IDWALK_CB_NEVER_NULL)) {
    id_owner->tag |= LIB_TAG_DOIT;
  }

  /* Special hack in case it's Object->data and we are in edit mode, and new_id is not nullptr
   * (otherwise, we follow common NEVER_NULL flags).
   * (skipped_indirect too). */
  if ((violates_never_null && skip_never_null) ||
      (is_obj_editmode && (((Object *)id_owner)->data == *id_p) &&
       (expected_mapping_result == ID_REMAP_RESULT_SOURCE_REMAPPED)) ||
      (skip_indirect && is_indirect) || (is_reference && skip_reference))
  {
    foreach_libblock_remap_callback_skip(id_owner,
                                         id_p,
                                         cb_flag,
                                         is_indirect,
                                         is_reference,
                                         violates_never_null,
                                         is_obj,
                                         is_obj_editmode);
  }
  else {
    foreach_libblock_remap_callback_apply(id_owner,
                                          id_self,
                                          id_p,
                                          id_remap_data,
                                          id_remapper,
                                          id_remapper_options,
                                          cb_flag,
                                          is_indirect,
                                          violates_never_null);
  }

  return IDWALK_RET_NOP;
}

static void libblock_remap_data_preprocess_ob(Object *ob,
                                              eIDRemapType remap_type,
                                              const IDRemapper *id_remapper)
{
  if (ob->type != OB_ARMATURE) {
    return;
  }
  if (ob->pose == nullptr) {
    return;
  }

  const bool is_cleanup_type = remap_type == ID_REMAP_TYPE_CLEANUP;
  /* Early exit when mapping, but no armature mappings present. */
  if (!is_cleanup_type && !BKE_id_remapper_has_mapping_for(id_remapper, FILTER_ID_AR)) {
    return;
  }

  /* Object's pose holds reference to armature bones. sic */
  /* Note that in theory, we should have to bother about linked/non-linked/never-null/etc.
   * flags/states.
   * Fortunately, this is just a tag, so we can accept to 'over-tag' a bit for pose recalc,
   * and avoid another complex and risky condition nightmare like the one we have in
   * foreach_libblock_remap_callback(). */
  const IDRemapperApplyResult expected_mapping_result = BKE_id_remapper_get_mapping_result(
      id_remapper, static_cast<ID *>(ob->data), ID_REMAP_APPLY_DEFAULT, nullptr);
  if (is_cleanup_type || expected_mapping_result == ID_REMAP_RESULT_SOURCE_REMAPPED) {
    ob->pose->flag |= POSE_RECALC;
    /* We need to clear pose bone pointers immediately, some code may access those before
     * pose is actually recomputed, which can lead to segfault. */
    BKE_pose_clear_pointers(ob->pose);
  }
}

static void libblock_remap_data_preprocess(ID *id_owner,
                                           eIDRemapType remap_type,
                                           const IDRemapper *id_remapper)
{
  switch (GS(id_owner->name)) {
    case ID_OB: {
      Object *ob = (Object *)id_owner;
      libblock_remap_data_preprocess_ob(ob, remap_type, id_remapper);
      break;
    }
    default:
      break;
  }
}

/**
 * Can be called with both old_ob and new_ob being nullptr,
 * this means we have to check whole Main database then.
 */
static void libblock_remap_data_postprocess_object_update(Main *bmain,
                                                          Object *old_ob,
                                                          Object * /*new_ob*/,
                                                          const bool do_sync_collection)
{
  /* Will only effectively process collections that have been tagged with
   * #COLLECTION_TAG_COLLECTION_OBJECT_DIRTY. See #collection_foreach_id callback. */
  BKE_collections_object_remove_invalids(bmain);

  if (do_sync_collection) {
    BKE_main_collection_sync_remap(bmain);
  }

  if (old_ob == nullptr) {
    for (Object *ob = static_cast<Object *>(bmain->objects.first); ob != nullptr;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->type == OB_MBALL && BKE_mball_is_basis(ob)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
    }
  }
  else {
    for (Object *ob = static_cast<Object *>(bmain->objects.first); ob != nullptr;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->type == OB_MBALL && BKE_mball_is_basis_for(ob, old_ob)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        break; /* There is only one basis... */
      }
    }
  }
}

/* Can be called with both old_collection and new_collection being nullptr,
 * this means we have to check whole Main database then. */
static void libblock_remap_data_postprocess_collection_update(Main *bmain,
                                                              Collection *owner_collection,
                                                              Collection * /*old_collection*/,
                                                              Collection *new_collection)
{
  if (new_collection == nullptr) {
    /* XXX Complex cases can lead to nullptr pointers in other collections than old_collection,
     * and BKE_main_collection_sync_remap() does not tolerate any of those, so for now always check
     * whole existing collections for nullptr pointers.
     * I'd consider optimizing that whole collection remapping process a TODO: for later. */
    BKE_collections_child_remove_nulls(bmain, owner_collection, nullptr /*old_collection*/);
  }
  else {
    /* Temp safe fix, but a "tad" brute force... We should probably be able to use parents from
     * old_collection instead? */
    /* NOTE: Also takes care of duplicated child collections that remapping may have created. */
    BKE_main_collections_parent_relations_rebuild(bmain);
  }

  BKE_main_collection_sync_remap(bmain);
}

static void libblock_remap_data_postprocess_obdata_relink(Main *bmain, Object *ob, ID *new_id)
{
  if (ob->data == new_id) {
    switch (GS(new_id->name)) {
      case ID_ME:
        multires_force_sculpt_rebuild(ob);
        break;
      case ID_CU_LEGACY:
        BKE_curve_type_test(ob);
        break;
      default:
        break;
    }
    BKE_modifiers_test_object(ob);
    BKE_object_materials_test(bmain, ob, new_id);
  }
}

static void libblock_remap_data_postprocess_nodetree_update(Main *bmain, ID *new_id)
{
  /* Update all group nodes using a node group. */
  ntreeUpdateAllUsers(bmain, new_id);
}

static void libblock_remap_data_update_tags(ID *old_id, ID *new_id, void *user_data)
{
  IDRemap *id_remap_data = static_cast<IDRemap *>(user_data);
  const int remap_flags = id_remap_data->flag;
  if ((remap_flags & ID_REMAP_SKIP_USER_CLEAR) == 0) {
    /* XXX We may not want to always 'transfer' fake-user from old to new id...
     *     Think for now it's desired behavior though,
     *     we can always add an option (flag) to control this later if needed. */
    if (old_id != nullptr && (old_id->flag & LIB_FAKEUSER) && new_id != nullptr) {
      id_fake_user_clear(old_id);
      id_fake_user_set(new_id);
    }

    id_us_clear_real(old_id);
  }

  if (new_id != nullptr && (new_id->tag & LIB_TAG_INDIRECT) &&
      (new_id->runtime.remap.status & ID_REMAP_IS_LINKED_DIRECT))
  {
    new_id->tag &= ~LIB_TAG_INDIRECT;
    new_id->flag &= ~LIB_INDIRECT_WEAK_LINK;
    new_id->tag |= LIB_TAG_EXTERN;
  }
}

static void libblock_remap_reset_remapping_status_callback(ID *old_id,
                                                           ID *new_id,
                                                           void * /*user_data*/)
{
  BKE_libblock_runtime_reset_remapping_status(old_id);
  if (new_id != nullptr) {
    BKE_libblock_runtime_reset_remapping_status(new_id);
  }
}

/**
 * Execute the 'data' part of the remapping (that is, all ID pointers from other ID data-blocks).
 *
 * Behavior differs depending on whether given \a id is nullptr or not:
 * - \a id nullptr: \a old_id must be non-nullptr, \a new_id may be nullptr (unlinking \a old_id)
 * or not (remapping \a old_id to \a new_id). The whole \a bmain database is checked, and all
 * pointers to \a old_id are remapped to \a new_id.
 * - \a id is non-nullptr:
 *   + If \a old_id is nullptr, \a new_id must also be nullptr,
 *     and all ID pointers from \a id are cleared
 *     (i.e. \a id does not references any other data-block anymore).
 *   + If \a old_id is non-nullptr, behavior is as with a nullptr \a id, but only within given \a
 * id.
 *
 * \param bmain: the Main data storage to operate on (may be nullptr, in which case part of the
 * post-process/depsgraph update won't happen).
 * \param id: the data-block to operate on
 * (can be nullptr, in which case we operate over all IDs from given bmain).
 * \param old_id: the data-block to dereference (may be nullptr if \a id is non-nullptr).
 * \param new_id: the new data-block to replace \a old_id references with (may be nullptr).
 * \param r_id_remap_data: if non-nullptr, the IDRemap struct to use
 * (useful to retrieve info about remapping process).
 */
static void libblock_remap_data(
    Main *bmain, ID *id, eIDRemapType remap_type, IDRemapper *id_remapper, const int remap_flags)
{
  IDRemap id_remap_data = {eIDRemapType(0)};
  const bool include_ui = (remap_flags & ID_REMAP_FORCE_UI_POINTERS) != 0;
  const int foreach_id_flags = (((remap_flags & ID_REMAP_FORCE_INTERNAL_RUNTIME_POINTERS) != 0 ?
                                     IDWALK_DO_INTERNAL_RUNTIME_POINTERS :
                                     IDWALK_NOP) |
                                (include_ui ? IDWALK_INCLUDE_UI : IDWALK_NOP) |
                                ((remap_flags & ID_REMAP_NO_ORIG_POINTERS_ACCESS) != 0 ?
                                     IDWALK_NO_ORIG_POINTERS_ACCESS :
                                     IDWALK_NOP));

  id_remap_data.id_remapper = id_remapper;
  id_remap_data.type = remap_type;
  id_remap_data.bmain = bmain;
  id_remap_data.id_owner = nullptr;
  id_remap_data.flag = remap_flags;

  BKE_id_remapper_iter(id_remapper, libblock_remap_reset_remapping_status_callback, nullptr);

  if (id) {
#ifdef DEBUG_PRINT
    printf("\tchecking id %s (%p, %p)\n", id->name, id, id->lib);
#endif
    id_remap_data.id_owner = id;
    libblock_remap_data_preprocess(id_remap_data.id_owner, remap_type, id_remapper);
    BKE_library_foreach_ID_link(
        nullptr, id, foreach_libblock_remap_callback, &id_remap_data, foreach_id_flags);
  }
  else {
    /* Note that this is a very 'brute force' approach,
     * maybe we could use some depsgraph to only process objects actually using given old_id...
     * sounds rather unlikely currently, though, so this will do for now. */
    ID *id_curr;

    FOREACH_MAIN_ID_BEGIN (bmain, id_curr) {
      const uint64_t can_use_filter_id = BKE_library_id_can_use_filter_id(id_curr, include_ui);
      const bool has_mapping = BKE_id_remapper_has_mapping_for(id_remapper, can_use_filter_id);

      /* Continue when id_remapper doesn't have any mappings that can be used by id_curr. */
      if (!has_mapping) {
        continue;
      }

      /* Note that we cannot skip indirect usages of old_id
       * here (if requested), we still need to check it for the
       * user count handling...
       * XXX No more true (except for debug usage of those
       * skipping counters). */
      id_remap_data.id_owner = id_curr;
      libblock_remap_data_preprocess(id_remap_data.id_owner, remap_type, id_remapper);
      BKE_library_foreach_ID_link(
          nullptr, id_curr, foreach_libblock_remap_callback, &id_remap_data, foreach_id_flags);
    }
    FOREACH_MAIN_ID_END;
  }

  BKE_id_remapper_iter(id_remapper, libblock_remap_data_update_tags, &id_remap_data);
}

struct LibBlockRemapMultipleUserData {
  Main *bmain;
  int remap_flags;
};

static void libblock_remap_foreach_idpair_cb(ID *old_id, ID *new_id, void *user_data)
{
  if (old_id == new_id) {
    return;
  }

  LibBlockRemapMultipleUserData *data = static_cast<LibBlockRemapMultipleUserData *>(user_data);
  Main *bmain = data->bmain;
  const int remap_flags = data->remap_flags;

  BLI_assert(old_id != nullptr);
  BLI_assert((new_id == nullptr) || GS(old_id->name) == GS(new_id->name));

  if (free_notifier_reference_cb) {
    free_notifier_reference_cb(old_id);
  }

  if ((remap_flags & ID_REMAP_SKIP_USER_CLEAR) == 0) {
    /* If old_id was used by some ugly 'user_one' stuff (like Image or Clip editors...), and user
     * count has actually been incremented for that, we have to decrease once more its user
     * count... unless we had to skip some 'user_one' cases. */
    if ((old_id->tag & LIB_TAG_EXTRAUSER_SET) &&
        !(old_id->runtime.remap.status & ID_REMAP_IS_USER_ONE_SKIPPED))
    {
      id_us_clear_real(old_id);
    }
  }

  const int skipped_refcounted = old_id->runtime.remap.skipped_refcounted;
  if (old_id->us - skipped_refcounted < 0) {
    CLOG_ERROR(&LOG,
               "Error in remapping process from '%s' (%p) to '%s' (%p): "
               "wrong user count in old ID after process (summing up to %d)",
               old_id->name,
               old_id,
               new_id ? new_id->name : "<nullptr>",
               new_id,
               old_id->us - skipped_refcounted);
  }

  const int skipped_direct = old_id->runtime.remap.skipped_direct;
  if (skipped_direct == 0) {
    /* old_id is assumed to not be used directly anymore... */
    if (old_id->lib && (old_id->tag & LIB_TAG_EXTERN)) {
      old_id->tag &= ~LIB_TAG_EXTERN;
      old_id->tag |= LIB_TAG_INDIRECT;
    }
  }

  /* Some after-process updates.
   * This is a bit ugly, but cannot see a way to avoid it.
   * Maybe we should do a per-ID callback for this instead? */
  switch (GS(old_id->name)) {
    case ID_OB:
      libblock_remap_data_postprocess_object_update(
          bmain, (Object *)old_id, (Object *)new_id, true);
      break;
    case ID_GR:
      libblock_remap_data_postprocess_collection_update(
          bmain, nullptr, (Collection *)old_id, (Collection *)new_id);
      break;
    case ID_ME:
    case ID_CU_LEGACY:
    case ID_MB:
    case ID_CV:
    case ID_PT:
    case ID_VO:
      if (new_id) { /* Only affects us in case obdata was relinked (changed). */
        for (Object *ob = static_cast<Object *>(bmain->objects.first); ob;
             ob = static_cast<Object *>(ob->id.next))
        {
          libblock_remap_data_postprocess_obdata_relink(bmain, ob, new_id);
        }
      }
      break;
    default:
      break;
  }

  /* Node trees may virtually use any kind of data-block... */
  /* XXX Yuck!!!! nodetree update can do pretty much any thing when talking about py nodes,
   *     including creating new data-blocks (see #50385), so we need to unlock main here. :(
   *     Why can't we have re-entrent locks? */
  BKE_main_unlock(bmain);
  libblock_remap_data_postprocess_nodetree_update(bmain, new_id);
  BKE_main_lock(bmain);

  /* Full rebuild of DEG! */
  DEG_relations_tag_update(bmain);

  BKE_libblock_runtime_reset_remapping_status(old_id);
}

void BKE_libblock_remap_multiple_locked(Main *bmain, IDRemapper *mappings, const int remap_flags)
{
  if (BKE_id_remapper_is_empty(mappings)) {
    /* Early exit nothing to do. */
    return;
  }

  libblock_remap_data(bmain, nullptr, ID_REMAP_TYPE_REMAP, mappings, remap_flags);

  LibBlockRemapMultipleUserData user_data = {nullptr};
  user_data.bmain = bmain;
  user_data.remap_flags = remap_flags;

  BKE_id_remapper_iter(mappings, libblock_remap_foreach_idpair_cb, &user_data);

  /* We assume editors do not hold references to their IDs... This is false in some cases
   * (Image is especially tricky here),
   * editors' code is to handle refcount (id->us) itself then. */
  if (remap_editor_id_reference_cb) {
    remap_editor_id_reference_cb(mappings);
  }

  /* Full rebuild of DEG! */
  DEG_relations_tag_update(bmain);
}

void BKE_libblock_remap_multiple_raw(Main *bmain, IDRemapper *mappings, const int remap_flags)
{
  if (BKE_id_remapper_is_empty(mappings)) {
    /* Early exit nothing to do. */
    return;
  }

  libblock_remap_data(bmain,
                      nullptr,
                      ID_REMAP_TYPE_REMAP,
                      mappings,
                      remap_flags | ID_REMAP_SKIP_USER_REFCOUNT | ID_REMAP_SKIP_UPDATE_TAGGING);
}

void BKE_libblock_remap_locked(Main *bmain, void *old_idv, void *new_idv, const int remap_flags)
{
  IDRemapper *remapper = BKE_id_remapper_create();
  ID *old_id = static_cast<ID *>(old_idv);
  ID *new_id = static_cast<ID *>(new_idv);
  BKE_id_remapper_add(remapper, old_id, new_id);
  BKE_libblock_remap_multiple_locked(bmain, remapper, remap_flags);
  BKE_id_remapper_free(remapper);
}

void BKE_libblock_remap(Main *bmain, void *old_idv, void *new_idv, const int remap_flags)
{
  BKE_main_lock(bmain);

  BKE_libblock_remap_locked(bmain, old_idv, new_idv, remap_flags);

  BKE_main_unlock(bmain);
}

void BKE_libblock_remap_multiple(Main *bmain, IDRemapper *mappings, const int remap_flags)
{
  BKE_main_lock(bmain);

  BKE_libblock_remap_multiple_locked(bmain, mappings, remap_flags);

  BKE_main_unlock(bmain);
}

void BKE_libblock_unlink(Main *bmain,
                         void *idv,
                         const bool do_flag_never_null,
                         const bool do_skip_indirect)
{
  const int remap_flags = (do_skip_indirect ? ID_REMAP_SKIP_INDIRECT_USAGE : 0) |
                          (do_flag_never_null ? ID_REMAP_FLAG_NEVER_NULL_USAGE : 0);

  BKE_main_lock(bmain);

  BKE_libblock_remap_locked(bmain, idv, nullptr, remap_flags);

  BKE_main_unlock(bmain);
}

/* XXX Arg! Naming... :(
 *     _relink? avoids confusion with _remap, but is confusing with _unlink
 *     _remap_used_ids?
 *     _remap_datablocks?
 *     BKE_id_remap maybe?
 *     ... sigh
 */

struct LibBlockRelinkMultipleUserData {
  Main *bmain;
  LinkNode *ids;
};

static void libblock_relink_foreach_idpair_cb(ID *old_id, ID *new_id, void *user_data)
{
  LibBlockRelinkMultipleUserData *data = static_cast<LibBlockRelinkMultipleUserData *>(user_data);
  Main *bmain = data->bmain;
  LinkNode *ids = data->ids;

  BLI_assert(old_id != nullptr);
  BLI_assert((new_id == nullptr) || GS(old_id->name) == GS(new_id->name));
  BLI_assert(old_id != new_id);

  bool is_object_update_processed = false;
  for (LinkNode *ln_iter = ids; ln_iter != nullptr; ln_iter = ln_iter->next) {
    ID *id_iter = static_cast<ID *>(ln_iter->link);

    /* Some after-process updates.
     * This is a bit ugly, but cannot see a way to avoid it.
     * Maybe we should do a per-ID callback for this instead?
     */
    switch (GS(id_iter->name)) {
      case ID_SCE:
      case ID_GR: {
        /* NOTE: here we know which collection we have affected, so at lest for nullptr children
         * detection we can only process that one.
         * This is also a required fix in case `id` would not be in Main anymore, which can happen
         * e.g. when called from `id_delete`. */
        Collection *owner_collection = (GS(id_iter->name) == ID_GR) ?
                                           (Collection *)id_iter :
                                           ((Scene *)id_iter)->master_collection;
        switch (GS(old_id->name)) {
          case ID_OB:
            if (!is_object_update_processed) {
              libblock_remap_data_postprocess_object_update(
                  bmain, (Object *)old_id, (Object *)new_id, true);
              is_object_update_processed = true;
            }
            break;
          case ID_GR:
            libblock_remap_data_postprocess_collection_update(
                bmain, owner_collection, (Collection *)old_id, (Collection *)new_id);
            break;
          default:
            break;
        }
        break;
      }
      case ID_OB:
        if (new_id != nullptr) { /* Only affects us in case obdata was relinked (changed). */
          libblock_remap_data_postprocess_obdata_relink(bmain, (Object *)id_iter, new_id);
        }
        break;
      default:
        break;
    }
  }
}

void BKE_libblock_relink_multiple(Main *bmain,
                                  LinkNode *ids,
                                  const eIDRemapType remap_type,
                                  IDRemapper *id_remapper,
                                  const int remap_flags)
{
  BLI_assert(remap_type == ID_REMAP_TYPE_REMAP || BKE_id_remapper_is_empty(id_remapper));

  for (LinkNode *ln_iter = ids; ln_iter != nullptr; ln_iter = ln_iter->next) {
    ID *id_iter = static_cast<ID *>(ln_iter->link);
    libblock_remap_data(bmain, id_iter, remap_type, id_remapper, remap_flags);
  }

  if (bmain == nullptr) {
    return;
  }

  switch (remap_type) {
    case ID_REMAP_TYPE_REMAP: {
      LibBlockRelinkMultipleUserData user_data = {nullptr};
      user_data.bmain = bmain;
      user_data.ids = ids;

      BKE_id_remapper_iter(id_remapper, libblock_relink_foreach_idpair_cb, &user_data);
      break;
    }
    case ID_REMAP_TYPE_CLEANUP: {
      bool is_object_update_processed = false;
      for (LinkNode *ln_iter = ids; ln_iter != nullptr; ln_iter = ln_iter->next) {
        ID *id_iter = static_cast<ID *>(ln_iter->link);

        switch (GS(id_iter->name)) {
          case ID_SCE:
          case ID_GR: {
            /* NOTE: here we know which collection we have affected, so at lest for nullptr
             * children detection we can only process that one. This is also a required fix in case
             * `id` would not be in Main anymore, which can happen e.g. when called from
             * `id_delete`. */
            Collection *owner_collection = (GS(id_iter->name) == ID_GR) ?
                                               (Collection *)id_iter :
                                               ((Scene *)id_iter)->master_collection;
            /* No choice but to check whole objects once, and all children collections. */
            if (!is_object_update_processed) {
              /* We only want to affect Object pointers here, not Collection ones, LayerCollections
               * will be resynced as part of the call to
               * `libblock_remap_data_postprocess_collection_update` below. */
              libblock_remap_data_postprocess_object_update(bmain, nullptr, nullptr, false);
              is_object_update_processed = true;
            }
            libblock_remap_data_postprocess_collection_update(
                bmain, owner_collection, nullptr, nullptr);
            break;
          }
          default:
            break;
        }
      }

      break;
    }
    default:
      BLI_assert_unreachable();
  }

  DEG_relations_tag_update(bmain);
}

void BKE_libblock_relink_ex(
    Main *bmain, void *idv, void *old_idv, void *new_idv, const int remap_flags)
{

  /* Should be able to replace all _relink() functions (constraints, rigidbody, etc.) ? */

  ID *id = static_cast<ID *>(idv);
  ID *old_id = static_cast<ID *>(old_idv);
  ID *new_id = static_cast<ID *>(new_idv);
  LinkNode ids{};
  ids.next = nullptr;
  ids.link = idv;

  /* No need to lock here, we are only affecting given ID, not bmain database. */
  IDRemapper *id_remapper = BKE_id_remapper_create();
  eIDRemapType remap_type = ID_REMAP_TYPE_REMAP;

  BLI_assert(id != nullptr);
  UNUSED_VARS_NDEBUG(id);
  if (old_id != nullptr) {
    BLI_assert((new_id == nullptr) || GS(old_id->name) == GS(new_id->name));
    BLI_assert(old_id != new_id);
    BKE_id_remapper_add(id_remapper, old_id, new_id);
  }
  else {
    BLI_assert(new_id == nullptr);
    remap_type = ID_REMAP_TYPE_CLEANUP;
  }

  BKE_libblock_relink_multiple(bmain, &ids, remap_type, id_remapper, remap_flags);

  BKE_id_remapper_free(id_remapper);
}

struct RelinkToNewIDData {
  LinkNode *ids;
  IDRemapper *id_remapper;
};

static void libblock_relink_to_newid_prepare_data(Main *bmain,
                                                  ID *id,
                                                  RelinkToNewIDData *relink_data);
static int id_relink_to_newid_looper(LibraryIDLinkCallbackData *cb_data)
{
  const int cb_flag = cb_data->cb_flag;
  /* NOTE: For now, support remapping `IDWALK_CB_EMBEDDED_NON_OWNING` pointers. */
  if (cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_OVERRIDE_LIBRARY_REFERENCE)) {
    return IDWALK_RET_NOP;
  }

  Main *bmain = cb_data->bmain;
  ID **id_pointer = cb_data->id_pointer;
  ID *id = *id_pointer;
  RelinkToNewIDData *relink_data = (RelinkToNewIDData *)cb_data->user_data;

  if (id) {
    /* See: NEW_ID macro */
    if (id->newid != nullptr) {
      BKE_id_remapper_add(relink_data->id_remapper, id, id->newid);
      id = id->newid;
    }
    if (id->tag & LIB_TAG_NEW) {
      libblock_relink_to_newid_prepare_data(bmain, id, relink_data);
    }
  }
  return IDWALK_RET_NOP;
}

static void libblock_relink_to_newid_prepare_data(Main *bmain,
                                                  ID *id,
                                                  RelinkToNewIDData *relink_data)
{
  if (ID_IS_LINKED(id)) {
    return;
  }

  id->tag &= ~LIB_TAG_NEW;
  BLI_linklist_prepend(&relink_data->ids, id);
  BKE_library_foreach_ID_link(bmain, id, id_relink_to_newid_looper, relink_data, 0);
}

void BKE_libblock_relink_to_newid(Main *bmain, ID *id, const int remap_flag)
{
  if (ID_IS_LINKED(id)) {
    return;
  }
  /* We do not want to have those cached relationship data here. */
  BLI_assert(bmain->relations == nullptr);

  RelinkToNewIDData relink_data{};
  relink_data.ids = nullptr;
  relink_data.id_remapper = BKE_id_remapper_create();

  libblock_relink_to_newid_prepare_data(bmain, id, &relink_data);

  const int remap_flag_final = remap_flag | ID_REMAP_SKIP_INDIRECT_USAGE |
                               ID_REMAP_SKIP_OVERRIDE_LIBRARY;
  BKE_libblock_relink_multiple(
      bmain, relink_data.ids, ID_REMAP_TYPE_REMAP, relink_data.id_remapper, remap_flag_final);

  BKE_id_remapper_free(relink_data.id_remapper);
  BLI_linklist_free(relink_data.ids, nullptr);
}
