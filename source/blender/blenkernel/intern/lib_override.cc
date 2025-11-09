/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <optional>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "BKE_anim_data.hh"
#include "BKE_armature.hh"
#include "BKE_blender.hh"
#include "BKE_collection.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"
#include "BKE_node.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "BLO_readfile.hh"

#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_memarena.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"
#include "RNA_types.hh"

#include "atomic_ops.h"

#include "lib_intern.hh"

// #define DEBUG_OVERRIDE_TIMEIT

#ifdef DEBUG_OVERRIDE_TIMEIT
#  include "BLI_time_utildefines.h"
#endif

using namespace blender::bke;

static CLG_LogRef LOG = {"lib.override"};
static CLG_LogRef LOG_RESYNC = {"lib.override.resync"};

namespace blender::bke::liboverride {

bool is_auto_resync_enabled()
{
  return !USER_DEVELOPER_TOOL_TEST(&U, no_override_auto_resync) &&
         (G.fileflags & G_LIBOVERRIDE_NO_AUTO_RESYNC) == 0;
}

}  // namespace blender::bke::liboverride

static void lib_override_library_property_copy(IDOverrideLibraryProperty *op_dst,
                                               IDOverrideLibraryProperty *op_src);
static void lib_override_library_property_operation_copy(
    IDOverrideLibraryPropertyOperation *opop_dst, IDOverrideLibraryPropertyOperation *opop_src);

static void lib_override_library_property_clear(IDOverrideLibraryProperty *op);
static void lib_override_library_property_operation_clear(
    IDOverrideLibraryPropertyOperation *opop);

BLI_INLINE IDOverrideLibraryRuntime *override_library_runtime_ensure(
    IDOverrideLibrary *liboverride)
{
  if (liboverride->runtime == nullptr) {
    liboverride->runtime = MEM_callocN<IDOverrideLibraryRuntime>(__func__);
  }
  return liboverride->runtime;
}

/**
 * Helper to preserve Pose mode on override objects.
 * A bit annoying to have this special case, but not much to be done here currently, since the
 * matching RNA property is read-only.
 */
BLI_INLINE void lib_override_object_posemode_transfer(ID *id_dst, ID *id_src)
{
  if (GS(id_src->name) == ID_OB && GS(id_dst->name) == ID_OB) {
    Object *ob_src = reinterpret_cast<Object *>(id_src);
    Object *ob_dst = reinterpret_cast<Object *>(id_dst);
    if (ob_src->type == OB_ARMATURE && (ob_src->mode & OB_MODE_POSE) != 0) {
      ob_dst->restore_mode = ob_dst->mode;
      ob_dst->mode |= OB_MODE_POSE;
    }
  }
}

/** Get override data for a given ID. Needed because of our beloved shape keys snowflake. */
BLI_INLINE const IDOverrideLibrary *BKE_lib_override_library_get(const Main * /*bmain*/,
                                                                 const ID *id,
                                                                 const ID * /*owner_id_hint*/,
                                                                 const ID **r_owner_id)
{
  if (id->flag & ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE) {
    const ID *owner_id = BKE_id_owner_get(const_cast<ID *>(id));
    BLI_assert_msg(owner_id != nullptr, "Liboverride-embedded ID with no owner");
    if (r_owner_id != nullptr) {
      *r_owner_id = owner_id;
    }
    return owner_id->override_library;
  }

  if (r_owner_id != nullptr) {
    *r_owner_id = id;
  }
  return id->override_library;
}

IDOverrideLibrary *BKE_lib_override_library_get(Main *bmain,
                                                ID *id,
                                                ID *owner_id_hint,
                                                ID **r_owner_id)
{
  /* Reuse the implementation of the const access function, which does not change the arguments.
   * Add const explicitly to make it clear to the compiler to avoid just calling this function. */
  return const_cast<IDOverrideLibrary *>(
      BKE_lib_override_library_get(const_cast<const Main *>(bmain),
                                   const_cast<const ID *>(id),
                                   const_cast<const ID *>(owner_id_hint),
                                   const_cast<const ID **>(r_owner_id)));
}

IDOverrideLibrary *BKE_lib_override_library_init(ID *local_id, ID *reference_id)
{
  /* The `reference_id` *must* be linked data. */
  BLI_assert(!reference_id || ID_IS_LINKED(reference_id));
  BLI_assert(local_id->override_library == nullptr);

  /* Else, generate new empty override. */
  local_id->override_library = MEM_callocN<IDOverrideLibrary>(__func__);
  local_id->override_library->reference = reference_id;
  if (reference_id) {
    id_us_plus(local_id->override_library->reference);
  }
  local_id->tag &= ~ID_TAG_LIBOVERRIDE_REFOK;
  /* By default initialized liboverrides are 'system overrides', higher-level code is responsible
   * to unset this flag for specific IDs. */
  local_id->override_library->flag |= LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
  /* TODO: do we want to add tag or flag to referee to mark it as such? */
  return local_id->override_library;
}

void BKE_lib_override_library_copy(ID *dst_id, const ID *src_id, const bool do_full_copy)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY(src_id));

  if (dst_id->override_library != nullptr) {
    if (src_id->override_library == nullptr) {
      BKE_lib_override_library_free(&dst_id->override_library, true);
      return;
    }

    BKE_lib_override_library_clear(dst_id->override_library, true);
  }
  else if (src_id->override_library == nullptr) {
    /* Virtual overrides of embedded data does not require any extra work. */
    return;
  }
  else {
    BKE_lib_override_library_init(dst_id, nullptr);
  }

  /* Reuse the source's reference for destination ID. */
  dst_id->override_library->reference = src_id->override_library->reference;
  id_us_plus(dst_id->override_library->reference);

  dst_id->override_library->hierarchy_root = src_id->override_library->hierarchy_root;
  dst_id->override_library->flag = src_id->override_library->flag;

  if (do_full_copy) {
    BLI_duplicatelist(&dst_id->override_library->properties,
                      &src_id->override_library->properties);
    for (IDOverrideLibraryProperty *op_dst = static_cast<IDOverrideLibraryProperty *>(
                                       dst_id->override_library->properties.first),
                                   *op_src = static_cast<IDOverrideLibraryProperty *>(
                                       src_id->override_library->properties.first);
         op_dst;
         op_dst = op_dst->next, op_src = op_src->next)
    {
      lib_override_library_property_copy(op_dst, op_src);
    }
  }

  dst_id->tag &= ~ID_TAG_LIBOVERRIDE_REFOK;
}

void BKE_lib_override_library_clear(IDOverrideLibrary *liboverride, const bool do_id_user)
{
  BLI_assert(liboverride != nullptr);

  if (!ELEM(nullptr, liboverride->runtime, liboverride->runtime->rna_path_to_override_properties))
  {
    BLI_ghash_clear(liboverride->runtime->rna_path_to_override_properties, nullptr, nullptr);
  }

  LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &liboverride->properties) {
    lib_override_library_property_clear(op);
  }
  BLI_freelistN(&liboverride->properties);

  if (do_id_user) {
    id_us_min(liboverride->reference);
    /* override->storage should never be refcounted... */
  }
}

void BKE_lib_override_library_free(IDOverrideLibrary **liboverride, const bool do_id_user)
{
  BLI_assert(*liboverride != nullptr);

  if ((*liboverride)->runtime != nullptr) {
    if ((*liboverride)->runtime->rna_path_to_override_properties != nullptr) {
      BLI_ghash_free((*liboverride)->runtime->rna_path_to_override_properties, nullptr, nullptr);
    }
    MEM_SAFE_FREE((*liboverride)->runtime);
  }

  BKE_lib_override_library_clear(*liboverride, do_id_user);
  MEM_freeN(*liboverride);
  *liboverride = nullptr;
}

static ID *lib_override_library_create_from(Main *bmain,
                                            Library *owner_library,
                                            ID *reference_id,
                                            const int lib_id_copy_flags)
{
  /* NOTE: do not copy possible override data from the reference here. */
  ID *local_id = BKE_id_copy_in_lib(bmain,
                                    owner_library,
                                    reference_id,
                                    std::nullopt,
                                    nullptr,
                                    (LIB_ID_COPY_DEFAULT | LIB_ID_COPY_NO_LIB_OVERRIDE |
                                     LIB_ID_COPY_NO_LIB_OVERRIDE_LOCAL_DATA_FLAG |
                                     lib_id_copy_flags));
  if (local_id == nullptr) {
    return nullptr;
  }
  BLI_assert(local_id->lib == owner_library);
  id_us_min(local_id);

  /* In case we could not get an override ID with the exact same name as its linked reference,
   * ensure we at least get a uniquely named override ID over the whole current Main data, to
   * reduce potential name collisions with other reference IDs.
   *
   * While in normal cases this would not be an issue, when files start to get heavily broken and
   * not sound, such conflicts can become a source of problems. */
  if (!STREQ(local_id->name + 2, reference_id->name + 2)) {
    BKE_main_namemap_remove_id(*bmain, *local_id);
    BLI_strncpy(local_id->name + 2, reference_id->name + 2, MAX_ID_NAME - 2);
    BKE_main_global_namemap_get_unique_name(*bmain, *local_id, local_id->name + 2);
    id_sort_by_name(which_libbase(bmain, GS(local_id->name)), local_id, nullptr);
  }

  /* In `NO_MAIN` case, generic `BKE_id_copy` code won't call this.
   * In liboverride resync case however, the currently not-in-Main new IDs will be added back to
   * Main later, so ensure that their linked dependencies and paths are properly handled here.
   *
   * NOTE: This is likely not the best place to do this. Ideally, #BKE_libblock_management_main_add
   * e.g. should take care of this. But for the time being, this works and has been battle-proofed.
   */
  if ((lib_id_copy_flags & LIB_ID_CREATE_NO_MAIN) != 0 && !ID_IS_LINKED(local_id)) {
    lib_id_copy_ensure_local(bmain, reference_id, local_id, 0);
  }

  BKE_lib_override_library_init(local_id, reference_id);

  /* NOTE: From liboverride perspective (and RNA one), shape keys are considered as local embedded
   * data-blocks, just like root node trees or master collections. Therefore, we never need to
   * create overrides for them. We need a way to mark them as overrides though. */
  Key *reference_key = BKE_key_from_id(reference_id);
  if (reference_key != nullptr) {
    Key *local_key = BKE_key_from_id(local_id);
    BLI_assert(local_key != nullptr);
    local_key->id.flag |= ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE;
  }

  return local_id;
}

bool BKE_lib_override_library_is_user_edited(const ID *id)
{
  /* TODO: This could be simplified by storing a flag in #IDOverrideLibrary
   * during the diffing process? */

  if (!ID_IS_OVERRIDE_LIBRARY(id)) {
    return false;
  }

  /* A bit weird, but those embedded IDs are handled by their owner ID anyway, so we can just
   * assume they are never user-edited, actual proper detection will happen from their owner check.
   */
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    return false;
  }

  LISTBASE_FOREACH (const IDOverrideLibraryProperty *, op, &id->override_library->properties) {
    LISTBASE_FOREACH (const IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
      if ((opop->flag & LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE) != 0) {
        continue;
      }
      if (opop->operation == LIBOVERRIDE_OP_NOOP) {
        continue;
      }
      /* If an operation does not match the filters above, it is considered as a user-editing one,
       * therefore this override is user-edited. */
      return true;
    }
  }
  return false;
}

bool BKE_lib_override_library_is_system_defined(const Main *bmain, const ID *id)
{
  if (ID_IS_OVERRIDE_LIBRARY(id)) {
    const ID *override_owner_id;
    BKE_lib_override_library_get(bmain, id, nullptr, &override_owner_id);
    return (override_owner_id->override_library->flag & LIBOVERRIDE_FLAG_SYSTEM_DEFINED) != 0;
  }
  return false;
}

bool BKE_lib_override_library_property_is_animated(
    const ID *id,
    const IDOverrideLibraryProperty *liboverride_prop,
    const PropertyRNA *override_rna_prop,
    const int rnaprop_index)
{
  AnimData *anim_data = BKE_animdata_from_id(id);
  if (anim_data != nullptr) {
    FCurve *fcurve;
    char *index_token_start = const_cast<char *>(
        RNA_path_array_index_token_find(liboverride_prop->rna_path, override_rna_prop));
    if (index_token_start != nullptr) {
      const char index_token_start_backup = *index_token_start;
      *index_token_start = '\0';
      fcurve = BKE_animadata_fcurve_find_by_rna_path(
          anim_data, liboverride_prop->rna_path, rnaprop_index, nullptr, nullptr);
      *index_token_start = index_token_start_backup;
    }
    else {
      fcurve = BKE_animadata_fcurve_find_by_rna_path(
          anim_data, liboverride_prop->rna_path, 0, nullptr, nullptr);
    }
    if (fcurve != nullptr) {
      return true;
    }
  }
  return false;
}

static int foreachid_is_hierarchy_leaf_fn(LibraryIDLinkCallbackData *cb_data)
{
  ID *id_owner = cb_data->owner_id;
  ID *id = *cb_data->id_pointer;
  bool *is_leaf = static_cast<bool *>(cb_data->user_data);

  if (cb_data->cb_flag & IDWALK_CB_LOOPBACK) {
    return IDWALK_RET_NOP;
  }

  if (id != nullptr && ID_IS_OVERRIDE_LIBRARY_REAL(id) &&
      id->override_library->hierarchy_root == id_owner->override_library->hierarchy_root)
  {
    *is_leaf = false;
    return IDWALK_RET_STOP_ITER;
  }
  return IDWALK_RET_NOP;
}

bool BKE_lib_override_library_is_hierarchy_leaf(Main *bmain, ID *id)
{
  if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    bool is_leaf = true;
    BKE_library_foreach_ID_link(
        bmain, id, foreachid_is_hierarchy_leaf_fn, &is_leaf, IDWALK_READONLY);
    return is_leaf;
  }

  return false;
}

void BKE_lib_override_id_tag_on_deg_tag_from_user(ID *id)
{
  /* Only local liboverrides need to be tagged for refresh, linked ones should not be editable. */
  if (ID_IS_LINKED(id) || !ID_IS_OVERRIDE_LIBRARY(id)) {
    return;
  }
  /* NOTE: Valid relationships between IDs here (especially the beloved ObData <-> ShapeKey special
   * case) cannot be always expected when ID get tagged. So now, embedded IDs and similar also get
   * tagged, and the 'liboverride refresh' code is responsible to properly propagate the update to
   * the owner ID when needed (see #BKE_lib_override_library_main_operations_create). */
  id->tag |= ID_TAG_LIBOVERRIDE_AUTOREFRESH;
}

ID *BKE_lib_override_library_create_from_id(Main *bmain,
                                            ID *reference_id,
                                            const bool do_tagged_remap)
{
  BLI_assert(reference_id != nullptr);
  BLI_assert(ID_IS_LINKED(reference_id));

  ID *local_id = lib_override_library_create_from(bmain, nullptr, reference_id, 0);
  /* We cannot allow automatic hierarchy resync on this ID, it is highly likely to generate a giant
   * mess in case there are a lot of hidden, non-instantiated, non-properly organized dependencies.
   * Ref #94650. */
  local_id->override_library->flag |= LIBOVERRIDE_FLAG_NO_HIERARCHY;
  local_id->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
  local_id->override_library->hierarchy_root = local_id;

  if (do_tagged_remap) {
    Key *reference_key = BKE_key_from_id(reference_id);
    Key *local_key = nullptr;
    if (reference_key != nullptr) {
      local_key = BKE_key_from_id(local_id);
      BLI_assert(local_key != nullptr);
    }

    ID *other_id;
    FOREACH_MAIN_ID_BEGIN (bmain, other_id) {
      if ((other_id->tag & ID_TAG_DOIT) != 0 && !ID_IS_LINKED(other_id)) {
        /* Note that using ID_REMAP_SKIP_INDIRECT_USAGE below is superfluous, as we only remap
         * local IDs usages anyway. */
        BKE_libblock_relink_ex(bmain,
                               other_id,
                               reference_id,
                               local_id,
                               ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_OVERRIDE_LIBRARY);
        if (reference_key != nullptr) {
          BKE_libblock_relink_ex(bmain,
                                 other_id,
                                 &reference_key->id,
                                 &local_key->id,
                                 ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_OVERRIDE_LIBRARY);
        }
      }
    }
    FOREACH_MAIN_ID_END;
  }

  /* Cleanup global namemap, to avoid extra processing with regular ID name management. Better to
   * re-create the global namemap on demand. */
  BKE_main_namemap_destroy(&bmain->name_map_global);

  return local_id;
}

static void lib_override_prefill_newid_from_existing_overrides(Main *bmain, ID *id_hierarchy_root)
{
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    ID *id = id_iter;
    if (GS(id_iter->name) == ID_KE) {
      id = reinterpret_cast<Key *>(id_iter)->from;
      BLI_assert(id != nullptr);
    }
    if (ID_IS_OVERRIDE_LIBRARY_REAL(id) &&
        id->override_library->hierarchy_root == id_hierarchy_root)
    {
      id->override_library->reference->newid = id;
      if (GS(id_iter->name) == ID_KE) {
        Key *reference_key = BKE_key_from_id(id->override_library->reference);
        if (reference_key != nullptr) {
          reference_key->id.newid = id_iter;
        }
      }
    }
  }
  FOREACH_MAIN_ID_END;
}

static void lib_override_remapper_overrides_add(id::IDRemapper &id_remapper,
                                                ID *reference_id,
                                                ID *local_id)
{
  id_remapper.add(reference_id, local_id);

  Key *reference_key = BKE_key_from_id(reference_id);
  Key *local_key = nullptr;
  if (reference_key != nullptr) {
    if (reference_id->newid != nullptr) {
      local_key = BKE_key_from_id(reference_id->newid);
      BLI_assert(local_key != nullptr);
    }

    id_remapper.add(&reference_key->id, &local_key->id);
  }
}

bool BKE_lib_override_library_create_from_tag(Main *bmain,
                                              Library *owner_library,
                                              const ID *id_root_reference,
                                              ID *id_hierarchy_root,
                                              const ID *id_hierarchy_root_reference,
                                              const bool do_no_main,
                                              const bool do_fully_editable)
{
  /* TODO: Make this static local function instead?
   * API is becoming complex, and it's not used outside of this file anyway. */

  BLI_assert(id_root_reference != nullptr && ID_IS_LINKED(id_root_reference));
  /* If we do not have any hierarchy root given, then the root reference must be tagged for
   * override. */
  BLI_assert(id_hierarchy_root != nullptr || id_hierarchy_root_reference != nullptr ||
             (id_root_reference->tag & ID_TAG_DOIT) != 0);
  /* At least one of the hierarchy root pointers must be nullptr, passing both is useless and can
   * create confusion. */
  BLI_assert(ELEM(nullptr, id_hierarchy_root, id_hierarchy_root_reference));

  if (id_hierarchy_root != nullptr) {
    /* If the hierarchy root is given, it must be a valid existing override (used during partial
     * resync process mainly). */
    BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_hierarchy_root) &&
               id_hierarchy_root->override_library->reference->lib == id_root_reference->lib);

    if (!do_no_main) {
      /* When processing within Main, set existing overrides in given hierarchy as 'newid' of their
       * linked reference. This allows to re-use existing overrides instead of creating new ones in
       * partial override cases. */
      lib_override_prefill_newid_from_existing_overrides(bmain, id_hierarchy_root);
    }
  }
  if (!ELEM(id_hierarchy_root_reference, nullptr, id_root_reference)) {
    /* If the reference hierarchy root is given, it must be from the same library as the reference
     * root, and also tagged for override. */
    BLI_assert(id_hierarchy_root_reference->lib == id_root_reference->lib &&
               (id_hierarchy_root_reference->tag & ID_TAG_DOIT) != 0);
  }

  const Library *reference_library = id_root_reference->lib;

  ID *reference_id;
  bool success = true;

  ListBase todo_ids = {nullptr};
  LinkData *todo_id_iter;

  /* Get all IDs we want to override. */
  FOREACH_MAIN_ID_BEGIN (bmain, reference_id) {
    if ((reference_id->tag & ID_TAG_DOIT) != 0 && reference_id->lib == reference_library &&
        BKE_idtype_idcode_is_linkable(GS(reference_id->name)))
    {
      todo_id_iter = MEM_callocN<LinkData>(__func__);
      todo_id_iter->data = reference_id;
      BLI_addtail(&todo_ids, todo_id_iter);
    }
  }
  FOREACH_MAIN_ID_END;

  /* Override the IDs. */
  for (todo_id_iter = static_cast<LinkData *>(todo_ids.first); todo_id_iter != nullptr;
       todo_id_iter = todo_id_iter->next)
  {
    reference_id = static_cast<ID *>(todo_id_iter->data);

    /* If `newid` is already set, assume it has been handled by calling code.
     * Only current use case: re-using proxy ID when converting to liboverride. */
    if (reference_id->newid == nullptr) {
      /* NOTE: `no main` case is used during resync procedure, to support recursive resync.
       * This requires extra care further down the resync process,
       * see: #BKE_lib_override_library_resync. */
      reference_id->newid = lib_override_library_create_from(
          bmain, owner_library, reference_id, do_no_main ? LIB_ID_CREATE_NO_MAIN : 0);
      if (reference_id->newid == nullptr) {
        success = false;
        break;
      }
      if (do_fully_editable) {
        reference_id->newid->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
      }
    }
    /* We also tag the new IDs so that in next step we can remap their pointers too. */
    reference_id->newid->tag |= ID_TAG_DOIT;

    Key *reference_key = BKE_key_from_id(reference_id);
    if (reference_key != nullptr) {
      reference_key->id.tag |= ID_TAG_DOIT;

      Key *local_key = BKE_key_from_id(reference_id->newid);
      BLI_assert(local_key != nullptr);
      reference_key->id.newid = &local_key->id;
      /* We also tag the new IDs so that in next step we can remap their pointers too. */
      local_key->id.tag |= ID_TAG_DOIT;
    }
  }

  /* Only remap new local ID's pointers, we don't want to force our new overrides onto our whole
   * existing linked IDs usages. */
  if (success) {
    /* If a valid liboverride hierarchy root was given, only remap non-liboverride data and
     * liboverrides belonging to that hierarchy. Avoids having other liboverride hierarchies of
     * the same reference data also remapped to the newly created liboverride. */
    const bool do_remap_liboverride_hierarchy_only = (id_hierarchy_root != nullptr && !do_no_main);

    if (id_hierarchy_root_reference != nullptr) {
      id_hierarchy_root = id_hierarchy_root_reference->newid;
    }
    else if (id_root_reference->newid != nullptr &&
             (id_hierarchy_root == nullptr ||
              id_hierarchy_root->override_library->reference == id_root_reference))
    {
      id_hierarchy_root = id_root_reference->newid;
    }
    BLI_assert(id_hierarchy_root != nullptr);

    blender::Vector<ID *> relinked_ids;
    id::IDRemapper id_remapper;
    /* Still checking the whole Main, that way we can tag other local IDs as needing to be
     * remapped to use newly created overriding IDs, if needed. */
    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      ID *other_id;
      /* In case we created new overrides as 'no main', they are not accessible directly in this
       * loop, but we can get to them through their reference's `newid` pointer. */
      if (do_no_main && id->lib == id_root_reference->lib && id->newid != nullptr) {
        other_id = id->newid;
        /* Otherwise we cannot properly distinguish between IDs that are actually from the
         * linked library (and should not be remapped), and IDs that are overrides re-generated
         * from the reference from the linked library, and must therefore be remapped.
         *
         * This is reset afterwards at the end of this loop. */
        other_id->lib = nullptr;
      }
      else {
        other_id = id;
      }

      /* If other ID is a linked one, but not from the same library as our reference, then we
       * consider we should also relink it, as part of recursive resync. */
      if ((other_id->tag & ID_TAG_DOIT) != 0 && other_id->lib != id_root_reference->lib) {
        ID *owner_id;
        BKE_lib_override_library_get(bmain, other_id, nullptr, &owner_id);

        /* When the root of the current liboverride hierarchy is known, only remap liboverrides if
         * they belong to that hierarchy. */
        if (!do_remap_liboverride_hierarchy_only ||
            (!ID_IS_OVERRIDE_LIBRARY_REAL(owner_id) ||
             owner_id->override_library->hierarchy_root == id_hierarchy_root))
        {
          relinked_ids.append(other_id);
        }

        if (ID_IS_OVERRIDE_LIBRARY_REAL(other_id) &&
            other_id->override_library->hierarchy_root == id_hierarchy_root)
        {
          reference_id = other_id->override_library->reference;
          ID *local_id = reference_id->newid;
          if (other_id == local_id) {
            lib_override_remapper_overrides_add(id_remapper, reference_id, local_id);
          }
        }
      }
      if (other_id != id) {
        other_id->lib = id_root_reference->lib;
      }
    }
    FOREACH_MAIN_ID_END;

    for (todo_id_iter = static_cast<LinkData *>(todo_ids.first); todo_id_iter != nullptr;
         todo_id_iter = todo_id_iter->next)
    {
      reference_id = static_cast<ID *>(todo_id_iter->data);
      ID *local_id = reference_id->newid;

      if (local_id == nullptr) {
        continue;
      }

      local_id->override_library->hierarchy_root = id_hierarchy_root;

      lib_override_remapper_overrides_add(id_remapper, reference_id, local_id);
    }

    BKE_libblock_relink_multiple(bmain,
                                 relinked_ids,
                                 ID_REMAP_TYPE_REMAP,
                                 id_remapper,
                                 ID_REMAP_SKIP_OVERRIDE_LIBRARY | ID_REMAP_FORCE_USER_REFCOUNT);

    relinked_ids.clear();
  }
  else {
    /* We need to cleanup potentially already created data. */
    for (todo_id_iter = static_cast<LinkData *>(todo_ids.first); todo_id_iter != nullptr;
         todo_id_iter = todo_id_iter->next)
    {
      reference_id = static_cast<ID *>(todo_id_iter->data);
      BKE_id_delete(bmain, reference_id->newid);
      reference_id->newid = nullptr;
    }
  }

  BLI_freelistN(&todo_ids);

  return success;
}

struct LibOverrideGroupTagData {
  Main *bmain;
  Scene *scene;

  /** The linked data used as reference for the liboverrides. */
  ID *id_root_reference;
  ID *hierarchy_root_id_reference;

  /** The existing liboverrides, if any. */
  ID *id_root_override;
  ID *hierarchy_root_id_override;

  /**
   * Whether we are looping on override data, or their references (linked) one.
   *
   * IMPORTANT: This value controls which of the `reference`/`override` ID pointers are accessed by
   * the `root`/`hierarchy_root` accessor functions below. */
  bool is_override;

  ID *root_get()
  {
    return is_override ? id_root_override : id_root_reference;
  }
  void root_set(ID *id_root)
  {
    if (is_override) {
      id_root_override = id_root;
    }
    else {
      id_root_reference = id_root;
    }
  }
  ID *hierarchy_root_get()
  {
    return is_override ? hierarchy_root_id_override : hierarchy_root_id_reference;
  }
  void hierarchy_root_set(ID *hierarchy_root_id)
  {
    if (is_override) {
      hierarchy_root_id_override = hierarchy_root_id;
    }
    else {
      hierarchy_root_id_reference = hierarchy_root_id;
    }
  }

  /** Whether we are creating new override, or resyncing existing one. */
  bool is_resync;

  /** ID tag to use for IDs detected as being part of the liboverride hierarchy. */
  uint tag;
  uint missing_tag;

  /**
   * A set of all IDs belonging to the reference linked hierarchy that is being overridden.
   *
   * NOTE: This is needed only for partial resync, when only part of the liboverridden hierarchy is
   * re-generated, since some IDs in that sub-hierarchy may not be detected as needing to be
   * overridden, while they would when considering the whole hierarchy. */
  blender::Set<ID *> linked_ids_hierarchy_default_override;
  bool do_create_linked_overrides_set;

  /**
   * Helpers to mark or unmark an ID as part of the processed (reference of) liboverride
   * hierarchy.
   *
   * \return `true` if the given ID is tagged as missing linked data, `false` otherwise.
   */
  bool id_tag_set(ID *id, const bool is_missing)
  {
    if (do_create_linked_overrides_set) {
      linked_ids_hierarchy_default_override.add(id);
    }
    else if (is_missing) {
      id->tag |= missing_tag;
    }
    else {
      id->tag |= tag;
    }
    return is_missing;
  }
  bool id_tag_clear(ID *id, const bool is_missing)
  {
    if (do_create_linked_overrides_set) {
      linked_ids_hierarchy_default_override.remove(id);
    }
    else if (is_missing) {
      id->tag &= ~missing_tag;
    }
    else {
      id->tag &= ~tag;
    }
    return is_missing;
  }

  /* Mapping linked objects to all their instantiating collections (as a linked list).
   * Avoids calling #BKE_collection_object_find over and over, this function is very expansive. */
  GHash *linked_object_to_instantiating_collections;
  MemArena *mem_arena;

  void clear()
  {
    linked_ids_hierarchy_default_override.clear();
    BLI_ghash_free(linked_object_to_instantiating_collections, nullptr, nullptr);
    BLI_memarena_free(mem_arena);

    bmain = nullptr;
    scene = nullptr;
    id_root_reference = nullptr;
    hierarchy_root_id_reference = nullptr;
    id_root_override = nullptr;
    hierarchy_root_id_override = nullptr;
    tag = 0;
    missing_tag = 0;
    is_override = false;
    is_resync = false;
  }
};

static void lib_override_group_tag_data_object_to_collection_init_collection_process(
    LibOverrideGroupTagData *data, Collection *collection)
{
  LISTBASE_FOREACH (CollectionObject *, collection_object, &collection->gobject) {
    Object *ob = collection_object->ob;
    if (!ID_IS_LINKED(ob)) {
      continue;
    }

    LinkNodePair **collections_linkedlist_p;
    if (!BLI_ghash_ensure_p(data->linked_object_to_instantiating_collections,
                            ob,
                            reinterpret_cast<void ***>(&collections_linkedlist_p)))
    {
      *collections_linkedlist_p = static_cast<LinkNodePair *>(
          BLI_memarena_calloc(data->mem_arena, sizeof(**collections_linkedlist_p)));
    }
    BLI_linklist_append_arena(*collections_linkedlist_p, collection, data->mem_arena);
  }
}

/* Initialize complex data, `data` is expected to be already initialized with basic pointers and
 * other simple data.
 *
 * NOTE: Currently creates a mapping from linked object to all of their instantiating collections
 * (as returned by #BKE_collection_object_find). */
static void lib_override_group_tag_data_object_to_collection_init(LibOverrideGroupTagData *data)
{
  data->mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  data->linked_object_to_instantiating_collections = BLI_ghash_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  if (data->scene != nullptr) {
    lib_override_group_tag_data_object_to_collection_init_collection_process(
        data, data->scene->master_collection);
  }
  LISTBASE_FOREACH (Collection *, collection, &data->bmain->collections) {
    lib_override_group_tag_data_object_to_collection_init_collection_process(data, collection);
  }
}

/* Checks that can decide to skip the ID based only on the matching #MainIDRelationsEntryItem data
 * representing the relationship of that ID with its owner ID. */
static bool lib_override_hierarchy_dependencies_relationship_skip_check(
    MainIDRelationsEntryItem *relation_id_entry)
{
  /* Skip all relationships that should never be taken into account to define a liboverride
   * hierarchy ('from', 'parents', 'owner' etc. pointers). */
  if ((relation_id_entry->usage_flag & IDWALK_CB_OVERRIDE_LIBRARY_NOT_OVERRIDABLE) != 0) {
    return true;
  }
  /* Loop-back pointers (`from` ones) should not be taken into account in liboverride hierarchies.
   *   - They generate an 'inverted' dependency that adds processing and...
   *   - They should always have a regular, 'forward' matching relation anyway. */
  if ((relation_id_entry->usage_flag & IDWALK_CB_LOOPBACK) != 0) {
    return true;
  }
  return false;
}

/* Checks that can decide to skip the ID based on some of its data and the one from its owner. */
static bool lib_override_hierarchy_dependencies_skip_check(ID *owner_id,
                                                           ID *other_id,
                                                           const bool check_override)
{
  /* Skip relationships to null pointer, or to itself. */
  if (ELEM(other_id, nullptr, owner_id)) {
    return true;
  }
  /* Skip any relationships to data from another library. */
  if (other_id->lib != owner_id->lib) {
    return true;
  }
  /* Skip relationships to non-override data if requested. */
  if (check_override) {
    BLI_assert_msg(
        ID_IS_OVERRIDE_LIBRARY(owner_id),
        "When processing liboverrides, the owner ID should always be a liboverride too here.");
    if (!ID_IS_OVERRIDE_LIBRARY(other_id)) {
      return true;
    }
  }
  /* Skip relationships to IDs that should not be involved in liboverrides currently.
   * NOTE: The Scene case is a bit specific:
   *         - While not officially supported, API allow to create liboverrides of whole Scene.
   *         - However, when creating liboverrides from other type of data (e.g. collections or
   *           objects), scenes should really not be considered as part of a hierarchy. If there
   *           are dependencies from other overridden IDs to a scene, this is considered as not
   *           supported (see also #121410). */
#define HIERARCHY_BREAKING_ID_TYPES ID_SCE, ID_LI, ID_SCR, ID_WM, ID_WS
  if (ELEM(GS(other_id->name), HIERARCHY_BREAKING_ID_TYPES) &&
      !ELEM(GS(owner_id->name), HIERARCHY_BREAKING_ID_TYPES))
  {
    return true;
  }
#undef HIERARCHY_BREAKING_ID_TYPES

  return false;
}

static void lib_override_hierarchy_dependencies_recursive_tag_from(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id = data->root_get();
  const bool is_override = data->is_override;

  if ((*reinterpret_cast<uint *>(&id->tag) & data->tag) == 0) {
    /* This ID is not tagged, no reason to proceed further to its parents. */
    return;
  }

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(bmain->relations->relations_from_pointers, id));
  BLI_assert(entry != nullptr);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_FROM) {
    /* This ID has already been processed. */
    return;
  }
  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_FROM;

  for (MainIDRelationsEntryItem *from_id_entry = entry->from_ids; from_id_entry != nullptr;
       from_id_entry = from_id_entry->next)
  {
    if (lib_override_hierarchy_dependencies_relationship_skip_check(from_id_entry)) {
      continue;
    }
    ID *from_id = from_id_entry->id_pointer.from;
    if (lib_override_hierarchy_dependencies_skip_check(id, from_id, is_override)) {
      continue;
    }

    from_id->tag |= data->tag;
    data->root_set(from_id);
    lib_override_hierarchy_dependencies_recursive_tag_from(data);
  }
  data->root_set(id);
}

/* Tag all IDs in dependency relationships within an override hierarchy/group.
 *
 * Requires existing `Main.relations`.
 *
 * NOTE: This is typically called to complete #lib_override_linked_group_tag.
 */
static bool lib_override_hierarchy_dependencies_recursive_tag(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id = data->root_get();
  const bool is_override = data->is_override;
  const bool is_resync = data->is_resync;

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(bmain->relations->relations_from_pointers, id));
  BLI_assert(entry != nullptr);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_TO) {
    /* This ID has already been processed. */
    return (*reinterpret_cast<uint *>(&id->tag) & data->tag) != 0;
  }
  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_TO;

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != nullptr;
       to_id_entry = to_id_entry->next)
  {
    if (lib_override_hierarchy_dependencies_relationship_skip_check(to_id_entry)) {
      continue;
    }
    ID *to_id = *to_id_entry->id_pointer.to;
    if (lib_override_hierarchy_dependencies_skip_check(id, to_id, is_override)) {
      continue;
    }

    data->root_set(to_id);
    if (lib_override_hierarchy_dependencies_recursive_tag(data)) {
      id->tag |= data->tag;
    }
  }
  data->root_set(id);

  /* If the current ID is/has been tagged for override above, then check its reversed dependencies
   * (i.e. IDs that depend on the current one).
   *
   * This will cover e.g. the case where user override an armature, and would expect the mesh
   * object deformed by that armature to also be overridden. */
  if ((*reinterpret_cast<uint *>(&id->tag) & data->tag) != 0 && !is_resync) {
    lib_override_hierarchy_dependencies_recursive_tag_from(data);
  }

  return (*reinterpret_cast<uint *>(&id->tag) & data->tag) != 0;
}

static void lib_override_linked_group_tag_recursive(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id_owner = data->root_get();
  BLI_assert(ID_IS_LINKED(id_owner));
  BLI_assert(!data->is_override);

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(bmain->relations->relations_from_pointers, id_owner));
  BLI_assert(entry != nullptr);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    /* This ID has already been processed. */
    return;
  }
  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != nullptr;
       to_id_entry = to_id_entry->next)
  {
    if (lib_override_hierarchy_dependencies_relationship_skip_check(to_id_entry)) {
      continue;
    }
    ID *to_id = *to_id_entry->id_pointer.to;
    BLI_assert(ID_IS_LINKED(to_id));
    if (lib_override_hierarchy_dependencies_skip_check(id_owner, to_id, false)) {
      continue;
    }

    /* Only tag ID if their usages is tagged as requiring liboverride by default, and the owner is
     * already tagged for liboverride.
     *
     * NOTE: 'in-between' IDs are handled as a separate step, typically by calling
     * #lib_override_hierarchy_dependencies_recursive_tag.
     * NOTE: missing IDs (aka placeholders) are never overridden. */
    if ((to_id_entry->usage_flag & IDWALK_CB_OVERRIDE_LIBRARY_HIERARCHY_DEFAULT) != 0 ||
        data->linked_ids_hierarchy_default_override.contains(to_id))
    {
      if (!data->id_tag_set(to_id, bool(to_id->tag & ID_TAG_MISSING))) {
        /* Only recursively process the dependencies if the owner is tagged for liboverride. */
        data->root_set(to_id);
        lib_override_linked_group_tag_recursive(data);
      }
    }
  }
  data->root_set(id_owner);
}

static bool lib_override_linked_group_tag_collections_keep_tagged_check_recursive(
    LibOverrideGroupTagData *data, Collection *collection)
{
  /* NOTE: Collection's object cache (using bases, as returned by #BKE_collection_object_cache_get)
   * is not usable here, as it may have become invalid from some previous operation and it should
   * not be updated here. So instead only use collections' reliable 'raw' data to check if some
   * object in the hierarchy of the given collection is still tagged for override. */
  for (CollectionObject *collection_object =
           static_cast<CollectionObject *>(collection->gobject.first);
       collection_object != nullptr;
       collection_object = collection_object->next)
  {
    Object *object = collection_object->ob;
    if (object == nullptr) {
      continue;
    }
    if ((object->id.tag & data->tag) != 0 ||
        data->linked_ids_hierarchy_default_override.contains(&object->id))
    {
      return true;
    }
  }

  for (CollectionChild *collection_child =
           static_cast<CollectionChild *>(collection->children.first);
       collection_child != nullptr;
       collection_child = collection_child->next)
  {
    if (lib_override_linked_group_tag_collections_keep_tagged_check_recursive(
            data, collection_child->collection))
    {
      return true;
    }
  }

  return false;
}

static void lib_override_linked_group_tag_clear_boneshapes_objects(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id_root = data->root_get();

  /* Remove (untag) bone shape objects, they shall never need to be to directly/explicitly
   * overridden. */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ob->id.lib != id_root->lib) {
      continue;
    }
    if (ob->type == OB_ARMATURE && ob->pose != nullptr &&
        ((ob->id.tag & data->tag) ||
         data->linked_ids_hierarchy_default_override.contains(&ob->id)))
    {
      for (bPoseChannel *pchan = static_cast<bPoseChannel *>(ob->pose->chanbase.first);
           pchan != nullptr;
           pchan = pchan->next)
      {
        if (pchan->custom != nullptr && &pchan->custom->id != id_root) {
          data->id_tag_clear(&pchan->custom->id, bool(pchan->custom->id.tag & ID_TAG_MISSING));
        }
      }
    }
  }

  /* Remove (untag) collections if they do not own any tagged object (either themselves, or in
   * their children collections). */
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    if (!((collection->id.tag & data->tag) != 0 ||
          data->linked_ids_hierarchy_default_override.contains(&collection->id)) ||
        &collection->id == id_root || collection->id.lib != id_root->lib)
    {
      continue;
    }

    if (!lib_override_linked_group_tag_collections_keep_tagged_check_recursive(data, collection)) {
      data->id_tag_clear(&collection->id, bool(collection->id.tag & ID_TAG_MISSING));
    }
  }
}

/* This will tag at least all 'boundary' linked IDs for a potential override group.
 *
 * Requires existing `Main.relations`.
 *
 * Note that you will then need to call #lib_override_hierarchy_dependencies_recursive_tag to
 * complete tagging of all dependencies within the override group.
 *
 * We currently only consider IDs which usages are marked as to be overridden by default (i.e.
 * tagged with #IDWALK_CB_OVERRIDE_LIBRARY_HIERARCHY_DEFAULT) as valid boundary IDs to define an
 * override group.
 */
static void lib_override_linked_group_tag(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id_root = data->root_get();
  ID *hierarchy_root_id = data->hierarchy_root_get();
  const bool is_resync = data->is_resync;
  BLI_assert(!data->is_override);

  if (id_root->tag & ID_TAG_MISSING) {
    id_root->tag |= data->missing_tag;
    return;
  }

  /* In case this code only process part of the whole hierarchy, it first needs to process the
   * whole linked hierarchy to know which IDs should be overridden anyway, even though in the more
   * limited sub-hierarchy scope they would not be. This is critical for partial resync to work
   * properly.
   *
   * NOTE: Regenerating that Set for every processed sub-hierarchy is not optimal. This is done
   * that way for now to limit the scope of these changes. Better handling is considered a TODO for
   * later (as part of a general refactoring/modernization of this whole code area). */

  const bool use_linked_overrides_set = hierarchy_root_id &&
                                        hierarchy_root_id->lib == id_root->lib &&
                                        hierarchy_root_id != id_root;

  BLI_assert(data->do_create_linked_overrides_set == false);
  if (use_linked_overrides_set) {
    BLI_assert(data->linked_ids_hierarchy_default_override.is_empty());
    data->linked_ids_hierarchy_default_override.add(id_root);
    data->linked_ids_hierarchy_default_override.add(hierarchy_root_id);
    data->do_create_linked_overrides_set = true;

    /* Store recursively all IDs in the hierarchy which should be liboverridden by default. */
    data->root_set(hierarchy_root_id);
    lib_override_linked_group_tag_recursive(data);

    /* Do not override objects used as bone shapes, nor their collections if possible. */
    lib_override_linked_group_tag_clear_boneshapes_objects(data);

    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
    data->root_set(id_root);
    data->do_create_linked_overrides_set = false;
  }

  /* Tag recursively all IDs in the hierarchy which should be liboverridden by default. */
  id_root->tag |= data->tag;
  lib_override_linked_group_tag_recursive(data);

  /* Do not override objects used as bone shapes, nor their collections if possible. */
  lib_override_linked_group_tag_clear_boneshapes_objects(data);

  if (use_linked_overrides_set) {
    data->linked_ids_hierarchy_default_override.clear();
  }

  /* For each object tagged for override, ensure we get at least one local or liboverride
   * collection to host it. Avoids getting a bunch of random object in the scene's master
   * collection when all objects' dependencies are not properly 'packed' into a single root
   * collection.
   *
   * NOTE: In resync case, we do not handle this at all, since:
   *         - In normal, valid cases nothing would be needed anyway (resync process takes care
   *           of tagging needed 'owner' collection then).
   *         - Partial resync makes it extremely difficult to properly handle such extra
   *           collection 'tagging for override' (since one would need to know if the new object
   *           is actually going to replace an already existing override [most common case], or
   *           if it is actually a real new 'orphan' one).
   *         - While not ideal, having objects dangling around is less critical than both points
   *           above.
   *        So if users add new objects to their library override hierarchy in an invalid way, so
   *        be it. Trying to find a collection to override and host this new object would most
   *        likely make existing override very unclean anyway. */
  if (is_resync) {
    return;
  }
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ID_IS_LINKED(ob) && (ob->id.tag & data->tag) != 0) {
      Collection *instantiating_collection = nullptr;
      Collection *instantiating_collection_override_candidate = nullptr;
      /* Loop over all collections instantiating the object, if we already have a 'locale' one we
       * have nothing to do, otherwise try to find a 'linked' one that we can override too. */
      LinkNodePair *instantiating_collection_linklist = static_cast<LinkNodePair *>(
          BLI_ghash_lookup(data->linked_object_to_instantiating_collections, ob));
      if (instantiating_collection_linklist != nullptr) {
        for (LinkNode *instantiating_collection_linknode = instantiating_collection_linklist->list;
             instantiating_collection_linknode != nullptr;
             instantiating_collection_linknode = instantiating_collection_linknode->next)
        {
          instantiating_collection = static_cast<Collection *>(
              instantiating_collection_linknode->link);
          if (!ID_IS_LINKED(instantiating_collection)) {
            /* There is a local collection instantiating the linked object to override, nothing
             * else to be done here. */
            break;
          }
          if (instantiating_collection->id.tag & data->tag ||
              data->linked_ids_hierarchy_default_override.contains(&instantiating_collection->id))
          {
            /* There is a linked collection instantiating the linked object to override,
             * already tagged to be overridden, nothing else to be done here. */
            break;
          }
          instantiating_collection_override_candidate = instantiating_collection;
          instantiating_collection = nullptr;
        }
      }

      if (instantiating_collection == nullptr &&
          instantiating_collection_override_candidate != nullptr)
      {
        data->id_tag_set(
            &instantiating_collection_override_candidate->id,
            bool(instantiating_collection_override_candidate->id.tag & ID_TAG_MISSING));
      }
    }
  }
}

static void lib_override_overrides_group_tag_recursive(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id_owner = data->root_get();
  BLI_assert(ID_IS_OVERRIDE_LIBRARY(id_owner));
  BLI_assert(data->is_override);
  BLI_assert(data->do_create_linked_overrides_set == false);

  ID *id_hierarchy_root = data->hierarchy_root_get();

  if (ID_IS_OVERRIDE_LIBRARY_REAL(id_owner) &&
      (id_owner->override_library->flag & LIBOVERRIDE_FLAG_NO_HIERARCHY) != 0)
  {
    return;
  }

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(bmain->relations->relations_from_pointers, id_owner));
  BLI_assert(entry != nullptr);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    /* This ID has already been processed. */
    return;
  }
  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != nullptr;
       to_id_entry = to_id_entry->next)
  {
    if (lib_override_hierarchy_dependencies_relationship_skip_check(to_id_entry)) {
      continue;
    }
    ID *to_id = *to_id_entry->id_pointer.to;
    if (lib_override_hierarchy_dependencies_skip_check(id_owner, to_id, true)) {
      continue;
    }

    /* Different hierarchy roots are break points in override hierarchies. */
    if (ID_IS_OVERRIDE_LIBRARY_REAL(to_id) &&
        to_id->override_library->hierarchy_root != id_hierarchy_root)
    {
      continue;
    }

    const Library *reference_lib =
        BKE_lib_override_library_get(bmain, id_owner, nullptr, nullptr)->reference->lib;
    const ID *to_id_reference =
        BKE_lib_override_library_get(bmain, to_id, nullptr, nullptr)->reference;
    if (to_id_reference->lib != reference_lib) {
      /* We do not override data-blocks from other libraries, nor do we process them. */
      continue;
    }

    data->id_tag_set(to_id, bool(to_id_reference->tag & ID_TAG_MISSING));

    /* Recursively process the dependencies. */
    data->root_set(to_id);
    lib_override_overrides_group_tag_recursive(data);
  }
  data->root_set(id_owner);
}

/* This will tag all override IDs of an override group defined by the given `id_root`. */
static void lib_override_overrides_group_tag(LibOverrideGroupTagData *data)
{
  ID *id_root = data->root_get();
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_root));
  BLI_assert(data->is_override);
  BLI_assert(data->do_create_linked_overrides_set == false);

  ID *id_hierarchy_root = data->hierarchy_root_get();
  BLI_assert(id_hierarchy_root != nullptr);
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_hierarchy_root));
  UNUSED_VARS_NDEBUG(id_hierarchy_root);

  data->id_tag_set(id_root, bool(id_root->override_library->reference->tag & ID_TAG_MISSING));

  /* Tag all local overrides in id_root's group. */
  lib_override_overrides_group_tag_recursive(data);
}

static bool lib_override_library_create_do(Main *bmain,
                                           Scene *scene,
                                           Library *owner_library,
                                           ID *id_root_reference,
                                           ID *id_hierarchy_root_reference,
                                           const bool do_fully_editable)
{
  BKE_main_relations_create(bmain, 0);
  LibOverrideGroupTagData data{};
  data.bmain = bmain;
  data.scene = scene;
  data.tag = ID_TAG_DOIT;
  data.missing_tag = ID_TAG_MISSING;
  data.is_override = false;
  data.is_resync = false;

  data.root_set(id_root_reference);
  data.hierarchy_root_set(id_hierarchy_root_reference);

  lib_override_group_tag_data_object_to_collection_init(&data);
  lib_override_linked_group_tag(&data);

  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  lib_override_hierarchy_dependencies_recursive_tag(&data);

  /* In case the operation is on an already partially overridden hierarchy, all existing overrides
   * in that hierarchy need to be tagged for remapping from linked reference ID usages to newly
   * created overrides ones. */
  if (id_hierarchy_root_reference->lib != id_root_reference->lib) {
    BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_hierarchy_root_reference));
    BLI_assert(id_hierarchy_root_reference->override_library->reference->lib ==
               id_root_reference->lib);

    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
    data.is_override = true;
    data.root_set(id_hierarchy_root_reference);
    data.hierarchy_root_set(id_hierarchy_root_reference);
    lib_override_overrides_group_tag(&data);
  }

  BKE_main_relations_free(bmain);
  data.clear();

  bool success = false;
  if (id_hierarchy_root_reference->lib != id_root_reference->lib) {
    success = BKE_lib_override_library_create_from_tag(bmain,
                                                       owner_library,
                                                       id_root_reference,
                                                       id_hierarchy_root_reference,
                                                       nullptr,
                                                       false,
                                                       do_fully_editable);
  }
  else {
    success = BKE_lib_override_library_create_from_tag(bmain,
                                                       owner_library,
                                                       id_root_reference,
                                                       nullptr,
                                                       id_hierarchy_root_reference,
                                                       false,
                                                       do_fully_editable);
  }

  /* Cleanup global namemap, to avoid extra processing with regular ID name management. Better to
   * re-create the global namemap on demand. */
  BKE_main_namemap_destroy(&bmain->name_map_global);

  return success;
}

static void lib_override_library_create_post_process(Main *bmain,
                                                     Scene *scene,
                                                     ViewLayer *view_layer,
                                                     const Library *owner_library,
                                                     ID *id_root,
                                                     ID *id_instance_hint,
                                                     Collection *residual_storage,
                                                     const Object *old_active_object,
                                                     const bool is_resync)
{
  /* If there is an old active object, there should also always be a given view layer. */
  BLI_assert(old_active_object == nullptr || view_layer != nullptr);

  /* NOTE: We only care about local IDs here, if a linked object is not instantiated in any way we
   * do not do anything about it. */

  /* We need to use the `_remap` version here as we prevented any LayerCollection resync during the
   * whole liboverride resyncing, which involves a lot of ID remapping.
   *
   * Otherwise, cached Base GHash e.g. can contain invalid stale data. */
  BKE_main_collection_sync_remap(bmain);

  /* We create a set of all objects referenced into the scene by its hierarchy of collections.
   * NOTE: This is different that the list of bases, since objects in excluded collections etc.
   * won't have a base, but are still considered as instanced from our point of view. */
  GSet *all_objects_in_scene = BKE_scene_objects_as_gset(scene, nullptr);

  if (is_resync || id_root == nullptr || id_root->newid == nullptr) {
    /* Instantiating the root collection or object should never be needed in resync case, since the
     * old override would be remapped to the new one. */
  }
  else if (ID_IS_LINKED(id_root->newid) && id_root->newid->lib != owner_library) {
    /* No instantiation in case the root override is linked data, unless it is part of the given
     * owner library.
     *
     * NOTE: that last case should never happen actually in current code? Since non-null owner
     * library should only happen in case of recursive resync, which is already excluded by the
     * previous condition. */
  }
  else if ((id_root->newid->override_library->flag & LIBOVERRIDE_FLAG_NO_HIERARCHY) == 0 &&
           id_root->newid->override_library->hierarchy_root != id_root->newid)
  {
    /* No instantiation in case this is not a hierarchy root, as it can be assumed already handled
     * as part of hierarchy processing. */
  }
  else {
    switch (GS(id_root->name)) {
      case ID_GR: {
        Object *ob_reference = id_instance_hint != nullptr && GS(id_instance_hint->name) == ID_OB ?
                                   reinterpret_cast<Object *>(id_instance_hint) :
                                   nullptr;
        Collection *collection_new = (reinterpret_cast<Collection *>(id_root->newid));
        if (is_resync && BKE_collection_is_in_scene(collection_new)) {
          break;
        }
        if (ob_reference != nullptr) {
          BKE_collection_add_from_object(bmain, scene, ob_reference, collection_new);
        }
        else if (id_instance_hint != nullptr) {
          BLI_assert(GS(id_instance_hint->name) == ID_GR);
          BKE_collection_add_from_collection(
              bmain, scene, (reinterpret_cast<Collection *>(id_instance_hint)), collection_new);
        }
        else {
          BKE_collection_add_from_collection(
              bmain, scene, (reinterpret_cast<Collection *>(id_root)), collection_new);
        }

        BLI_assert(BKE_collection_is_in_scene(collection_new));

        all_objects_in_scene = BKE_scene_objects_as_gset(scene, all_objects_in_scene);
        break;
      }
      case ID_OB: {
        Object *ob_new = reinterpret_cast<Object *>(id_root->newid);
        if (BLI_gset_lookup(all_objects_in_scene, ob_new) == nullptr) {
          BKE_collection_object_add_from(
              bmain, scene, reinterpret_cast<Object *>(id_root), ob_new);
          all_objects_in_scene = BKE_scene_objects_as_gset(scene, all_objects_in_scene);
        }
        break;
      }
      default:
        break;
    }
  }

  if (view_layer != nullptr) {
    BKE_view_layer_synced_ensure(scene, view_layer);
  }
  else {
    BKE_scene_view_layers_synced_ensure(scene);
  }

  /* We need to ensure all new overrides of objects are properly instantiated. */
  Collection *default_instantiating_collection = residual_storage;
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    Object *ob_new = reinterpret_cast<Object *>(ob->id.newid);
    if (ob_new == nullptr || (ID_IS_LINKED(ob_new) && ob_new->id.lib != owner_library)) {
      continue;
    }

    BLI_assert(ob_new->id.override_library != nullptr &&
               ob_new->id.override_library->reference == &ob->id);

    if (old_active_object == ob) {
      BLI_assert(view_layer);
      /* May have been tagged as dirty again in a previous iteration of this loop, e.g. if adding a
       * liboverride object to a collection. */
      BKE_view_layer_synced_ensure(scene, view_layer);
      Base *basact = BKE_view_layer_base_find(view_layer, ob_new);
      if (basact != nullptr) {
        view_layer->basact = basact;
      }
      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    }

    if (BLI_gset_lookup(all_objects_in_scene, ob_new) == nullptr) {
      if (id_root != nullptr && default_instantiating_collection == nullptr) {
        ID *id_ref = id_root->newid != nullptr ? id_root->newid : id_root;
        switch (GS(id_ref->name)) {
          case ID_GR: {
            /* Adding the object to a specific collection outside of the root overridden one is a
             * fairly bad idea (it breaks the override hierarchy concept). But there is no other
             * way to do this currently (we cannot add new collections to overridden root one,
             * this is not currently supported).
             * Since that will be fairly annoying and noisy, only do that in case the override
             * object is not part of any existing collection (i.e. its user count is 0). In
             * practice this should never happen I think. */
            if (ID_REAL_USERS(ob_new) != 0) {
              continue;
            }
            default_instantiating_collection = BKE_id_new<Collection>(bmain, "OVERRIDE_HIDDEN");
            id_us_min(&default_instantiating_collection->id);
            /* Hide the collection from viewport and render. */
            default_instantiating_collection->flag |= COLLECTION_HIDE_VIEWPORT |
                                                      COLLECTION_HIDE_RENDER;
            break;
          }
          case ID_OB: {
            /* Add the other objects to one of the collections instantiating the
             * root object, or scene's master collection if none found. */
            Object *ob_ref = reinterpret_cast<Object *>(id_ref);
            LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
              if (BKE_collection_has_object(collection, ob_ref) &&
                  (view_layer != nullptr ?
                       BKE_view_layer_has_collection(view_layer, collection) :
                       BKE_collection_has_collection(scene->master_collection, collection)) &&
                  !ID_IS_LINKED(collection) && !ID_IS_OVERRIDE_LIBRARY(collection))
              {
                default_instantiating_collection = collection;
              }
            }
            break;
          }
          default:
            break;
        }
      }
      if (default_instantiating_collection == nullptr) {
        default_instantiating_collection = scene->master_collection;
      }

      BKE_collection_object_add(bmain, default_instantiating_collection, ob_new);
      DEG_id_tag_update_ex(bmain, &ob_new->id, ID_RECALC_TRANSFORM | ID_RECALC_BASE_FLAGS);
    }
  }

  if (id_root != nullptr &&
      !ELEM(default_instantiating_collection, nullptr, scene->master_collection))
  {
    ID *id_ref = id_root->newid != nullptr ? id_root->newid : id_root;
    switch (GS(id_ref->name)) {
      case ID_GR:
        BKE_collection_add_from_collection(bmain,
                                           scene,
                                           reinterpret_cast<Collection *>(id_ref),
                                           default_instantiating_collection);
        break;
      default:
        /* Add to master collection. */
        BKE_collection_add_from_collection(
            bmain, scene, nullptr, default_instantiating_collection);
        break;
    }
  }

  BLI_gset_free(all_objects_in_scene, nullptr);
}

bool BKE_lib_override_library_create(Main *bmain,
                                     Scene *scene,
                                     ViewLayer *view_layer,
                                     Library *owner_library,
                                     ID *id_root_reference,
                                     ID *id_hierarchy_root_reference,
                                     ID *id_instance_hint,
                                     ID **r_id_root_override,
                                     const bool do_fully_editable)
{
  if (r_id_root_override != nullptr) {
    *r_id_root_override = nullptr;
  }

  if (id_hierarchy_root_reference == nullptr) {
    id_hierarchy_root_reference = id_root_reference;
  }

  /* While in theory it _should_ be enough to ensure sync of given view-layer (if any), or at least
   * of given scene, think for now it's better to get a fully synced Main at this point, this code
   * may do some very wide remapping/data access in some cases. */
  BKE_main_view_layers_synced_ensure(bmain);
  const Object *old_active_object = (view_layer != nullptr) ?
                                        BKE_view_layer_active_object_get(view_layer) :
                                        nullptr;

  const bool success = lib_override_library_create_do(bmain,
                                                      scene,
                                                      owner_library,
                                                      id_root_reference,
                                                      id_hierarchy_root_reference,
                                                      do_fully_editable);

  if (!success) {
    return success;
  }

  if (r_id_root_override != nullptr) {
    *r_id_root_override = id_root_reference->newid;
  }

  lib_override_library_create_post_process(bmain,
                                           scene,
                                           view_layer,
                                           owner_library,
                                           id_root_reference,
                                           id_instance_hint,
                                           nullptr,
                                           old_active_object,
                                           false);

  /* Cleanup. */
  BKE_main_id_newptr_and_tag_clear(bmain);
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

  /* We need to rebuild some of the deleted override rules (for UI feedback purpose). */
  BKE_lib_override_library_main_operations_create(bmain, true, nullptr);

  return success;
}

static ID *lib_override_root_find(Main *bmain, ID *id, const int curr_level, int *r_best_level)
{
  if (curr_level > 1000) {
    CLOG_ERROR(&LOG,
               "Levels of dependency relationships between library overrides IDs is way too high, "
               "skipping further processing loops (involves at least '%s')",
               id->name);
    return nullptr;
  }

  if (!ID_IS_OVERRIDE_LIBRARY(id)) {
    BLI_assert_unreachable();
    return nullptr;
  }

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(bmain->relations->relations_from_pointers, id));
  BLI_assert(entry != nullptr);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      /* This ID has already been processed. */
      *r_best_level = curr_level;
      return id->override_library->hierarchy_root;
    }

    BLI_assert(id->flag & ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE);
    ID *id_owner;
    int best_level_placeholder = 0;
    BKE_lib_override_library_get(bmain, id, nullptr, &id_owner);
    return lib_override_root_find(bmain, id_owner, curr_level + 1, &best_level_placeholder);
  }

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS) {
    /* Re-processing an entry already being processed higher in the call-graph (re-entry caused by
     * a dependency loops). Just do nothing, there is no more useful info to provide here. */
    return nullptr;
  }
  /* Flag this entry to avoid re-processing it in case some dependency loop leads to it again
   * downwards in the call-stack. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS;

  int best_level_candidate = curr_level;
  ID *best_root_id_candidate = id;

  for (MainIDRelationsEntryItem *from_id_entry = entry->from_ids; from_id_entry != nullptr;
       from_id_entry = from_id_entry->next)
  {
    if (lib_override_hierarchy_dependencies_relationship_skip_check(from_id_entry)) {
      continue;
    }
    ID *from_id = from_id_entry->id_pointer.from;
    if (lib_override_hierarchy_dependencies_skip_check(id, from_id, true)) {
      continue;
    }

    int level_candidate = curr_level + 1;
    /* Recursively process the parent. */
    ID *root_id_candidate = lib_override_root_find(
        bmain, from_id, curr_level + 1, &level_candidate);
    if (level_candidate > best_level_candidate && root_id_candidate != nullptr) {
      best_root_id_candidate = root_id_candidate;
      best_level_candidate = level_candidate;
    }
  }

  if (!ID_IS_OVERRIDE_LIBRARY_REAL(best_root_id_candidate)) {
    BLI_assert(id->flag & ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE);
    ID *id_owner;
    int best_level_placeholder = 0;
    BKE_lib_override_library_get(bmain, best_root_id_candidate, nullptr, &id_owner);
    best_root_id_candidate = lib_override_root_find(
        bmain, id_owner, curr_level + 1, &best_level_placeholder);
  }

  BLI_assert(best_root_id_candidate != nullptr);
  BLI_assert((best_root_id_candidate->flag & ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE) == 0);

  /* This way this ID won't be processed again, should it be encountered again through another
   * relationship hierarchy. */
  entry->tags &= ~MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS;
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  *r_best_level = best_level_candidate;
  return best_root_id_candidate;
}

/**
 * Ensure that the current hierarchy root for the given liboverride #id is valid, i.e. that the
 * root id is effectively one of its ancestors.
 */
static bool lib_override_root_is_valid(Main *bmain, ID *id)
{
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    BLI_assert_unreachable();
    return false;
  }
  ID *id_root = id->override_library->hierarchy_root;
  if (!id_root || !ID_IS_OVERRIDE_LIBRARY_REAL(id_root)) {
    BLI_assert_unreachable();
    return false;
  }

  if (id_root == id) {
    return true;
  }

  blender::VectorSet<ID *> ancestors = {id};
  for (int64_t i = 0; i < ancestors.size(); i++) {
    ID *id_iter = ancestors[i];

    MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
        BLI_ghash_lookup(bmain->relations->relations_from_pointers, id_iter));
    BLI_assert(entry != nullptr);

    for (MainIDRelationsEntryItem *from_id_entry = entry->from_ids; from_id_entry != nullptr;
         from_id_entry = from_id_entry->next)
    {
      if (lib_override_hierarchy_dependencies_relationship_skip_check(from_id_entry)) {
        continue;
      }
      ID *from_id = from_id_entry->id_pointer.from;
      if (lib_override_hierarchy_dependencies_skip_check(id_iter, from_id, true)) {
        continue;
      }

      if (from_id == id_root) {
        /* The hierarchy root is a valid ancestor of the given id. */
        return true;
      }
      ancestors.add(from_id);
    }
  }
  return false;
}

static void lib_override_root_hierarchy_set(
    Main *bmain, ID *id_root, ID *id, ID *id_from, blender::Set<ID *> &processed_ids)
{
  if (processed_ids.contains(id)) {
    /* This ID has already been checked as having a valid hierarchy root, do not attempt to replace
     * it with another one just because it is also used by another liboverride hierarchy. */
    return;
  }
  if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    if (id->override_library->hierarchy_root == id_root) {
      /* Already set, nothing else to do here, sub-hierarchy is also assumed to be properly set
       * then. */
      return;
    }

    /* Hierarchy root already set, and not matching currently proposed one, try to find which is
     * best. */
    if (id->override_library->hierarchy_root != nullptr) {
      /* Check if given `id_from` matches with the hierarchy of the linked reference ID, in which
       * case we assume that the given hierarchy root is the 'real' one.
       *
       * NOTE: This can fail if user mixed dependencies between several overrides of a same
       * reference linked hierarchy. Not much to be done in that case, it's virtually impossible to
       * fix this automatically in a reliable way. */
      if (id_from == nullptr || !ID_IS_OVERRIDE_LIBRARY_REAL(id_from)) {
        /* Too complicated to deal with for now. */
        CLOG_WARN(&LOG,
                  "Inconsistency in library override hierarchy of ID '%s'.\n"
                  "\tNot enough data to verify validity of current proposed root '%s', assuming "
                  "already set one '%s' is valid.",
                  id->name,
                  id_root->name,
                  id->override_library->hierarchy_root->name);
        return;
      }

      ID *id_from_ref = id_from->override_library->reference;
      MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(BLI_ghash_lookup(
          bmain->relations->relations_from_pointers, id->override_library->reference));
      BLI_assert(entry != nullptr);

      /* Enforce replacing hierarchy root if the current one is invalid. */
      bool do_replace_root = (!id->override_library->hierarchy_root ||
                              !ID_IS_OVERRIDE_LIBRARY_REAL(id->override_library->hierarchy_root) ||
                              id->override_library->hierarchy_root->lib != id->lib);
      for (MainIDRelationsEntryItem *from_id_entry = entry->from_ids; from_id_entry != nullptr;
           from_id_entry = from_id_entry->next)
      {
        if (lib_override_hierarchy_dependencies_relationship_skip_check(from_id_entry)) {
          /* Never consider non-overridable relationships as actual dependencies. */
          continue;
        }

        if (id_from_ref == from_id_entry->id_pointer.from) {
          /* A matching parent was found in reference linked data, assume given hierarchy root is
           * the valid one. */
          do_replace_root = true;
          CLOG_WARN(
              &LOG,
              "Inconsistency in library override hierarchy of ID '%s'.\n"
              "\tCurrent proposed root '%s' detected as valid, will replace already set one '%s'.",
              id->name,
              id_root->name,
              id->override_library->hierarchy_root->name);
          break;
        }
      }

      if (!do_replace_root) {
        CLOG_WARN(
            &LOG,
            "Inconsistency in library override hierarchy of ID '%s'.\n"
            "\tCurrent proposed root '%s' not detected as valid, keeping already set one '%s'.",
            id->name,
            id_root->name,
            id->override_library->hierarchy_root->name);
        return;
      }
    }

    CLOG_DEBUG(&LOG,
               "Modifying library override hierarchy of ID '%s'.\n"
               "\tFrom old root '%s' to new root '%s'.",
               id->name,
               id->override_library->hierarchy_root ? id->override_library->hierarchy_root->name :
                                                      "<NONE>",
               id_root->name);

    id->override_library->hierarchy_root = id_root;
  }

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(bmain->relations->relations_from_pointers, id));
  BLI_assert(entry != nullptr);

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != nullptr;
       to_id_entry = to_id_entry->next)
  {
    if (lib_override_hierarchy_dependencies_relationship_skip_check(to_id_entry)) {
      continue;
    }
    ID *to_id = *to_id_entry->id_pointer.to;
    if (lib_override_hierarchy_dependencies_skip_check(id, to_id, true)) {
      continue;
    }

    /* Recursively process the sub-hierarchy. */
    lib_override_root_hierarchy_set(bmain, id_root, to_id, id, processed_ids);
  }
}

static void lib_override_library_main_hierarchy_id_root_ensure(Main *bmain,
                                                               ID *id,
                                                               blender::Set<ID *> &processed_ids)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id));

  if (id->override_library->hierarchy_root != nullptr) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id->override_library->hierarchy_root) ||
        id->override_library->hierarchy_root->lib != id->lib)
    {
      CLOG_ERROR(
          &LOG,
          "Existing override hierarchy root ('%s') for ID '%s' is invalid, will try to find a "
          "new valid one",
          id->override_library->hierarchy_root != nullptr ?
              id->override_library->hierarchy_root->name :
              "<NONE>",
          id->name);
      id->override_library->hierarchy_root = nullptr;
    }
    else if (!lib_override_root_is_valid(bmain, id)) {
      /* Serious invalid cases (likely resulting from bugs or invalid operations) should have
       * been caught by the first check above. Invalid hierarchy roots detected here can happen
       * in normal situations, e.g. when breaking a hierarchy by making one of its components
       * local. See also #137412. */
      CLOG_DEBUG(
          &LOG,
          "Existing override hierarchy root ('%s') for ID '%s' is invalid, will try to find a "
          "new valid one",
          id->override_library->hierarchy_root != nullptr ?
              id->override_library->hierarchy_root->name :
              "<NONE>",
          id->name);
      id->override_library->hierarchy_root = nullptr;
    }
    else {
      /* This ID is considered as having a valid hierarchy root. */
      processed_ids.add(id);
      return;
    }
  }

  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS, false);

  int best_level = 0;
  ID *id_root = lib_override_root_find(bmain, id, best_level, &best_level);

  if (!ELEM(id->override_library->hierarchy_root, id_root, nullptr)) {
    /* In case the detected hierarchy root does not match with the currently defined one, this is
     * likely an issue and is worth a warning. */
    CLOG_WARN(&LOG,
              "Potential inconsistency in library override hierarchy of ID '%s' (current root "
              "%s), detected as part of the hierarchy of '%s' (current root '%s')",
              id->name,
              id->override_library->hierarchy_root != nullptr ?
                  id->override_library->hierarchy_root->name :
                  "<NONE>",
              id_root->name,
              id_root->override_library->hierarchy_root != nullptr ?
                  id_root->override_library->hierarchy_root->name :
                  "<NONE>");
    processed_ids.add(id);
    return;
  }

  lib_override_root_hierarchy_set(bmain, id_root, id, nullptr, processed_ids);

  BLI_assert(id->override_library->hierarchy_root != nullptr);
}

void BKE_lib_override_library_main_hierarchy_root_ensure(Main *bmain)
{
  ID *id;

  BKE_main_relations_create(bmain, 0);
  blender::Set<ID *> processed_ids;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      processed_ids.add(id);
      continue;
    }

    lib_override_library_main_hierarchy_id_root_ensure(bmain, id, processed_ids);
  }
  FOREACH_MAIN_ID_END;

  BKE_main_relations_free(bmain);
}

static void lib_override_library_remap(
    Main *bmain,
    const ID *id_root_reference,
    blender::Vector<std::pair<ID *, ID *>> &references_and_new_overrides,
    GHash *linkedref_to_old_override)
{
  id::IDRemapper remapper_overrides_old_to_new;
  blender::Vector<ID *> nomain_ids;
  blender::Vector<ID *> new_overrides;

  /* Used to ensure that newly created overrides have all of their 'linked id' pointers remapped to
   * the matching override if it exists. Necessary because in partial resync case, some existing
   * liboverride may be used by the resynced ones, yet they would not be part of the resynced
   * partial hierarchy, so #BKE_lib_override_library_create_from_tag cannot find them and handle
   * their remapping. */
  id::IDRemapper remapper_overrides_reference_to_old;

  /* Add remapping from old to new overrides. */
  for (auto [id_reference, id_override_new] : references_and_new_overrides) {
    new_overrides.append(id_override_new);
    ID *id_override_old = static_cast<ID *>(
        BLI_ghash_lookup(linkedref_to_old_override, id_reference));
    if (id_override_old == nullptr) {
      continue;
    }
    remapper_overrides_old_to_new.add(id_override_old, id_override_new);
  }

  GHashIterator linkedref_to_old_override_iter;
  GHASH_ITER (linkedref_to_old_override_iter, linkedref_to_old_override) {
    /* Remap no-main override IDs we just created too. */
    ID *id_override_old_iter = static_cast<ID *>(
        BLI_ghashIterator_getValue(&linkedref_to_old_override_iter));
    if ((id_override_old_iter->tag & ID_TAG_NO_MAIN) != 0) {
      nomain_ids.append(id_override_old_iter);
    }
    /* And remap linked data to old (existing, unchanged) overrides, when no new one was created.
     */
    ID *id_reference_iter = static_cast<ID *>(
        BLI_ghashIterator_getKey(&linkedref_to_old_override_iter));

    /* NOTE: Usually `id_reference_iter->lib == id_root_reference->lib` should always be true.
     * However, there are some cases where it is not, e.g. if the linked reference of a liboverride
     * is relocated to another ID in another library. */
#if 0
    BLI_assert(id_reference_iter->lib == id_root_reference->lib);
    UNUSED_VARS_NDEBUG(id_root_reference);
#else
    UNUSED_VARS(id_root_reference);
#endif
    if (!id_reference_iter->newid) {
      remapper_overrides_reference_to_old.add(id_reference_iter, id_override_old_iter);
    }
  }

  /* Remap all IDs to use the new override. */
  BKE_libblock_remap_multiple(bmain, remapper_overrides_old_to_new, 0);
  BKE_libblock_relink_multiple(bmain,
                               nomain_ids,
                               ID_REMAP_TYPE_REMAP,
                               remapper_overrides_old_to_new,
                               ID_REMAP_FORCE_USER_REFCOUNT | ID_REMAP_FORCE_NEVER_NULL_USAGE);
  /* In new overrides, remap linked ID to their matching already existing overrides. */
  BKE_libblock_relink_multiple(bmain,
                               new_overrides,
                               ID_REMAP_TYPE_REMAP,
                               remapper_overrides_reference_to_old,
                               ID_REMAP_SKIP_OVERRIDE_LIBRARY);
}

/**
 * Mapping to find suitable missing linked liboverrides to replace by the newly generated linked
 * liboverrides during resync process.
 *
 * \note About Order:
 * In most cases, if there are several virtual linked liboverrides generated with the same base
 * name (like `OBCube.001`, `OBCube.002`, etc.), this mapping system will find the correct one, for
 * the following reasons:
 *  - Order of creation of these virtual IDs in resync process is expected to be stable (i.e.
 * several runs of resync code based on the same linked data would re-create the same virtual
 * liboverride IDs in the same order);
 *  - Order of creation and usage of the mapping data (a FIFO queue) also ensures that the missing
 * placeholder `OBCube.001` is always 're-used' before `OBCube.002`.
 *
 * In case linked data keep being modified, these conditions may fail and the mapping may start to
 * return 'wrong' results. However, this is considered as an acceptable limitation here, since this
 * is mainly a 'best effort' to recover from situations that should not be happening in the first
 * place.
 */

using LibOverrideMissingIDsData_Key = const std::pair<std::string, Library *>;
using LibOverrideMissingIDsData_Value = std::deque<ID *>;
using LibOverrideMissingIDsData =
    std::map<LibOverrideMissingIDsData_Key, LibOverrideMissingIDsData_Value>;

/* Return a key suitable for the missing IDs mapping, i.e. a pair of
 * `<full ID name (including first two ID type chars) without a potential numeric extension,
 *   ID library>`.
 *
 * So e.g. returns `<"OBMyObject", lib>` for ID from `lib` with names like `"OBMyObject"`,
 * `"OBMyObject.002"`, `"OBMyObject.12345"`, and so on, but _not_ `"OBMyObject.12.002"`.
 */
static LibOverrideMissingIDsData_Key lib_override_library_resync_missing_id_key(ID *id)
{
  std::string id_name_key(id->name);
  const size_t last_key_index = id_name_key.find_last_not_of("0123456789");

  BLI_assert(last_key_index != std::string::npos);

  if (id_name_key[last_key_index] == '.') {
    id_name_key.resize(last_key_index);
  }

  return LibOverrideMissingIDsData_Key(id_name_key, id->lib);
}

static LibOverrideMissingIDsData lib_override_library_resync_build_missing_ids_data(
    Main *bmain, const bool is_relocate)
{
  LibOverrideMissingIDsData missing_ids;
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (is_relocate) {
      if (!ID_IS_OVERRIDE_LIBRARY(id_iter)) {
        continue;
      }
      const int required_tags = ID_TAG_LIBOVERRIDE_NEED_RESYNC;
      if ((id_iter->tag & required_tags) != required_tags) {
        continue;
      }
    }
    else { /* Handling of missing linked liboverrides. */
      if (!ID_IS_LINKED(id_iter)) {
        continue;
      }
      const int required_tags = (ID_TAG_MISSING | ID_TAG_LIBOVERRIDE_NEED_RESYNC);
      if ((id_iter->tag & required_tags) != required_tags) {
        continue;
      }
    }

    LibOverrideMissingIDsData_Key key = lib_override_library_resync_missing_id_key(id_iter);
    std::pair<LibOverrideMissingIDsData::iterator, bool> value = missing_ids.try_emplace(
        key, LibOverrideMissingIDsData_Value());
    value.first->second.push_back(id_iter);
  }
  FOREACH_MAIN_ID_END;

  return missing_ids;
}

static ID *lib_override_library_resync_search_missing_ids_data(
    LibOverrideMissingIDsData &missing_ids, ID *id_override)
{
  LibOverrideMissingIDsData_Key key = lib_override_library_resync_missing_id_key(id_override);
  const LibOverrideMissingIDsData::iterator value = missing_ids.find(key);
  if (value == missing_ids.end()) {
    return nullptr;
  }
  if (value->second.empty()) {
    return nullptr;
  }
  ID *match_id = value->second.front();
  value->second.pop_front();
  return match_id;
}

static bool lib_override_library_resync(
    Main *bmain,
    const blender::Map<Library *, Library *> *new_to_old_libraries_map,
    Scene *scene,
    ViewLayer *view_layer,
    ID *id_root,
    LinkNode *id_resync_roots,
    ListBase *no_main_ids_list,
    Collection *override_resync_residual_storage,
    const bool do_hierarchy_enforce,
    const bool do_post_process,
    BlendFileReadReport *reports)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_root));

  ID *id_root_reference = id_root->override_library->reference;
  ID *id;

  const Object *old_active_object = nullptr;
  if (view_layer) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    old_active_object = BKE_view_layer_active_object_get(view_layer);
  }
  else {
    BKE_scene_view_layers_synced_ensure(scene);
  }

  if (id_root_reference->tag & ID_TAG_MISSING) {
    BKE_reportf(reports != nullptr ? reports->reports : nullptr,
                RPT_ERROR,
                "Impossible to resync data-block %s and its dependencies, as its linked reference "
                "is missing",
                id_root->name + 2);
    return false;
  }

  BKE_main_relations_create(bmain, 0);
  LibOverrideGroupTagData data{};
  data.bmain = bmain;
  data.scene = scene;
  data.tag = ID_TAG_DOIT;
  data.missing_tag = ID_TAG_MISSING;
  data.is_override = true;
  data.is_resync = true;

  data.root_set(id_root);
  data.hierarchy_root_set(id_root->override_library->hierarchy_root);

  lib_override_group_tag_data_object_to_collection_init(&data);

  /* Mapping 'linked reference IDs' -> 'Local override IDs' of existing overrides, populated from
   * each sub-tree that actually needs to be resynced. */
  GHash *linkedref_to_old_override = BLI_ghash_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);

  /* Only tag linked IDs from related linked reference hierarchy that are actually part of
   * the sub-trees of each detected sub-roots needing resync. */
  for (LinkNode *resync_root_link = id_resync_roots; resync_root_link != nullptr;
       resync_root_link = resync_root_link->next)
  {
    ID *id_resync_root = static_cast<ID *>(resync_root_link->link);
    BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_resync_root));

    if ((id_resync_root->tag & ID_TAG_NO_MAIN) != 0) {
      CLOG_ERROR(&LOG_RESYNC,
                 "While dealing with root '%s', resync root ID '%s' (%p) found to be alreaady "
                 "resynced.\n",
                 id_root->name,
                 id_resync_root->name,
                 id_resync_root);
    }
    //    if (no_main_ids_list && BLI_findindex(no_main_ids_list, id_resync_root) != -1) {
    //      CLOG_ERROR(
    //          &LOG,
    //          "While dealing with root '%s', resync root ID '%s' found to be alreaady
    //          resynced.\n", id_root->name, id_resync_root->name);
    //    }

    ID *id_resync_root_reference = id_resync_root->override_library->reference;

    if (id_resync_root_reference->tag & ID_TAG_MISSING) {
      BKE_reportf(
          reports != nullptr ? reports->reports : nullptr,
          RPT_ERROR,
          "Impossible to resync data-block %s and its dependencies, as its linked reference "
          "is missing",
          id_root->name + 2);
      BLI_ghash_free(linkedref_to_old_override, nullptr, nullptr);
      BKE_main_relations_free(bmain);
      data.clear();
      return false;
    }

    /* Tag local overrides of the current resync sub-hierarchy. */
    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
    data.is_override = true;
    data.root_set(id_resync_root);
    lib_override_overrides_group_tag(&data);

    /* Tag reference data matching the current resync sub-hierarchy. */
    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
    data.is_override = false;
    data.root_set(id_resync_root->override_library->reference);
    data.hierarchy_root_set(
        id_resync_root->override_library->hierarchy_root->override_library->reference);
    lib_override_linked_group_tag(&data);

    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
    lib_override_hierarchy_dependencies_recursive_tag(&data);

    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      if ((id->lib != id_root->lib) || !ID_IS_OVERRIDE_LIBRARY(id)) {
        continue;
      }

      /* IDs that get fully removed from linked data remain as local overrides (using place-holder
       * linked IDs as reference), but they are often not reachable from any current valid local
       * override hierarchy anymore. This will ensure they get properly deleted at the end of this
       * function. */
      if (!ID_IS_LINKED(id) && ID_IS_OVERRIDE_LIBRARY_REAL(id) &&
          (id->override_library->reference->tag & ID_TAG_MISSING) != 0 &&
          /* Unfortunately deleting obdata means deleting their objects too. Since there is no
           * guarantee that a valid override object using an obsolete override obdata gets properly
           * updated, we ignore those here for now. In practice this should not be a big issue. */
          !OB_DATA_SUPPORT_ID(GS(id->name)))
      {
        id->tag |= ID_TAG_MISSING;
      }

      /* While this should not happen in typical cases (and won't be properly supported here),
       * user is free to do all kind of very bad things, including having different local
       * overrides of a same linked ID in a same hierarchy. */
      IDOverrideLibrary *id_override_library = BKE_lib_override_library_get(
          bmain, id, nullptr, nullptr);

      if (id_override_library->hierarchy_root != id_root->override_library->hierarchy_root) {
        continue;
      }

      ID *reference_id = id_override_library->reference;
      if (GS(reference_id->name) != GS(id->name)) {
        switch (GS(id->name)) {
          case ID_KE:
            reference_id = reinterpret_cast<ID *>(BKE_key_from_id(reference_id));
            break;
          case ID_GR:
            BLI_assert(GS(reference_id->name) == ID_SCE);
            reference_id = reinterpret_cast<ID *>(
                reinterpret_cast<Scene *>(reference_id)->master_collection);
            break;
          case ID_NT:
            reference_id = reinterpret_cast<ID *>(blender::bke::node_tree_from_id(id));
            break;
          default:
            break;
        }
      }
      if (reference_id == nullptr) {
        /* Can happen e.g. when there is a local override of a shape-key, but the matching linked
         * obdata (mesh etc.) does not have any shape-key anymore. */
        continue;
      }
      BLI_assert(GS(reference_id->name) == GS(id->name));

      if (!BLI_ghash_haskey(linkedref_to_old_override, reference_id)) {
        BLI_ghash_insert(linkedref_to_old_override, reference_id, id);
        if (!ID_IS_OVERRIDE_LIBRARY_REAL(id) || (id->tag & ID_TAG_DOIT) == 0) {
          continue;
        }
        if ((id->override_library->reference->tag & ID_TAG_DOIT) == 0) {
          /* We have an override, but now it does not seem to be necessary to override that ID
           * anymore. Check if there are some actual overrides from the user, otherwise assume
           * that we can get rid of this local override. */
          if (BKE_lib_override_library_is_user_edited(id)) {
            id->override_library->reference->tag |= ID_TAG_DOIT;
          }
        }
      }
    }
    FOREACH_MAIN_ID_END;

    /* Code above may have added some tags, we need to update this too. */
    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
    lib_override_hierarchy_dependencies_recursive_tag(&data);
  }

  /* Tag all local overrides of the current hierarchy. */
  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  data.is_override = true;
  data.root_set(id_root);
  data.hierarchy_root_set(id_root->override_library->hierarchy_root);
  lib_override_overrides_group_tag(&data);

  BKE_main_relations_free(bmain);
  data.clear();

  /* Make new override from linked data. */
  /* Note that this call also remaps all pointers of tagged IDs from old override IDs to new
   * override IDs (including within the old overrides themselves, since those are tagged too
   * above). */
  const bool success = BKE_lib_override_library_create_from_tag(
      bmain,
      nullptr,
      id_root_reference,
      id_root->override_library->hierarchy_root,
      nullptr,
      true,
      false);

  if (!success) {
    BLI_ghash_free(linkedref_to_old_override, nullptr, nullptr);
    return success;
  }

  /* This used to be the library of the root reference. Should always be the same as the current
   * library on readfile case, but may differ when relocating linked data from a library to
   * another (See #BKE_blendfile_id_relocate and #BKE_blendfile_library_relocate). */
  Library *id_root_reference_lib_old = (new_to_old_libraries_map ?
                                            new_to_old_libraries_map->lookup_default(
                                                id_root_reference->lib, id_root_reference->lib) :
                                            id_root_reference->lib);
  const bool is_relocate = id_root_reference_lib_old != id_root_reference->lib;

  /* Get a mapping of all missing linked IDs that were liboverrides, to search for 'old
   * liboverrides' for newly created ones that do not already have one, in next step. */
  LibOverrideMissingIDsData missing_ids_data = lib_override_library_resync_build_missing_ids_data(
      bmain, is_relocate);
  /* Vector of pairs of reference IDs, and their new override IDs. */
  blender::Vector<std::pair<ID *, ID *>> references_and_new_overrides;

  ListBase *lb;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    ID *id_reference_iter;
    FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id_reference_iter) {
      if ((id_reference_iter->tag & ID_TAG_DOIT) == 0 || id_reference_iter->newid == nullptr ||
          !ELEM(id_reference_iter->lib, id_root_reference->lib, id_root_reference_lib_old))
      {
        continue;
      }
      ID *id_override_new = id_reference_iter->newid;
      references_and_new_overrides.append(std::make_pair(id_reference_iter, id_override_new));

      ID *id_override_old = static_cast<ID *>(
          BLI_ghash_lookup(linkedref_to_old_override, id_reference_iter));

      BLI_assert((id_override_new->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) == 0);

      /* We need to 'move back' newly created override into its proper library (since it was
       * duplicated from the reference ID with 'no main' option, it should currently be the same
       * as the reference ID one). */
      BLI_assert(/*!ID_IS_LINKED(id_override_new) || */ id_override_new->lib ==
                 id_reference_iter->lib);
      BLI_assert(id_override_old == nullptr || id_override_old->lib == id_root->lib);
      id_override_new->lib = id_root->lib;

      /* The old override may have been created as linked data and then referenced by local data
       * during a previous Blender session, in which case it became directly linked and a reference
       * to it was stored in the local .blend file. however, since that linked liboverride ID does
       * not actually exist in the original library file, on next file read it is lost and marked
       * as missing ID. */
      if (id_override_old == nullptr && (ID_IS_LINKED(id_override_new) || is_relocate)) {
        id_override_old = lib_override_library_resync_search_missing_ids_data(missing_ids_data,
                                                                              id_override_new);
        BLI_assert(id_override_old == nullptr || id_override_old->lib == id_override_new->lib);
        if (id_override_old != nullptr) {
          BLI_ghash_insert(linkedref_to_old_override, id_reference_iter, id_override_old);

          Key *key_override_old = BKE_key_from_id(id_override_old);
          Key *key_reference_iter = BKE_key_from_id(id_reference_iter);
          if (key_reference_iter && key_override_old) {
            BLI_ghash_insert(
                linkedref_to_old_override, &key_reference_iter->id, &key_override_old->id);
          }

          CLOG_DEBUG(&LOG_RESYNC,
                     "Found missing linked old override best-match %s for new linked override %s",
                     id_override_old->name,
                     id_override_new->name);
        }
      }

      /* Remap step below will tag directly linked ones properly as needed. */
      if (ID_IS_LINKED(id_override_new)) {
        id_override_new->tag |= ID_TAG_INDIRECT;
      }

      if (id_override_old != nullptr) {
        /* Swap the names between old override ID and new one. */
        char id_name_buf[MAX_ID_NAME];
        memcpy(id_name_buf, id_override_old->name, sizeof(id_name_buf));
        memcpy(id_override_old->name, id_override_new->name, sizeof(id_override_old->name));
        memcpy(id_override_new->name, id_name_buf, sizeof(id_override_new->name));

        BLI_insertlinkreplace(lb, id_override_old, id_override_new);
        id_override_old->tag |= ID_TAG_NO_MAIN;
        id_override_new->tag &= ~ID_TAG_NO_MAIN;

        lib_override_object_posemode_transfer(id_override_new, id_override_old);

        /* Missing old liboverrides cannot transfer their override rules to new liboverride.
         * This is fine though, since these are expected to only be 'virtual' linked overrides
         * generated by resync of linked overrides. So nothing is expected to be overridden here.
         */
        if (ID_IS_OVERRIDE_LIBRARY_REAL(id_override_new) &&
            (id_override_old->tag & ID_TAG_MISSING) == 0)
        {
          BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_override_old));

          id_override_new->override_library->flag = id_override_old->override_library->flag;

          /* NOTE: Since `runtime->tag` is not copied from old to new liboverride, the potential
           * `LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT` is kept on the old, to-be-freed
           * liboverride, and the new one is assumed to be properly part of its hierarchy again. */

          /* Copy over overrides rules from old override ID to new one. */
          BLI_duplicatelist(&id_override_new->override_library->properties,
                            &id_override_old->override_library->properties);
          IDOverrideLibraryProperty *op_new = static_cast<IDOverrideLibraryProperty *>(
              id_override_new->override_library->properties.first);
          IDOverrideLibraryProperty *op_old = static_cast<IDOverrideLibraryProperty *>(
              id_override_old->override_library->properties.first);
          for (; op_new; op_new = op_new->next, op_old = op_old->next) {
            lib_override_library_property_copy(op_new, op_old);
          }
        }

        BLI_addtail(no_main_ids_list, id_override_old);
      }
      else {
        /* Add to proper main list, ensure unique name for local ID, sort, and clear relevant
         * tags. */
        BKE_libblock_management_main_add(bmain, id_override_new);
      }
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;

  /* We remap old to new override usages in a separate step, after all new overrides have
   * been added to Main.
   *
   * This function also ensures that newly created overrides get all their linked ID pointers
   * remapped to a valid override one, whether new or already existing. In partial resync case,
   * #BKE_lib_override_library_create_from_tag cannot reliably discover _all_ valid existing
   * overrides used by the newly resynced ones, since the local resynced hierarchy may not contain
   * them. */
  lib_override_library_remap(
      bmain, id_root_reference, references_and_new_overrides, linkedref_to_old_override);

  BKE_main_collection_sync(bmain);

  blender::Vector<ID *> id_override_old_vector;

  /* We need to apply override rules in a separate loop, after all ID pointers have been properly
   * remapped, and all new local override IDs have gotten their proper original names, otherwise
   * override operations based on those ID names would fail. */
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if ((id->tag & ID_TAG_DOIT) == 0 || id->newid == nullptr || id->lib != id_root_reference->lib)
    {
      continue;
    }

    ID *id_override_new = id->newid;
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_override_new)) {
      continue;
    }

    ID *id_override_old = static_cast<ID *>(BLI_ghash_lookup(linkedref_to_old_override, id));
    if (id_override_old == nullptr) {
      continue;
    }

    if (ID_IS_OVERRIDE_LIBRARY_REAL(id_override_old)) {
      /* The remapping from old to new liboverrides above has a sad side effect on ShapeKeys. Since
       * old liboverrides are also remapped, it means that the old liboverride owner of the shape
       * key is also now pointing to the new liboverride shape key, not the old one. Since shape
       * keys do not own their liboverride data, the old liboverride shape key user has to be
       * restored to use the old liboverride shape-key, otherwise applying shape key override
       * operations will be useless (would apply using the new, from linked data, liboverride,
       * being effectively a no-op). */
      Key **key_override_old_p = BKE_key_from_id_p(id_override_old);
      if (key_override_old_p != nullptr && *key_override_old_p != nullptr) {
        Key *key_linked_reference = BKE_key_from_id(id_override_new->override_library->reference);
        BLI_assert(key_linked_reference != nullptr);
        BLI_assert(key_linked_reference->id.newid == &(*key_override_old_p)->id);
        Key *key_override_old = static_cast<Key *>(
            BLI_ghash_lookup(linkedref_to_old_override, &key_linked_reference->id));
        BLI_assert(key_override_old != nullptr);
        *key_override_old_p = key_override_old;
      }

      /* Apply rules on new override ID using old one as 'source' data. */
      /* Note that since we already remapped ID pointers in old override IDs to new ones, we
       * can also apply ID pointer override rules safely here. */
      PointerRNA rnaptr_src = RNA_id_pointer_create(id_override_old);
      PointerRNA rnaptr_dst = RNA_id_pointer_create(id_override_new);

      /* In case the parent of the liboverride object matches hierarchy-wise the parent of its
       * linked reference, also enforce clearing any override of the other related parenting
       * settings.
       *
       * While this may break some rare use-cases, in almost all situations the best behavior here
       * is to follow the values from the reference data (especially when it comes to the invert
       * parent matrix). */
      bool do_clear_parenting_override = false;
      if (GS(id_override_new->name) == ID_OB) {
        Object *ob_old = reinterpret_cast<Object *>(id_override_old);
        Object *ob_new = reinterpret_cast<Object *>(id_override_new);
        if (ob_new->parent && ob_new->parent != ob_old->parent &&
            /* Parent is not a liboverride. */
            (ob_new->parent ==
                 reinterpret_cast<Object *>(ob_new->id.override_library->reference)->parent ||
             /* Parent is a hierarchy-matching liboverride. */
             (ID_IS_OVERRIDE_LIBRARY_REAL(ob_new->parent) &&
              reinterpret_cast<Object *>(ob_new->parent->id.override_library->reference) ==
                  reinterpret_cast<Object *>(ob_new->id.override_library->reference)->parent)))
        {
          do_clear_parenting_override = true;
        }
      }

      /* We remove any operation tagged with `LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE`,
       * that way the potentially new pointer will be properly kept, when old one is still valid
       * too (typical case: assigning new ID to some usage, while old one remains used elsewhere
       * in the override hierarchy). */
      LISTBASE_FOREACH_MUTABLE (
          IDOverrideLibraryProperty *, op, &id_override_new->override_library->properties)
      {
        LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
          if (opop->flag & LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE) {
            lib_override_library_property_operation_clear(opop);
            BLI_freelinkN(&op->operations, opop);
          }
        }
        if (BLI_listbase_is_empty(&op->operations)) {
          BKE_lib_override_library_property_delete(id_override_new->override_library, op);
        }
        else if (do_clear_parenting_override) {
          if (strstr(op->rna_path, "matrix_parent_inverse") ||
              strstr(op->rna_path, "parent_type") || strstr(op->rna_path, "parent_bone") ||
              strstr(op->rna_path, "parent_vertices"))
          {
            CLOG_DEBUG(&LOG_RESYNC,
                       "Deleting liboverride property '%s' from object %s, as its parent pointer "
                       "matches the reference data hierarchy wise",
                       id_override_new->name + 2,
                       op->rna_path);
            BKE_lib_override_library_property_delete(id_override_new->override_library, op);
          }
        }
      }

      RNA_struct_override_apply(bmain,
                                &rnaptr_dst,
                                &rnaptr_src,
                                nullptr,
                                id_override_new->override_library,
                                do_hierarchy_enforce ? RNA_OVERRIDE_APPLY_FLAG_IGNORE_ID_POINTERS :
                                                       RNA_OVERRIDE_APPLY_FLAG_NOP);

      /* Clear the old shape key pointer again, otherwise it won't make ID management code happy
       * when freeing (at least from user count side of things). */
      if (key_override_old_p != nullptr) {
        *key_override_old_p = nullptr;
      }
    }

    id_override_old_vector.append(id_override_old);
  }
  FOREACH_MAIN_ID_END;

  /* Once overrides have been properly 'transferred' from old to new ID, we can clear ID usages
   * of the old one.
   * This is necessary in case said old ID is not in Main anymore. */
  id::IDRemapper id_remapper;
  BKE_libblock_relink_multiple(bmain,
                               id_override_old_vector,
                               ID_REMAP_TYPE_CLEANUP,
                               id_remapper,
                               ID_REMAP_FORCE_USER_REFCOUNT | ID_REMAP_FORCE_NEVER_NULL_USAGE);
  for (ID *id_override_old : id_override_old_vector) {
    id_override_old->tag |= ID_TAG_NO_USER_REFCOUNT;
  }
  id_override_old_vector.clear();

  /* Delete old override IDs.
   * Note that we have to use tagged group deletion here, since ID deletion also uses
   * ID_TAG_DOIT. This improves performances anyway, so everything is fine. */
  int user_edited_overrides_deletion_count = 0;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (id->tag & ID_TAG_DOIT) {
      /* Since this code can also be called on linked liboverride now (during recursive resync),
       * order of processing cannot guarantee anymore that the old liboverride won't be tagged for
       * deletion before being processed by this loop (which would then untag it again).
       *
       * So instead store old liboverrides in Main into a temp list again, and do the tagging
       * separately once this loop over all IDs in main is done. */
      if (id->newid != nullptr && id->lib == id_root_reference->lib) {
        ID *id_override_old = static_cast<ID *>(BLI_ghash_lookup(linkedref_to_old_override, id));

        if (id_override_old != nullptr) {
          id->newid->tag &= ~ID_TAG_DOIT;
          if (id_override_old->tag & ID_TAG_NO_MAIN) {
            id_override_old->tag |= ID_TAG_DOIT;
            BLI_assert(BLI_findindex(no_main_ids_list, id_override_old) != -1);
          }
          else {
            /* Defer tagging. */
            id_override_old_vector.append(id_override_old);
          }
        }
      }
      id->tag &= ~ID_TAG_DOIT;
    }
    /* Also deal with old overrides that went missing in new linked data - only for real local
     * overrides for now, not those who are linked. */
    else if (id->tag & ID_TAG_MISSING && !ID_IS_LINKED(id) && ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      bool do_delete;
      ID *hierarchy_root = id->override_library->hierarchy_root;
      if (id->override_library->reference->lib->id.tag & ID_TAG_MISSING) {
        /* Do not delete overrides which reference is missing because the library itself is missing
         * (ref. #100586). */
        do_delete = false;
      }
      else if (hierarchy_root != nullptr &&
               hierarchy_root->override_library->reference->tag & ID_TAG_MISSING)
      {
        /* Do not delete overrides which root hierarchy reference is missing. This would typically
         * cause more harm than good. */
        do_delete = false;
      }
      else if (!BKE_lib_override_library_is_user_edited(id)) {
        /* If user never edited them, we can delete them. */
        do_delete = true;
        CLOG_DEBUG(&LOG_RESYNC, "Old override %s is being deleted", id->name);
      }
#if 0
      else {
        /* Otherwise, keep them, user needs to decide whether what to do with them. */
        BLI_assert((id->tag & ID_TAG_DOIT) == 0);
        do_delete = false;
        id_fake_user_set(id);
        id->flag |= ID_FLAG_LIB_OVERRIDE_RESYNC_LEFTOVER;
        CLOG_DEBUG(&LOG_RESYNC,
                  "Old override %s is being kept around as it was user-edited",
                  id->name);
      }
#else
      else {
        /* Delete them nevertheless, with fat warning, user needs to decide whether they want to
         * save that version of the file (and accept the loss), or not. */
        do_delete = true;
        CLOG_WARN(&LOG_RESYNC,
                  "Old override %s is being deleted even though it was user-edited",
                  id->name);
        user_edited_overrides_deletion_count++;
      }
#endif
      if (do_delete) {
        id->tag |= ID_TAG_DOIT;
        id->tag &= ~ID_TAG_MISSING;
      }
      else if (id->override_library->runtime != nullptr) {
        /* Cleanup of this temporary tag, since that somewhat broken liboverride is explicitly
         * kept for now. */
        id->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT;
      }
    }
  }
  FOREACH_MAIN_ID_END;

  /* Finalize tagging old liboverrides for deletion. */
  for (ID *id_override_old : id_override_old_vector) {
    id_override_old->tag |= ID_TAG_DOIT;
  }
  id_override_old_vector.clear();

  /* Cleanup, many pointers in this GHash are already invalid now. */
  BLI_ghash_free(linkedref_to_old_override, nullptr, nullptr);

  BKE_id_multi_tagged_delete(bmain);

  /* At this point, `id_root` may have been resynced, therefore deleted. In that case we need to
   * update it to its new version.
   */
  if (id_root_reference->newid != nullptr) {
    id_root = id_root_reference->newid;
  }

  if (user_edited_overrides_deletion_count > 0) {
    BKE_reportf(reports != nullptr ? reports->reports : nullptr,
                RPT_WARNING,
                "During resync of data-block %s, %d obsolete overrides were deleted, that had "
                "local changes defined by user",
                id_root->name + 2,
                user_edited_overrides_deletion_count);
  }

  if (do_post_process) {
    /* Essentially ensures that potentially new overrides of new objects will be instantiated. */
    /* NOTE: Here 'reference' collection and 'newly added' collection are the same, which is fine
     * since we already relinked old root override collection to new resync'ed one above. So this
     * call is not expected to instantiate this new resync'ed collection anywhere, just to ensure
     * that we do not have any stray objects. */
    lib_override_library_create_post_process(bmain,
                                             scene,
                                             view_layer,
                                             nullptr,
                                             id_root_reference,
                                             id_root,
                                             override_resync_residual_storage,
                                             old_active_object,
                                             true);
  }

  /* Cleanup. */
  BKE_main_id_newptr_and_tag_clear(bmain);
  /* That one should not be needed in fact, as #BKE_id_multi_tagged_delete call above should have
   * deleted all tagged IDs. */
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

  return success;
}

/** Cleanup: Remove unused 'place holder' linked IDs. */
static void lib_override_cleanup_after_resync(Main *bmain)
{
  LibQueryUnusedIDsData parameters;
  parameters.do_local_ids = true;
  parameters.do_linked_ids = true;
  parameters.do_recursive = true;
  parameters.filter_fn = [](const ID *id) -> bool {
    if (ID_IS_LINKED(id) && (id->tag & ID_TAG_MISSING) != 0) {
      return true;
    }
    /* This is a fairly complex case.
     *
     * LibOverride resync process takes care of removing 'no more valid' liboverrides (see at the
     * end of #lib_override_library_main_resync_on_library_indirect_level). However, since it does
     * not resync data which linked reference is missing (see
     * #lib_override_library_main_resync_id_skip_check), these are kept 'as is'. Indeed,
     * liboverride resync code cannot know if a specific liboverride data is only part of its
     * hierarchy, or if it is also used by some other data (in which case it should be preserved if
     * the linked reference goes missing).
     *
     * So instead, we consider these cases as also valid candidates for deletion here, since the
     * whole recursive process in `BKE_lib_query_unused_ids_tag` will ensure that if there is still
     * any valid user of these, they won't get tagged for deletion.
     *
     * Also, do not delete 'orphaned' liboverrides if it's a hierarchy root, or if its hierarchy
     * root's reference is missing, since this is much more likely a case of actual missing data,
     * rather than changes in the liboverride's hierarchy in the linked data.
     */
    if (ID_IS_OVERRIDE_LIBRARY(id)) {
      const IDOverrideLibrary *override_library = BKE_lib_override_library_get(
          nullptr, id, nullptr, nullptr);
      /* NOTE: Since hierarchy root final validation is only done on 'expected-to-be-processed' IDs
       * after each level of recursive resync (and not the whole Main data-base, see @00375abc38),
       * `root` may now be null here. Likely, because deleting some root override sets that pointer
       * to null.
       *
       * If this turns out to be a wrong fix, the other likely solution would be to add a call to
       * #BKE_lib_override_library_main_hierarchy_root_ensure before calling
       * #lib_override_cleanup_after_resync.
       *
       * This was technically already possible before (e.g. for non-hierarchical isolated
       * liboverrides), but in practice this is very rare case. */
      const ID *root = override_library->hierarchy_root;
      if (root && (root == id || (root->override_library->reference->tag & ID_TAG_MISSING) != 0)) {
        return false;
      }
      return ((override_library->reference->tag & ID_TAG_MISSING) != 0);
    }
    return false;
  };
  BKE_lib_query_unused_ids_tag(bmain, ID_TAG_DOIT, parameters);
  if (parameters.num_total[INDEX_ID_NULL]) {
    CLOG_DEBUG(&LOG_RESYNC,
               "Deleting %d unused linked missing IDs and their unused liboverrides (including %d "
               "local ones)\n",
               parameters.num_total[INDEX_ID_NULL],
               parameters.num_local[INDEX_ID_NULL]);
  }
  BKE_id_multi_tagged_delete(bmain);
}

bool BKE_lib_override_library_resync(Main *bmain,
                                     Scene *scene,
                                     ViewLayer *view_layer,
                                     ID *id_root,
                                     Collection *override_resync_residual_storage,
                                     const bool do_hierarchy_enforce,
                                     BlendFileReadReport *reports)
{
  ListBase no_main_ids_list = {nullptr};
  LinkNode id_resync_roots{};
  id_resync_roots.link = id_root;
  id_resync_roots.next = nullptr;

  const bool success = lib_override_library_resync(bmain,
                                                   nullptr,
                                                   scene,
                                                   view_layer,
                                                   id_root,
                                                   &id_resync_roots,
                                                   &no_main_ids_list,
                                                   override_resync_residual_storage,
                                                   do_hierarchy_enforce,
                                                   true,
                                                   reports);

  LISTBASE_FOREACH_MUTABLE (ID *, id_iter, &no_main_ids_list) {
    BKE_id_free(bmain, id_iter);
  }

  /* Cleanup global namemap, to avoid extra processing with regular ID name management. Better to
   * re-create the global namemap on demand. */
  BKE_main_namemap_destroy(&bmain->name_map_global);

  lib_override_cleanup_after_resync(bmain);

  return success;
}

static bool lib_override_resync_id_lib_level_is_valid(ID *id,
                                                      const int library_indirect_level,
                                                      const bool do_strict_equal)
{
  const int id_lib_level = (ID_IS_LINKED(id) ? id->lib->runtime->temp_index : 0);
  return do_strict_equal ? id_lib_level == library_indirect_level :
                           id_lib_level <= library_indirect_level;
}

/* Check ancestors overrides for resync, to ensure all IDs in-between two tagged-for-resync ones
 * are also properly tagged.
 *
 * WARNING: Expects `bmain` to have valid relation data.
 *
 * Returns `true` if it finds an ancestor within the current liboverride hierarchy also tagged as
 * needing resync, `false` otherwise.
 *
 * NOTE: If `check_only` is true, it only does the check and returns, without any modification to
 * the data.
 */
static void lib_override_resync_tagging_finalize_recurse(Main *bmain,
                                                         ID *id_root,
                                                         ID *id_from,
                                                         const int library_indirect_level,
                                                         bool is_in_partial_resync_hierarchy)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_root));
  BLI_assert(id_root->override_library->hierarchy_root != nullptr);

  if (!lib_override_resync_id_lib_level_is_valid(id_root, library_indirect_level, false)) {
    CLOG_ERROR(
        &LOG,
        "While processing indirect level %d, ID %s from lib %s of indirect level %d detected "
        "as needing resync, skipping",
        library_indirect_level,
        id_root->name,
        id_root->lib ? id_root->lib->filepath : "<LOCAL>",
        id_root->lib ? id_root->lib->runtime->temp_index : 0);
    id_root->tag &= ~ID_TAG_LIBOVERRIDE_NEED_RESYNC;
    return;
  }

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(bmain->relations->relations_from_pointers, id_root));
  BLI_assert(entry != nullptr);

  bool is_reprocessing_current_entry = false;
  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS) {
    /* This ID is already being processed, this indicates a dependency loop. */
    BLI_assert((entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) == 0);

    if (id_root->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) {
      /* This ID is already tagged for resync, then the loop leading back to it is also fully
       * tagged for resync, nothing else to do. */
      BLI_assert(is_in_partial_resync_hierarchy);
      return;
    }
    if (!is_in_partial_resync_hierarchy) {
      /* This ID is not tagged for resync, and is part of a loop where none of the other IDs are
       * tagged for resync, nothing else to do. */
      return;
    }
    /* This ID is not yet tagged for resync, but is part of a loop which is (partially) tagged
     * for resync.
     * The whole loop needs to be processed a second time to ensure all of its members are properly
     * tagged for resync then. */
    is_reprocessing_current_entry = true;

    CLOG_DEBUG(
        &LOG,
        "ID %s (%p) is detected as part of a hierarchy dependency loop requiring resync, it "
        "is now being re-processed to ensure proper tagging of the whole loop",
        id_root->name,
        id_root->lib);
  }
  else if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    /* This ID has already been processed. */
    BLI_assert((entry->tags & MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS) == 0);

    /* If it was already detected as needing resync, then its whole sub-tree should also be fully
     * processed. Only need to ensure that it is not tagged as potential partial resync root
     * anymore, if now processed as part of another partial resync hierarchy. */
    if (id_root->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) {
      if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_DOIT && is_in_partial_resync_hierarchy) {
        CLOG_DEBUG(
            &LOG,
            "ID %s (%p) was marked as a potential root for partial resync, but it is used by "
            "%s (%p), which is also tagged for resync, so it is not a root after all",
            id_root->name,
            id_root->lib,
            id_from->name,
            id_from->lib);

        entry->tags &= ~MAINIDRELATIONS_ENTRY_TAGS_DOIT;
      }
      return;
    }
    /* Else, if it is not being processed as part of a resync hierarchy, nothing more to do either,
     * its current status and the one of its whole dependency tree is also assumed valid. */
    if (!is_in_partial_resync_hierarchy) {
      return;
    }

    /* Else, this ID was processed before and not detected as needing resync, but it now needs
     * resync, so its whole sub-tree needs to be re-processed to be properly tagged as needing
     * resync. */
    entry->tags &= ~MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;
  }

  if (is_in_partial_resync_hierarchy) {
    BLI_assert(id_from != nullptr);

    if ((id_root->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) == 0) {
      CLOG_DEBUG(&LOG,
                 "ID %s (%p) now tagged as needing resync because they are used by %s (%p) "
                 "that needs to be resynced",
                 id_root->name,
                 id_root->lib,
                 id_from->name,
                 id_from->lib);
      id_root->tag |= ID_TAG_LIBOVERRIDE_NEED_RESYNC;
    }
  }
  else if (id_root->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) {
    /* Not yet within a partial resync hierarchy, and this ID is tagged for resync, it is a
     * potential partial resync root. */
    is_in_partial_resync_hierarchy = true;
  }

  /* Temporary tag to help manage dependency loops. */
  if (!is_reprocessing_current_entry) {
    BLI_assert((entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) == 0);
    entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS;

    /* Since this ID is reached from the hierarchy root, it is not isolated from it. */
    if (id_root->override_library->hierarchy_root != id_root &&
        id_root->override_library->runtime != nullptr)
    {
      id_root->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT;
    }
  }

  /* Check the whole sub-tree hierarchy of this ID. */
  for (MainIDRelationsEntryItem *entry_item = entry->to_ids; entry_item != nullptr;
       entry_item = entry_item->next)
  {
    if (lib_override_hierarchy_dependencies_relationship_skip_check(entry_item)) {
      continue;
    }
    ID *id_to = *(entry_item->id_pointer.to);
    /* Ensure the 'real' override is processed, in case `id_to` is e.g. an embedded ID, get its
     * owner instead. */
    BKE_lib_override_library_get(bmain, id_to, nullptr, &id_to);

    if (lib_override_hierarchy_dependencies_skip_check(id_root, id_to, true)) {
      continue;
    }
    BLI_assert_msg(ID_IS_OVERRIDE_LIBRARY_REAL(id_to),
                   "Check above ensured `id_to` is a liboverride, so it should be a real one (not "
                   "an embedded one)");

    /* Non-matching hierarchy root IDs mean this is not the same liboverride hierarchy anymore. */
    if (id_to->override_library->hierarchy_root != id_root->override_library->hierarchy_root) {
      continue;
    }

    lib_override_resync_tagging_finalize_recurse(
        bmain, id_to, id_root, library_indirect_level, is_in_partial_resync_hierarchy);

    /* Call above may have changed that status in case of dependency loop, update it for the next
     * dependency processing. */
    is_in_partial_resync_hierarchy = (id_root->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) != 0;
  }

  if (!is_reprocessing_current_entry) {
    BLI_assert((entry->tags & MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS) != 0);
    BLI_assert((entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) == 0);

    entry->tags &= ~MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS;
    entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

    if (is_in_partial_resync_hierarchy &&
        (id_from == nullptr || (id_from->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) == 0))
    {
      /* This ID (and its whole sub-tree of dependencies) is now considered as processed. If it is
       * tagged for resync, but its 'calling parent' is not, it is a potential partial resync root.
       */
      CLOG_DEBUG(
          &LOG_RESYNC, "Potential root for partial resync: %s (%p)", id_root->name, id_root->lib);
      entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_DOIT;
    }
  }
}

/* Return true if the ID should be skipped for resync given current context. */
static bool lib_override_library_main_resync_id_skip_check(ID *id,
                                                           const int library_indirect_level)
{
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    return true;
  }

  if (!lib_override_resync_id_lib_level_is_valid(id, library_indirect_level, true)) {
    return true;
  }

  /* Do not attempt to resync from missing data. */
  if (((id->tag | id->override_library->reference->tag) & ID_TAG_MISSING) != 0) {
    return true;
  }

  if (id->override_library->flag & LIBOVERRIDE_FLAG_NO_HIERARCHY) {
    /* This ID is not part of an override hierarchy. */
    BLI_assert((id->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) == 0);
    return true;
  }

  /* Do not attempt to resync when hierarchy root is missing, this would usually do more harm
   * than good. */
  ID *hierarchy_root = id->override_library->hierarchy_root;
  if (hierarchy_root == nullptr ||
      ((hierarchy_root->tag | hierarchy_root->override_library->reference->tag) &
       ID_TAG_MISSING) != 0)
  {
    return true;
  }

  return false;
}

/**
 * Clear 'unreachable' tag of existing liboverrides if they are using another reachable liboverride
 * (typical case: Mesh object which only relationship to the rest of the liboverride hierarchy is
 * through its 'parent' pointer (i.e. rest of the hierarchy has no actual relationship to this mesh
 * object).
 *
 * Logic and rational of this function are very similar to these of
 * #lib_override_hierarchy_dependencies_recursive_tag_from, but withing specific resync context.
 *
 * \returns True if it finds a non-isolated 'parent' ID, false otherwise.
 */
static bool lib_override_resync_tagging_finalize_recursive_check_from(
    Main *bmain, ID *id, const int library_indirect_level)
{
  BLI_assert(!lib_override_library_main_resync_id_skip_check(id, library_indirect_level));

  if (id->override_library->hierarchy_root == id ||
      (id->override_library->runtime->tag & LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT) == 0)
  {
    BLI_assert(
        id->override_library->hierarchy_root != id || id->override_library->runtime == nullptr ||
        (id->override_library->runtime->tag & LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT) == 0);
    return true;
  }

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(bmain->relations->relations_from_pointers, id));
  BLI_assert(entry != nullptr);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_TO) {
    /* This ID has already been processed, since 'true' conditions have already been checked above,
     * it is validated as an isolated liboverride. */
    return false;
  }
  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_TO;

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != nullptr;
       to_id_entry = to_id_entry->next)
  {
    if (lib_override_hierarchy_dependencies_relationship_skip_check(to_id_entry)) {
      continue;
    }
    ID *to_id = *(to_id_entry->id_pointer.to);
    if (lib_override_library_main_resync_id_skip_check(to_id, library_indirect_level)) {
      continue;
    }

    if (lib_override_resync_tagging_finalize_recursive_check_from(
            bmain, to_id, library_indirect_level))
    {
      id->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT;
      return true;
    }
  }

  return false;
}

/* Once all IDs needing resync have been tagged, partial ID roots can be found by processing each
 * tagged-for-resync IDs' ancestors within their liboverride hierarchy. */
static void lib_override_resync_tagging_finalize(Main *bmain,
                                                 GHash *id_roots,
                                                 const int library_indirect_level)
{
  ID *id_iter;

  /* Tag all IDs to be processed, which are real liboverrides part of a hierarchy, and not the
   * root of their hierarchy, as potentially isolated from their hierarchy root. */
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (lib_override_library_main_resync_id_skip_check(id_iter, library_indirect_level)) {
      continue;
    }

    if (!ELEM(id_iter->override_library->hierarchy_root, id_iter, nullptr)) {
      override_library_runtime_ensure(id_iter->override_library)->tag |=
          LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT;
    }
  }
  FOREACH_MAIN_ID_END;

  /* Finalize all IDs needing tagging for resync, and tag partial resync roots. Will also clear the
   * 'isolated' tag from all processed IDs. */
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (lib_override_library_main_resync_id_skip_check(id_iter, library_indirect_level)) {
      continue;
    }

    /* Only process hierarchy root IDs here. */
    if (id_iter->override_library->hierarchy_root != id_iter) {
      continue;
    }

    lib_override_resync_tagging_finalize_recurse(
        bmain, id_iter, nullptr, library_indirect_level, false);
  }
  FOREACH_MAIN_ID_END;

#ifndef NDEBUG
  /* Validation loop to ensure all entries have been processed as expected by
   * `lib_override_resync_tagging_finalize_recurse`, in above loop. */
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (lib_override_library_main_resync_id_skip_check(id_iter, library_indirect_level)) {
      continue;
    }
    if ((id_iter->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) == 0) {
      continue;
    }

    MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
        BLI_ghash_lookup(bmain->relations->relations_from_pointers, id_iter));
    BLI_assert(entry != nullptr);
    BLI_assert((entry->tags & MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS) == 0);

    if ((entry->tags & MAINIDRELATIONS_ENTRY_TAGS_DOIT) == 0) {
      continue;
    }

    BLI_assert(entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED);
  }
  FOREACH_MAIN_ID_END;
#endif

  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);

  /* Process above cleared all IDs actually still in relation with their root from the tag.
   *
   * The only exception being IDs only in relation with their root through a 'reversed' from
   * pointer (typical case: armature object is the hierarchy root, its child mesh object is only
   * related to it through its own 'parent' pointer, the armature one has no 'to' relationships to
   * its deformed mesh object.
   *
   * Remaining ones are in a limbo, typically they could have been removed or moved around in the
   * hierarchy (e.g. an object moved into another sub-collection). Tag them as needing resync,
   * actual resyncing code will handle them as needed. */
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (lib_override_library_main_resync_id_skip_check(id_iter, library_indirect_level)) {
      continue;
    }

    if (!ELEM(id_iter->override_library->hierarchy_root, id_iter, nullptr) &&
        (id_iter->override_library->runtime->tag & LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT))
    {
      /* Check and clear 'isolated' tags from cases like child objects of a hierarchy root object.
       * Sigh. */
      if (lib_override_resync_tagging_finalize_recursive_check_from(
              bmain, id_iter, library_indirect_level))
      {
        BLI_assert((id_iter->override_library->runtime->tag &
                    LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT) == 0);
        CLOG_DEBUG(&LOG_RESYNC,
                   "ID %s (%p) detected as only related to its hierarchy root by 'reversed' "
                   "relationship(s) (e.g. object parenting), tagging it as needing "
                   "resync",
                   id_iter->name,
                   id_iter->lib);
      }
      else {
        CLOG_DEBUG(
            &LOG_RESYNC,
            "ID %s (%p) detected as 'isolated' from its hierarchy root, tagging it as needing "
            "resync",
            id_iter->name,
            id_iter->lib);
      }
      id_iter->tag |= ID_TAG_LIBOVERRIDE_NEED_RESYNC;
    }
  }
  FOREACH_MAIN_ID_END;

  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);

  /* If no tagged-for-resync ancestor was found, but the iterated ID is tagged for resync, then it
   * is a root of a resync sub-tree. Find the root of the whole override hierarchy and add the
   * iterated ID as one of its resync sub-tree roots. */
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (lib_override_library_main_resync_id_skip_check(id_iter, library_indirect_level)) {
      continue;
    }
    if ((id_iter->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) == 0) {
      continue;
    }

    MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
        BLI_ghash_lookup(bmain->relations->relations_from_pointers, id_iter));
    BLI_assert(entry != nullptr);

    if ((entry->tags & MAINIDRELATIONS_ENTRY_TAGS_DOIT) == 0) {
      continue;
    }

    ID *hierarchy_root = id_iter->override_library->hierarchy_root;
    BLI_assert(hierarchy_root->lib == id_iter->lib);

    if (id_iter != hierarchy_root) {
      CLOG_DEBUG(&LOG_RESYNC,
                 "Found root ID '%s' for partial resync root ID '%s'",
                 hierarchy_root->name,
                 id_iter->name);

      BLI_assert(hierarchy_root->override_library != nullptr);

      BLI_assert((id_iter->override_library->runtime->tag &
                  LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT) == 0);
    }

    LinkNodePair **id_resync_roots_p;
    if (!BLI_ghash_ensure_p(
            id_roots, hierarchy_root, reinterpret_cast<void ***>(&id_resync_roots_p)))
    {
      *id_resync_roots_p = MEM_callocN<LinkNodePair>(__func__);
    }
    BLI_linklist_append(*id_resync_roots_p, id_iter);
  }
  FOREACH_MAIN_ID_END;

  BKE_main_relations_tag_set(
      bmain,
      static_cast<const eMainIDRelationsEntryTags>(MAINIDRELATIONS_ENTRY_TAGS_PROCESSED |
                                                   MAINIDRELATIONS_ENTRY_TAGS_DOIT |
                                                   MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS),
      false);
}

/* Ensure resync of all overrides at one level of indirect usage.
 *
 * We need to handle each level independently, since an override at level n may be affected by
 * other overrides from level n + 1 etc. (i.e. from linked overrides it may use).
 */
static bool lib_override_library_main_resync_on_library_indirect_level(
    Main *bmain,
    const blender::Map<Library *, Library *> *new_to_old_libraries_map,
    Scene *scene,
    ViewLayer *view_layer,
    Collection *override_resync_residual_storage,
    const int library_indirect_level,
    BlendFileReadReport *reports)
{
  const bool do_reports_recursive_resync_timing = (library_indirect_level != 0);
  const double init_time = do_reports_recursive_resync_timing ? BLI_time_now_seconds() : 0.0;

  BKE_main_relations_create(bmain, 0);
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

  /* NOTE: in code below, the order in which `FOREACH_MAIN_ID_BEGIN` processes ID types ensures
   * that we always process 'higher-level' overrides first (i.e. scenes, then collections, then
   * objects, then other types). */

  /* Detect all linked data that would need to be overridden if we had to create an override from
   * those used by current existing overrides. */
  LibOverrideGroupTagData data = {};
  data.bmain = bmain;
  data.scene = scene;
  data.tag = ID_TAG_DOIT;
  data.missing_tag = ID_TAG_MISSING;
  data.is_override = false;
  data.is_resync = true;
  lib_override_group_tag_data_object_to_collection_init(&data);
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (lib_override_library_main_resync_id_skip_check(id, library_indirect_level)) {
      continue;
    }

    if (id->tag & (ID_TAG_DOIT | ID_TAG_MISSING)) {
      /* We already processed that ID as part of another ID's hierarchy. */
      continue;
    }

    data.root_set(id->override_library->reference);
    lib_override_linked_group_tag(&data);
    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
    lib_override_hierarchy_dependencies_recursive_tag(&data);
    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  }
  FOREACH_MAIN_ID_END;
  data.clear();

  GHash *id_roots = BLI_ghash_ptr_new(__func__);

  /* Now check existing overrides, those needing resync will be the one either already tagged as
   * such, or the one using linked data that is now tagged as needing override. */
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (lib_override_library_main_resync_id_skip_check(id, library_indirect_level)) {
      continue;
    }

    if (id->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) {
      CLOG_DEBUG(
          &LOG_RESYNC, "ID %s (%p) was already tagged as needing resync", id->name, id->lib);
      if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
        override_library_runtime_ensure(id->override_library)->tag |=
            LIBOVERRIDE_TAG_NEED_RESYNC_ORIGINAL;
      }
      continue;
    }

    MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
        BLI_ghash_lookup(bmain->relations->relations_from_pointers, id));
    BLI_assert(entry != nullptr);

    for (MainIDRelationsEntryItem *entry_item = entry->to_ids; entry_item != nullptr;
         entry_item = entry_item->next)
    {
      if (lib_override_hierarchy_dependencies_relationship_skip_check(entry_item)) {
        continue;
      }
      ID *id_to = *entry_item->id_pointer.to;

      /* Case where this ID pointer was to a linked ID, that now needs to be overridden. */
      if (ID_IS_LINKED(id_to) && (id_to->lib != id->lib) && (id_to->tag & ID_TAG_DOIT) != 0) {
        CLOG_DEBUG(&LOG_RESYNC,
                   "ID %s (%p) now tagged as needing resync because they use linked %s (%p) that "
                   "now needs to be overridden",
                   id->name,
                   id->lib,
                   id_to->name,
                   id_to->lib);
        id->tag |= ID_TAG_LIBOVERRIDE_NEED_RESYNC;
        break;
      }
    }
  }
  FOREACH_MAIN_ID_END;

  /* Handling hierarchy relations for final tagging needs to happen after all IDs in a given
   * hierarchy have been tagged for resync in previous loop above. Otherwise, some resync roots may
   * be missing. */
  lib_override_resync_tagging_finalize(bmain, id_roots, library_indirect_level);

#ifndef NDEBUG
  /* Check for validity/integrity of the computed set of root IDs, and their sub-branches defined
   * by their resync root IDs. */
  {
    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
    GHashIterator *id_roots_iter = BLI_ghashIterator_new(id_roots);
    while (!BLI_ghashIterator_done(id_roots_iter)) {
      ID *id_root = static_cast<ID *>(BLI_ghashIterator_getKey(id_roots_iter));
      LinkNodePair *id_resync_roots = static_cast<LinkNodePair *>(
          BLI_ghashIterator_getValue(id_roots_iter));
      CLOG_DEBUG(&LOG_RESYNC,
                 "Checking validity of computed TODO data for root '%s'... \n",
                 id_root->name);

      if (id_root->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) {
        LinkNode *id_resync_root_iter = id_resync_roots->list;
        ID *id_resync_root = static_cast<ID *>(id_resync_root_iter->link);

        if (id_resync_roots->list != id_resync_roots->last_node || id_resync_root != id_root) {
          CLOG_ERROR(&LOG_RESYNC,
                     "Hierarchy root ID is tagged for resync, yet it is not the only partial "
                     "resync roots, this should not happen."
                     "\n\tRoot ID: %s"
                     "\n\tFirst Resync root ID: %s"
                     "\n\tLast Resync root ID: %s",
                     id_root->name,
                     static_cast<ID *>(id_resync_roots->list->link)->name,
                     static_cast<ID *>(id_resync_roots->last_node->link)->name);
        }
      }
      for (LinkNode *id_resync_root_iter = id_resync_roots->list; id_resync_root_iter != nullptr;
           id_resync_root_iter = id_resync_root_iter->next)
      {
        ID *id_resync_root = static_cast<ID *>(id_resync_root_iter->link);
        BLI_assert(id_resync_root == id_root || !BLI_ghash_haskey(id_roots, id_resync_root));
        if (id_resync_root == id_root) {
          if (id_resync_root_iter != id_resync_roots->list ||
              id_resync_root_iter != id_resync_roots->last_node)
          {
            CLOG_ERROR(&LOG_RESYNC,
                       "Resync root ID is same as root ID of the override hierarchy, yet other "
                       "resync root IDs are also defined, this should not happen at this point."
                       "\n\tRoot ID: %s"
                       "\n\tFirst Resync root ID: %s"
                       "\n\tLast Resync root ID: %s",
                       id_root->name,
                       static_cast<ID *>(id_resync_roots->list->link)->name,
                       static_cast<ID *>(id_resync_roots->last_node->link)->name);
          }
        }
      }
      BLI_ghashIterator_step(id_roots_iter);
    }
    BLI_ghashIterator_free(id_roots_iter);
  }
#endif

  BKE_main_relations_free(bmain);
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

  ListBase no_main_ids_list = {nullptr};

  GHashIterator *id_roots_iter = BLI_ghashIterator_new(id_roots);
  while (!BLI_ghashIterator_done(id_roots_iter)) {
    ID *id_root = static_cast<ID *>(BLI_ghashIterator_getKey(id_roots_iter));
    Library *library = id_root->lib;
    LinkNodePair *id_resync_roots = static_cast<LinkNodePair *>(
        BLI_ghashIterator_getValue(id_roots_iter));

    if (ID_IS_LINKED(id_root)) {
      id_root->lib->runtime->tag |= LIBRARY_TAG_RESYNC_REQUIRED;
    }

    CLOG_DEBUG(&LOG_RESYNC,
               "Resyncing all dependencies under root %s (%p), first one being '%s'...",
               id_root->name,
               reinterpret_cast<void *>(library),
               reinterpret_cast<ID *>(id_resync_roots->list->link)->name);
    const bool success = lib_override_library_resync(bmain,
                                                     new_to_old_libraries_map,
                                                     scene,
                                                     view_layer,
                                                     id_root,
                                                     id_resync_roots->list,
                                                     &no_main_ids_list,
                                                     override_resync_residual_storage,
                                                     false,
                                                     false,
                                                     reports);
    CLOG_DEBUG(&LOG_RESYNC, "\tSuccess: %d", success);
    if (success) {
      reports->count.resynced_lib_overrides++;
      if (library_indirect_level > 0 && reports->do_resynced_lib_overrides_libraries_list &&
          BLI_linklist_index(reports->resynced_lib_overrides_libraries, library) < 0)
      {
        BLI_linklist_prepend(&reports->resynced_lib_overrides_libraries, library);
        reports->resynced_lib_overrides_libraries_count++;
      }
    }

    BLI_linklist_free(id_resync_roots->list, nullptr);
    BLI_ghashIterator_step(id_roots_iter);
  }
  BLI_ghashIterator_free(id_roots_iter);

  LISTBASE_FOREACH_MUTABLE (ID *, id_iter, &no_main_ids_list) {
    BKE_id_free(bmain, id_iter);
  }
  BLI_listbase_clear(&no_main_ids_list);

  /* Just in case, should not be needed in theory, since #lib_override_library_resync should have
   * already cleared them all. */
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

  /* Check there are no left-over IDs needing resync from the current (or higher) level of indirect
   * library level. */
  bool process_lib_level_again = false;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (lib_override_library_main_resync_id_skip_check(id, library_indirect_level)) {
      continue;
    }

    const bool need_resync = (id->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) != 0;
    const bool need_reseync_original = (id->override_library->runtime != nullptr &&
                                        (id->override_library->runtime->tag &
                                         LIBOVERRIDE_TAG_NEED_RESYNC_ORIGINAL) != 0);
    const bool is_isolated_from_root = (id->override_library->runtime != nullptr &&
                                        (id->override_library->runtime->tag &
                                         LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT) != 0);

    if (need_resync && is_isolated_from_root) {
      if (!BKE_lib_override_library_is_user_edited(id)) {
        CLOG_WARN(
            &LOG_RESYNC,
            "Deleting unused ID override %s from library level %d, still found as needing "
            "resync, and being isolated from its hierarchy root. This can happen when its "
            "otherwise unchanged linked reference was moved around in the library file (e.g. if "
            "an object was moved into another sub-collection of the same hierarchy).",
            id->name,
            ID_IS_LINKED(id) ? id->lib->runtime->temp_index : 0);
        id->tag |= ID_TAG_DOIT;
      }
      else {
        CLOG_WARN(
            &LOG_RESYNC,
            "Keeping user-edited ID override %s from library level %d still found as "
            "needing resync, and being isolated from its hierarchy root. This can happen when its "
            "otherwise unchanged linked reference was moved around in the library file (e.g. if "
            "an object was moved into another sub-collection of the same hierarchy).",
            id->name,
            ID_IS_LINKED(id) ? id->lib->runtime->temp_index : 0);
        id->tag &= ~ID_TAG_LIBOVERRIDE_NEED_RESYNC;
        id->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT;
      }
    }
    else if (need_resync) {
      if (need_reseync_original) {
        CLOG_DEBUG(&LOG_RESYNC,
                   "ID override %s from library level %d still found as needing resync after "
                   "tackling library level %d. Since it was originally tagged as such by "
                   "RNA/liboverride apply code, this whole level of library needs to be processed "
                   "another time.",
                   id->name,
                   ID_IS_LINKED(id) ? id->lib->runtime->temp_index : 0,
                   library_indirect_level);
        process_lib_level_again = true;
        /* Cleanup tag for now, will be re-set by next iteration of this function. */
        id->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_NEED_RESYNC_ORIGINAL;
      }
      else {
        /* If it was only tagged for resync as part of resync process itself, it means it was
         * originally inside of a resync hierarchy, but not in the matching reference hierarchy
         * anymore. So it did not actually need to be resynced, simply clear the tag. */
        CLOG_DEBUG(&LOG_RESYNC,
                   "ID override %s from library level %d still found as needing resync after "
                   "tackling library level %d. However, it was not tagged as such by "
                   "RNA/liboverride apply code, so ignoring it",
                   id->name,
                   ID_IS_LINKED(id) ? id->lib->runtime->temp_index : 0,
                   library_indirect_level);
        id->tag &= ~ID_TAG_LIBOVERRIDE_NEED_RESYNC;
      }
    }
    else if (need_reseync_original) {
      /* Just cleanup of temporary tag, the ID has been resynced successfully. */
      id->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_NEED_RESYNC_ORIGINAL;
    }
    else if (is_isolated_from_root) {
      CLOG_ERROR(
          &LOG_RESYNC,
          "ID override %s from library level %d still tagged as isolated from its hierarchy root, "
          "it should have been either properly resynced or removed at that point.",
          id->name,
          ID_IS_LINKED(id) ? id->lib->runtime->temp_index : 0);
      id->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT;
    }
  }
  FOREACH_MAIN_ID_END;

  /* Delete 'isolated from root' remaining IDs tagged in above check loop. */
  BKE_id_multi_tagged_delete(bmain);
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

  BLI_ghash_free(id_roots, nullptr, MEM_freeN);

  /* In some fairly rare (and degenerate) cases, some root ID from other liboverrides may have been
   * freed, and therefore set to nullptr. Attempt to fix this as best as possible. */
  /* WARNING: Cannot use directly #BKE_lib_override_library_main_hierarchy_root_ensure here, as it
   * processes the whole Main content - only the IDs matching current resync scope should be
   * checked here. */
  {
    BKE_main_relations_create(bmain, 0);
    blender::Set<ID *> processed_ids;

    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      if (lib_override_library_main_resync_id_skip_check(id, library_indirect_level)) {
        processed_ids.add(id);
        continue;
      }

      lib_override_library_main_hierarchy_id_root_ensure(bmain, id, processed_ids);
    }
    FOREACH_MAIN_ID_END;

    BKE_main_relations_free(bmain);
  }

  if (do_reports_recursive_resync_timing) {
    reports->duration.lib_overrides_recursive_resync += BLI_time_now_seconds() - init_time;
  }

  return process_lib_level_again;
}

static int lib_override_sort_libraries_func(LibraryIDLinkCallbackData *cb_data)
{
  if (cb_data->cb_flag & IDWALK_CB_LOOPBACK) {
    return IDWALK_RET_NOP;
  }
  ID *id_owner = cb_data->owner_id;
  ID *id = *cb_data->id_pointer;
  if (id != nullptr && ID_IS_LINKED(id) && id->lib != id_owner->lib) {
    const int owner_library_indirect_level = ID_IS_LINKED(id_owner) ?
                                                 id_owner->lib->runtime->temp_index :
                                                 0;
    if (owner_library_indirect_level > 100) {
      CLOG_ERROR(&LOG_RESYNC,
                 "Levels of indirect usages of libraries is way too high, there are most likely "
                 "dependency loops, skipping further building loops (involves at least '%s' from "
                 "'%s' and '%s' from '%s')",
                 id_owner->name,
                 id_owner->lib->filepath,
                 id->name,
                 id->lib->filepath);
      return IDWALK_RET_NOP;
    }
    if (owner_library_indirect_level > 90) {
      CLOG_WARN(
          &LOG_RESYNC,
          "Levels of indirect usages of libraries is suspiciously too high, there are most likely "
          "dependency loops (involves at least '%s' from '%s' and '%s' from '%s')",
          id_owner->name,
          id_owner->lib->filepath,
          id->name,
          id->lib->filepath);
    }

    if (owner_library_indirect_level >= id->lib->runtime->temp_index) {
      id->lib->runtime->temp_index = owner_library_indirect_level + 1;
      *reinterpret_cast<bool *>(cb_data->user_data) = true;
    }
  }
  return IDWALK_RET_NOP;
}

/**
 * Define the `temp_index` of libraries from their highest level of indirect usage.
 *
 * E.g. if lib_a uses lib_b, lib_c and lib_d, and lib_b also uses lib_d, then lib_a has an index of
 * 1, lib_b and lib_c an index of 2, and lib_d an index of 3.
 */
static int lib_override_libraries_index_define(Main *bmain)
{
  LISTBASE_FOREACH (Library *, library, &bmain->libraries) {
    /* index 0 is reserved for local data. */
    library->runtime->temp_index = 1;
  }
  bool do_continue = true;
  while (do_continue) {
    do_continue = false;
    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      /* NOTE: In theory all non-liboverride IDs could be skipped here. This does not gives any
       * performances boost though, so for now keep it as is (i.e. also consider non-liboverride
       * relationships to establish libraries hierarchy). */
      BKE_library_foreach_ID_link(
          bmain, id, lib_override_sort_libraries_func, &do_continue, IDWALK_READONLY);
    }
    FOREACH_MAIN_ID_END;
  }

  int library_indirect_level_max = 0;
  LISTBASE_FOREACH (Library *, library, &bmain->libraries) {
    library_indirect_level_max = std::max(library->runtime->temp_index,
                                          library_indirect_level_max);
  }
  return library_indirect_level_max;
}

void BKE_lib_override_library_main_resync(
    Main *bmain,
    const blender::Map<Library *, Library *> *new_to_old_libraries_map,
    Scene *scene,
    ViewLayer *view_layer,
    BlendFileReadReport *reports)
{
  /* We use a specific collection to gather/store all 'orphaned' override collections and objects
   * generated by re-sync-process. This avoids putting them in scene's master collection. */
#define OVERRIDE_RESYNC_RESIDUAL_STORAGE_NAME "OVERRIDE_RESYNC_LEFTOVERS"
  Collection *override_resync_residual_storage = static_cast<Collection *>(BLI_findstring(
      &bmain->collections, OVERRIDE_RESYNC_RESIDUAL_STORAGE_NAME, offsetof(ID, name) + 2));
  if (override_resync_residual_storage != nullptr &&
      ID_IS_LINKED(override_resync_residual_storage))
  {
    override_resync_residual_storage = nullptr;
  }
  if (override_resync_residual_storage == nullptr) {
    override_resync_residual_storage = BKE_collection_add(
        bmain, scene->master_collection, OVERRIDE_RESYNC_RESIDUAL_STORAGE_NAME);
    /* Hide the collection from viewport and render. */
    override_resync_residual_storage->flag |= COLLECTION_HIDE_VIEWPORT | COLLECTION_HIDE_RENDER;
  }
  /* BKE_collection_add above could have tagged the view_layer out of sync. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Object *old_active_object = BKE_view_layer_active_object_get(view_layer);

  /* Necessary to improve performances, and prevent layers matching override sub-collections to be
   * lost when re-syncing the parent override collection.
   * Ref. #73411. */
  BKE_layer_collection_resync_forbid();

  int library_indirect_level = lib_override_libraries_index_define(bmain);
  while (library_indirect_level >= 0) {
    int level_reprocess_count = 0;
    /* Update overrides from each indirect level separately.
     *
     * About the looping here: It may happen that some sub-hierarchies of liboverride are moved
     * around (the hierarchy in reference data does not match anymore the existing one in
     * liboverride data). In some cases, these sub-hierarchies won't be resynced then. If some IDs
     * in these sub-hierarchies actually do need resync, then the whole process needs to be applied
     * again, until all cases are fully processed.
     *
     * In practice, even in very complex and 'dirty'/outdated production files, typically less than
     * ten reprocesses are enough to cover all cases (in the vast majority of cases, no reprocess
     * is needed at all). */
    while (lib_override_library_main_resync_on_library_indirect_level(
        bmain,
        new_to_old_libraries_map,
        scene,
        view_layer,
        override_resync_residual_storage,
        library_indirect_level,
        reports))
    {
      level_reprocess_count++;
      if (level_reprocess_count > 100) {
        CLOG_WARN(
            &LOG_RESYNC,
            "Need to reprocess resync for library level %d more than %d times, aborting. This is "
            "either caused by extremely complex liboverride hierarchies, or a bug",
            library_indirect_level,
            level_reprocess_count);
        break;
      }
      CLOG_DEBUG(&LOG_RESYNC,
                 "Applying reprocess %d for resyncing at library level %d",
                 level_reprocess_count,
                 library_indirect_level);
    }
    library_indirect_level--;
  }

  BKE_layer_collection_resync_allow();

  /* Essentially ensures that potentially new overrides of new objects will be instantiated. */
  lib_override_library_create_post_process(bmain,
                                           scene,
                                           view_layer,
                                           nullptr,
                                           nullptr,
                                           nullptr,
                                           override_resync_residual_storage,
                                           old_active_object,
                                           true);

  if (BKE_collection_is_empty(override_resync_residual_storage)) {
    BKE_collection_delete(bmain, override_resync_residual_storage, true);
  }

  LISTBASE_FOREACH (Library *, library, &bmain->libraries) {
    if (library->runtime->tag & LIBRARY_TAG_RESYNC_REQUIRED) {
      CLOG_DEBUG(&LOG_RESYNC,
                 "library '%s' contains some linked overrides that required recursive resync, "
                 "consider updating it",
                 library->filepath);
    }
  }

  /* Cleanup global namemap, to avoid extra processing with regular ID name management. Better to
   * re-create the global namemap on demand. */
  BKE_main_namemap_destroy(&bmain->name_map_global);

  lib_override_cleanup_after_resync(bmain);

  BLI_assert(BKE_main_namemap_validate(*bmain));
}

void BKE_lib_override_library_delete(Main *bmain, ID *id_root)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_root));

  /* Tag all library overrides in the chains of dependencies from the given root one. */
  BKE_main_relations_create(bmain, 0);
  LibOverrideGroupTagData data{};
  data.bmain = bmain;
  data.scene = nullptr;
  data.tag = ID_TAG_DOIT;
  data.missing_tag = ID_TAG_MISSING;
  data.is_override = true;
  data.is_resync = false;

  data.root_set(id_root);
  data.hierarchy_root_set(id_root->override_library->hierarchy_root);

  lib_override_group_tag_data_object_to_collection_init(&data);
  lib_override_overrides_group_tag(&data);

  BKE_main_relations_free(bmain);
  data.clear();

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (id->tag & ID_TAG_DOIT) {
      if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
        ID *id_override_reference = id->override_library->reference;

        /* Remap the whole local IDs to use the linked data. */
        BKE_libblock_remap(bmain, id, id_override_reference, ID_REMAP_SKIP_INDIRECT_USAGE);
      }
    }
  }
  FOREACH_MAIN_ID_END;

  /* Delete the override IDs. */
  BKE_id_multi_tagged_delete(bmain);

  /* Should not actually be needed here. */
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
}

void BKE_lib_override_library_make_local(Main *bmain, ID *id)
{
  if (ID_IS_OVERRIDE_LIBRARY_VIRTUAL(id)) {
    /* We should never directly 'make local' virtual overrides (aka shape keys). */
    BLI_assert_unreachable();
    id->flag &= ~ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE;
    return;
  }
  /* Cannot use `ID_IS_OVERRIDE_LIBRARY` here, as we may call this function on some already
   * partially processed liboverrides (e.g. from the #PartialWriteContext code), where the linked
   * reference pointer has already been set to null. */
  if (!id->override_library) {
    return;
  }

  BKE_lib_override_library_free(&id->override_library, true);

  Key *shape_key = BKE_key_from_id(id);
  if (shape_key != nullptr) {
    shape_key->id.flag &= ~ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE;
  }

  if (GS(id->name) == ID_SCE) {
    Collection *master_collection = reinterpret_cast<Scene *>(id)->master_collection;
    if (master_collection != nullptr) {
      master_collection->id.flag &= ~ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE;
    }
  }

  bNodeTree *node_tree = blender::bke::node_tree_from_id(id);
  if (node_tree != nullptr) {
    node_tree->id.flag &= ~ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE;
  }

  /* In case a liboverride hierarchy root is 'made local', i.e. is not a liboverride anymore, all
   * hierarchy roots of all liboverrides need to be validated/re-generated again.
   * Only in case `bmain` is given, otherwise caller is responsible to do this. */
  if (bmain) {
    BKE_lib_override_library_main_hierarchy_root_ensure(bmain);
  }
}

/* We only build override GHash on request. */
BLI_INLINE GHash *override_library_rna_path_mapping_ensure(IDOverrideLibrary *liboverride)
{
  IDOverrideLibraryRuntime *liboverride_runtime = override_library_runtime_ensure(liboverride);
  if (liboverride_runtime->rna_path_to_override_properties == nullptr) {
    liboverride_runtime->rna_path_to_override_properties = BLI_ghash_new(
        BLI_ghashutil_strhash_p_murmur, BLI_ghashutil_strcmp, __func__);
    for (IDOverrideLibraryProperty *op =
             static_cast<IDOverrideLibraryProperty *>(liboverride->properties.first);
         op != nullptr;
         op = op->next)
    {
      BLI_ghash_insert(liboverride_runtime->rna_path_to_override_properties, op->rna_path, op);
    }
  }

  return liboverride_runtime->rna_path_to_override_properties;
}

IDOverrideLibraryProperty *BKE_lib_override_library_property_find(IDOverrideLibrary *liboverride,
                                                                  const char *rna_path)
{
  GHash *liboverride_runtime = override_library_rna_path_mapping_ensure(liboverride);
  return static_cast<IDOverrideLibraryProperty *>(BLI_ghash_lookup(liboverride_runtime, rna_path));
}

IDOverrideLibraryProperty *BKE_lib_override_library_property_get(IDOverrideLibrary *liboverride,
                                                                 const char *rna_path,
                                                                 bool *r_created)
{
  IDOverrideLibraryProperty *op = BKE_lib_override_library_property_find(liboverride, rna_path);

  if (op == nullptr) {
    op = MEM_callocN<IDOverrideLibraryProperty>(__func__);
    op->rna_path = BLI_strdup(rna_path);
    BLI_addtail(&liboverride->properties, op);

    GHash *liboverride_runtime = override_library_rna_path_mapping_ensure(liboverride);
    BLI_ghash_insert(liboverride_runtime, op->rna_path, op);

    if (r_created) {
      *r_created = true;
    }
  }
  else if (r_created) {
    *r_created = false;
  }

  return op;
}

bool BKE_lib_override_rna_property_find(PointerRNA *idpoin,
                                        const IDOverrideLibraryProperty *library_prop,
                                        PointerRNA *r_override_poin,
                                        PropertyRNA **r_override_prop,
                                        int *r_index)
{
  BLI_assert(RNA_struct_is_ID(idpoin->type) && ID_IS_OVERRIDE_LIBRARY(idpoin->data));
  return RNA_path_resolve_property_full(
      idpoin, library_prop->rna_path, r_override_poin, r_override_prop, r_index);
}

void lib_override_library_property_copy(IDOverrideLibraryProperty *op_dst,
                                        IDOverrideLibraryProperty *op_src)
{
  op_dst->rna_path = BLI_strdup(op_src->rna_path);
  BLI_duplicatelist(&op_dst->operations, &op_src->operations);

  for (IDOverrideLibraryPropertyOperation *
           opop_dst = static_cast<IDOverrideLibraryPropertyOperation *>(op_dst->operations.first),
          *opop_src = static_cast<IDOverrideLibraryPropertyOperation *>(op_src->operations.first);
       opop_dst;
       opop_dst = opop_dst->next, opop_src = opop_src->next)
  {
    lib_override_library_property_operation_copy(opop_dst, opop_src);
  }
}

void lib_override_library_property_clear(IDOverrideLibraryProperty *op)
{
  BLI_assert(op->rna_path != nullptr);

  MEM_freeN(op->rna_path);

  LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
    lib_override_library_property_operation_clear(opop);
  }
  BLI_freelistN(&op->operations);
}

bool BKE_lib_override_library_property_rna_path_change(IDOverrideLibrary *liboverride,
                                                       const char *old_rna_path,
                                                       const char *new_rna_path)
{
  /* Find the override property by its old RNA path. */
  GHash *liboverride_runtime = override_library_rna_path_mapping_ensure(liboverride);
  IDOverrideLibraryProperty *liboverride_property = static_cast<IDOverrideLibraryProperty *>(
      BLI_ghash_popkey(liboverride_runtime, old_rna_path, nullptr));

  if (liboverride_property == nullptr) {
    return false;
  }

  /* Switch over the RNA path. */
  MEM_SAFE_FREE(liboverride_property->rna_path);
  liboverride_property->rna_path = BLI_strdup(new_rna_path);

  /* Put property back into the lookup mapping, using the new RNA path. */
  BLI_ghash_insert(liboverride_runtime, liboverride_property->rna_path, liboverride_property);

  return true;
}

static void lib_override_library_property_delete(IDOverrideLibrary *liboverride,
                                                 IDOverrideLibraryProperty *liboverride_property,
                                                 const bool do_runtime_updates)
{
  if (do_runtime_updates &&
      !ELEM(nullptr, liboverride->runtime, liboverride->runtime->rna_path_to_override_properties))
  {
    BLI_ghash_remove(liboverride->runtime->rna_path_to_override_properties,
                     liboverride_property->rna_path,
                     nullptr,
                     nullptr);
  }
  lib_override_library_property_clear(liboverride_property);
  BLI_freelinkN(&liboverride->properties, liboverride_property);
}

bool BKE_lib_override_library_property_search_and_delete(IDOverrideLibrary *liboverride,
                                                         const char *rna_path)
{
  /* Find the override property by its old RNA path. */
  GHash *liboverride_runtime = override_library_rna_path_mapping_ensure(liboverride);
  IDOverrideLibraryProperty *liboverride_property = static_cast<IDOverrideLibraryProperty *>(
      BLI_ghash_popkey(liboverride_runtime, rna_path, nullptr));

  if (liboverride_property == nullptr) {
    return false;
  }

  /* The key (rna_path) was already popped out of the runtime mapping above. */
  lib_override_library_property_delete(liboverride, liboverride_property, false);
  return true;
}

void BKE_lib_override_library_property_delete(IDOverrideLibrary *liboverride,
                                              IDOverrideLibraryProperty *liboverride_property)
{
  lib_override_library_property_delete(liboverride, liboverride_property, true);
}

static IDOverrideLibraryPropertyOperation *liboverride_opop_find_name_lib_iterative(
    ListBase *liboverride_operations,
    const char *subitem_main_name,
    const char *subitem_other_name,
    const std::optional<const ID *> &subitem_main_id,
    const std::optional<const ID *> &subitem_other_id,
    const size_t offesetof_opop_main_name,
    const size_t offesetof_opop_other_name,
    const size_t offesetof_opop_main_id,
    const size_t offesetof_opop_other_id)
{
  const bool do_ids(subitem_main_id);
  IDOverrideLibraryPropertyOperation *opop;
  for (opop = static_cast<IDOverrideLibraryPropertyOperation *>(BLI_findstring_ptr(
           liboverride_operations, subitem_main_name, int(offesetof_opop_main_name)));
       opop;
       opop = static_cast<IDOverrideLibraryPropertyOperation *>(BLI_listbase_findafter_string_ptr(
           reinterpret_cast<Link *>(opop), subitem_main_name, int(offesetof_opop_main_name))))
  {
    const char *opop_other_name = *reinterpret_cast<const char **>(reinterpret_cast<char *>(opop) +
                                                                   offesetof_opop_other_name);
    const bool opop_use_id = (opop->flag & LIBOVERRIDE_OP_FLAG_IDPOINTER_ITEM_USE_ID) != 0;

    if (do_ids && opop_use_id) {
      /* Skip if ID pointers are expected valid and they do not exactly match. */
      const ID *opop_main_id = *reinterpret_cast<const ID **>(reinterpret_cast<char *>(opop) +
                                                              offesetof_opop_main_id);
      if (*subitem_main_id != opop_main_id) {
        continue;
      }
      const ID *opop_other_id = *reinterpret_cast<const ID **>(reinterpret_cast<char *>(opop) +
                                                               offesetof_opop_other_id);
      if (*subitem_other_id != opop_other_id) {
        continue;
      }
    }

    /* Only check other name if ID handling is matching between given search parameters and
     * current liboverride operation (i.e. if both have valid ID pointers, or both have none). */
    if ((do_ids && opop_use_id) || (!do_ids && !opop_use_id)) {
      if (!subitem_other_name && !opop_other_name) {
        return opop;
      }
      if (subitem_other_name && opop_other_name && STREQ(subitem_other_name, opop_other_name)) {
        return opop;
      }
    }

    /* No exact match found, keep checking the rest of the list of operations. */
  }

  return nullptr;
}

IDOverrideLibraryPropertyOperation *BKE_lib_override_library_property_operation_find(
    IDOverrideLibraryProperty *liboverride_property,
    const char *subitem_refname,
    const char *subitem_locname,
    const std::optional<const ID *> &subitem_refid,
    const std::optional<const ID *> &subitem_locid,
    const int subitem_refindex,
    const int subitem_locindex,
    const bool strict,
    bool *r_strict)
{
  BLI_assert(!subitem_refid == !subitem_locid);

  IDOverrideLibraryPropertyOperation *opop;
  const int subitem_defindex = -1;

  if (r_strict) {
    *r_strict = true;
  }

  if (subitem_locname != nullptr) {
    opop = liboverride_opop_find_name_lib_iterative(
        &liboverride_property->operations,
        subitem_locname,
        subitem_refname,
        subitem_locid,
        subitem_refid,
        offsetof(IDOverrideLibraryPropertyOperation, subitem_local_name),
        offsetof(IDOverrideLibraryPropertyOperation, subitem_reference_name),
        offsetof(IDOverrideLibraryPropertyOperation, subitem_local_id),
        offsetof(IDOverrideLibraryPropertyOperation, subitem_reference_id));

    if (opop != nullptr) {
      return opop;
    }
  }

  if (subitem_refname != nullptr) {
    opop = liboverride_opop_find_name_lib_iterative(
        &liboverride_property->operations,
        subitem_refname,
        subitem_locname,
        subitem_refid,
        subitem_locid,
        offsetof(IDOverrideLibraryPropertyOperation, subitem_reference_name),
        offsetof(IDOverrideLibraryPropertyOperation, subitem_local_name),
        offsetof(IDOverrideLibraryPropertyOperation, subitem_reference_id),
        offsetof(IDOverrideLibraryPropertyOperation, subitem_local_id));

    if (opop != nullptr) {
      return opop;
    }
  }

  opop = static_cast<IDOverrideLibraryPropertyOperation *>(
      BLI_listbase_bytes_find(&liboverride_property->operations,
                              &subitem_locindex,
                              sizeof(subitem_locindex),
                              offsetof(IDOverrideLibraryPropertyOperation, subitem_local_index)));
  if (opop) {
    return ELEM(subitem_refindex, -1, opop->subitem_reference_index) ? opop : nullptr;
  }

  opop = static_cast<IDOverrideLibraryPropertyOperation *>(BLI_listbase_bytes_find(
      &liboverride_property->operations,
      &subitem_refindex,
      sizeof(subitem_refindex),
      offsetof(IDOverrideLibraryPropertyOperation, subitem_reference_index)));
  if (opop) {
    return ELEM(subitem_locindex, -1, opop->subitem_local_index) ? opop : nullptr;
  }

  /* `index == -1` means all indices, that is a valid fallback in case we requested specific index.
   */
  if (!strict && (subitem_locindex != subitem_defindex)) {
    opop = static_cast<IDOverrideLibraryPropertyOperation *>(BLI_listbase_bytes_find(
        &liboverride_property->operations,
        &subitem_defindex,
        sizeof(subitem_defindex),
        offsetof(IDOverrideLibraryPropertyOperation, subitem_local_index)));
    if (opop) {
      if (r_strict) {
        *r_strict = false;
      }
      return opop;
    }
  }

  return nullptr;
}

IDOverrideLibraryPropertyOperation *BKE_lib_override_library_property_operation_get(
    IDOverrideLibraryProperty *liboverride_property,
    const short operation,
    const char *subitem_refname,
    const char *subitem_locname,
    const std::optional<ID *> &subitem_refid,
    const std::optional<ID *> &subitem_locid,
    const int subitem_refindex,
    const int subitem_locindex,
    const bool strict,
    bool *r_strict,
    bool *r_created)
{
  BLI_assert(!subitem_refid == !subitem_locid);

  IDOverrideLibraryPropertyOperation *opop = BKE_lib_override_library_property_operation_find(
      liboverride_property,
      subitem_refname,
      subitem_locname,
      subitem_refid,
      subitem_locid,
      subitem_refindex,
      subitem_locindex,
      strict,
      r_strict);

  if (opop == nullptr) {
    opop = MEM_callocN<IDOverrideLibraryPropertyOperation>(__func__);
    opop->operation = operation;
    if (subitem_locname) {
      opop->subitem_local_name = BLI_strdup(subitem_locname);
    }
    if (subitem_refname) {
      opop->subitem_reference_name = BLI_strdup(subitem_refname);
    }
    opop->subitem_local_index = subitem_locindex;
    opop->subitem_reference_index = subitem_refindex;

    if (subitem_refid) {
      opop->subitem_reference_id = *subitem_refid;
      opop->subitem_local_id = *subitem_locid;
      opop->flag |= LIBOVERRIDE_OP_FLAG_IDPOINTER_ITEM_USE_ID;
    }

    BLI_addtail(&liboverride_property->operations, opop);

    if (r_created) {
      *r_created = true;
    }
  }
  else if (r_created) {
    *r_created = false;
  }

  return opop;
}

void lib_override_library_property_operation_copy(IDOverrideLibraryPropertyOperation *opop_dst,
                                                  IDOverrideLibraryPropertyOperation *opop_src)
{
  if (opop_src->subitem_reference_name) {
    opop_dst->subitem_reference_name = BLI_strdup(opop_src->subitem_reference_name);
  }
  if (opop_src->subitem_local_name) {
    opop_dst->subitem_local_name = BLI_strdup(opop_src->subitem_local_name);
  }
}

void lib_override_library_property_operation_clear(IDOverrideLibraryPropertyOperation *opop)
{
  if (opop->subitem_reference_name) {
    MEM_freeN(opop->subitem_reference_name);
  }
  if (opop->subitem_local_name) {
    MEM_freeN(opop->subitem_local_name);
  }
}

void BKE_lib_override_library_property_operation_delete(
    IDOverrideLibraryProperty *liboverride_property,
    IDOverrideLibraryPropertyOperation *liboverride_property_operation)
{
  lib_override_library_property_operation_clear(liboverride_property_operation);
  BLI_freelinkN(&liboverride_property->operations, liboverride_property_operation);
}

bool BKE_lib_override_library_property_operation_operands_validate(
    IDOverrideLibraryPropertyOperation *liboverride_property_operation,
    PointerRNA *ptr_dst,
    PointerRNA *ptr_src,
    PointerRNA *ptr_storage,
    PropertyRNA *prop_dst,
    PropertyRNA *prop_src,
    PropertyRNA *prop_storage)
{
  switch (liboverride_property_operation->operation) {
    case LIBOVERRIDE_OP_NOOP:
      return true;
    case LIBOVERRIDE_OP_ADD:
      ATTR_FALLTHROUGH;
    case LIBOVERRIDE_OP_SUBTRACT:
      ATTR_FALLTHROUGH;
    case LIBOVERRIDE_OP_MULTIPLY:
      if (ptr_storage == nullptr || ptr_storage->data == nullptr || prop_storage == nullptr) {
        BLI_assert_msg(0, "Missing data to apply differential override operation.");
        return false;
      }
      ATTR_FALLTHROUGH;
    case LIBOVERRIDE_OP_INSERT_AFTER:
      ATTR_FALLTHROUGH;
    case LIBOVERRIDE_OP_INSERT_BEFORE:
      ATTR_FALLTHROUGH;
    case LIBOVERRIDE_OP_REPLACE:
      if ((ptr_dst == nullptr || ptr_dst->data == nullptr || prop_dst == nullptr) ||
          (ptr_src == nullptr || ptr_src->data == nullptr || prop_src == nullptr))
      {
        BLI_assert_msg(0, "Missing data to apply override operation.");
        return false;
      }
  }

  return true;
}

static bool override_library_is_valid(const ID &id,
                                      const IDOverrideLibrary &liboverride,
                                      ReportList *reports)
{
  if (liboverride.reference == nullptr) {
    /* This (probably) used to be a template ID, could be linked or local, not an override. */
    BKE_reportf(reports,
                RPT_WARNING,
                "Library override templates have been removed: removing all override data from "
                "the data-block '%s'",
                id.name);
    return false;
  }
  if (liboverride.reference == &id) {
    /* Very serious data corruption, cannot do much about it besides removing the liboverride data.
     */
    BKE_reportf(reports,
                RPT_ERROR,
                "Data corruption: data-block '%s' is using itself as library override reference, "
                "removing all override data",
                id.name);
    return false;
  }
  if (!ID_IS_LINKED(liboverride.reference)) {
    /* Very serious data corruption, cannot do much about it besides removing the liboverride data.
     */
    BKE_reportf(reports,
                RPT_ERROR,
                "Data corruption: data-block '%s' is using another local data-block ('%s') as "
                "library override reference, removing all override data",
                id.name,
                liboverride.reference->name);
    return false;
  }
  return true;
}

/** Check all override properties and rules to ensure they are valid. Remove invalid ones. */
static void override_library_properties_validate(const ID &id,
                                                 IDOverrideLibrary &liboverride,
                                                 ReportList *reports)
{
  LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryProperty *, op, &liboverride.properties) {
    if (!op->rna_path) {
      BKE_reportf(
          reports,
          RPT_ERROR,
          "Data corruption: data-block `%s` has a Library Override property with no RNA path",
          id.name);
      /* Simpler to allocate a dummy string here, than fix all 'normal' clearing/deletion code that
       * does expect a non-null RNA path. */
      op->rna_path = BLI_strdup("");
      lib_override_library_property_delete(&liboverride, op, true);
    }
  }
}

void BKE_lib_override_library_validate(Main *bmain, ID *id, ReportList *reports)
{
  /* Do NOT use `ID_IS_OVERRIDE_LIBRARY` here, since this code also needs to fix broken cases (like
   * null reference pointer), which would be skipped by that macro. */
  if (id->override_library == nullptr && !ID_IS_OVERRIDE_LIBRARY_VIRTUAL(id)) {
    return;
  }

  ID *liboverride_id = id;
  IDOverrideLibrary *liboverride = id->override_library;
  if (ID_IS_OVERRIDE_LIBRARY_VIRTUAL(id)) {
    liboverride = BKE_lib_override_library_get(bmain, id, nullptr, &liboverride_id);
    if (!liboverride || !override_library_is_valid(*liboverride_id, *liboverride, reports)) {
      /* Happens in case the given ID is a liboverride-embedded one (actual embedded ID like
       * NodeTree or master collection, or shape-keys), used by a totally not-liboverride owner ID.
       * Just clear the relevant ID flag.
       */
      id->flag &= ~ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE;
      return;
    }
  }
  BLI_assert(liboverride);

  /* NOTE: In code deleting liboverride data below, #BKE_lib_override_library_make_local is used
   * instead of directly calling #BKE_lib_override_library_free, because the former also handles
   * properly 'liboverride embedded' IDs, like root node-trees, or shape-keys. */
  if (!override_library_is_valid(*liboverride_id, *liboverride, reports)) {
    BKE_lib_override_library_make_local(nullptr, liboverride_id);
    return;
  }

  override_library_properties_validate(*liboverride_id, *liboverride, reports);
}

void BKE_lib_override_library_main_validate(Main *bmain, ReportList *reports)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    BKE_lib_override_library_validate(bmain, id, reports);
  }
  FOREACH_MAIN_ID_END;
}

bool BKE_lib_override_library_status_check_local(Main *bmain, ID *local)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(local));

  ID *reference = local->override_library->reference;

  BLI_assert(reference);
  BLI_assert(GS(local->name) == GS(reference->name));

  if (GS(local->name) == ID_OB) {
    /* Our beloved pose's bone cross-data pointers. Usually, depsgraph evaluation would
     * ensure this is valid, but in some situations (like hidden collections etc.) this won't
     * be the case, so we need to take care of this ourselves. */
    Object *ob_local = reinterpret_cast<Object *>(local);
    if (ob_local->type == OB_ARMATURE) {
      Object *ob_reference = reinterpret_cast<Object *>(local->override_library->reference);
      BLI_assert(ob_local->data != nullptr);
      BLI_assert(ob_reference->data != nullptr);
      BKE_pose_ensure(bmain, ob_local, static_cast<bArmature *>(ob_local->data), true);
      BKE_pose_ensure(bmain, ob_reference, static_cast<bArmature *>(ob_reference->data), true);
    }
  }

  /* Note that reference is assumed always valid, caller has to ensure that itself. */

  PointerRNA rnaptr_local = RNA_id_pointer_create(local);
  PointerRNA rnaptr_reference = RNA_id_pointer_create(reference);

  if (!RNA_struct_override_matches(
          bmain,
          &rnaptr_local,
          &rnaptr_reference,
          nullptr,
          0,
          local->override_library,
          (RNA_OVERRIDE_COMPARE_IGNORE_NON_OVERRIDABLE | RNA_OVERRIDE_COMPARE_IGNORE_OVERRIDDEN),
          nullptr))
  {
    local->tag &= ~ID_TAG_LIBOVERRIDE_REFOK;
    return false;
  }

  return true;
}

bool BKE_lib_override_library_status_check_reference(Main *bmain, ID *local)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(local));

  ID *reference = local->override_library->reference;

  BLI_assert(reference);
  BLI_assert(GS(local->name) == GS(reference->name));

  if (reference->override_library && (reference->tag & ID_TAG_LIBOVERRIDE_REFOK) == 0) {
    if (!BKE_lib_override_library_status_check_reference(bmain, reference)) {
      /* If reference is also an override of another data-block, and its status is not OK,
       * then this override is not OK either.
       * Note that this should only happen when reloading libraries. */
      local->tag &= ~ID_TAG_LIBOVERRIDE_REFOK;
      return false;
    }
  }

  if (GS(local->name) == ID_OB) {
    /* Our beloved pose's bone cross-data pointers. Usually, depsgraph evaluation would
     * ensure this is valid, but in some situations (like hidden collections etc.) this won't
     * be the case, so we need to take care of this ourselves. */
    Object *ob_local = reinterpret_cast<Object *>(local);
    if (ob_local->type == OB_ARMATURE) {
      Object *ob_reference = reinterpret_cast<Object *>(local->override_library->reference);
      BLI_assert(ob_local->data != nullptr);
      BLI_assert(ob_reference->data != nullptr);
      BKE_pose_ensure(bmain, ob_local, static_cast<bArmature *>(ob_local->data), true);
      BKE_pose_ensure(bmain, ob_reference, static_cast<bArmature *>(ob_reference->data), true);
    }
  }

  PointerRNA rnaptr_local = RNA_id_pointer_create(local);
  PointerRNA rnaptr_reference = RNA_id_pointer_create(reference);

  if (!RNA_struct_override_matches(bmain,
                                   &rnaptr_local,
                                   &rnaptr_reference,
                                   nullptr,
                                   0,
                                   local->override_library,
                                   RNA_OVERRIDE_COMPARE_IGNORE_OVERRIDDEN,
                                   nullptr))
  {
    local->tag &= ~ID_TAG_LIBOVERRIDE_REFOK;
    return false;
  }

  return true;
}

static void lib_override_library_operations_create(Main *bmain,
                                                   ID *local,
                                                   const eRNAOverrideMatch liboverride_match_flags,
                                                   eRNAOverrideMatchResult *r_report_flags)
{
  BLI_assert(!ID_IS_LINKED(local));
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(local));

  /* Do not attempt to generate overriding rules from an empty place-holder generated by link
   * code when it cannot find the actual library/ID. Much better to keep the local data-block as
   * is in the file in that case, until broken lib is fixed. */
  if (ID_MISSING(local->override_library->reference)) {
    return;
  }

  if (GS(local->name) == ID_OB) {
    /* Our beloved pose's bone cross-data pointers. Usually, depsgraph evaluation would
     * ensure this is valid, but in some situations (like hidden collections etc.) this won't
     * be the case, so we need to take care of this ourselves. */
    Object *ob_local = reinterpret_cast<Object *>(local);
    if (ob_local->type == OB_ARMATURE) {
      Object *ob_reference = reinterpret_cast<Object *>(local->override_library->reference);
      BLI_assert(ob_local->data != nullptr);
      BLI_assert(ob_reference->data != nullptr);
      BKE_pose_ensure(bmain, ob_local, static_cast<bArmature *>(ob_local->data), true);
      BKE_pose_ensure(bmain, ob_reference, static_cast<bArmature *>(ob_reference->data), true);
    }
  }

  PointerRNA rnaptr_local = RNA_id_pointer_create(local);
  PointerRNA rnaptr_reference = RNA_id_pointer_create(local->override_library->reference);

  eRNAOverrideMatchResult local_report_flags = RNA_OVERRIDE_MATCH_RESULT_INIT;
  RNA_struct_override_matches(bmain,
                              &rnaptr_local,
                              &rnaptr_reference,
                              nullptr,
                              0,
                              local->override_library,
                              liboverride_match_flags,
                              &local_report_flags);

  if (local_report_flags & RNA_OVERRIDE_MATCH_RESULT_RESTORED) {
    CLOG_DEBUG(&LOG, "We did restore some properties of %s from its reference", local->name);
  }
  if (local_report_flags & RNA_OVERRIDE_MATCH_RESULT_RESTORE_TAGGED) {
    CLOG_DEBUG(
        &LOG, "We did tag some properties of %s for restoration from its reference", local->name);
  }
  if (local_report_flags & RNA_OVERRIDE_MATCH_RESULT_CREATED) {
    CLOG_DEBUG(&LOG, "We did generate library override rules for %s", local->name);
  }
  else {
    CLOG_DEBUG(&LOG, "No new library override rules for %s", local->name);
  }

  if (r_report_flags != nullptr) {
    *r_report_flags = (*r_report_flags | local_report_flags);
  }
}
void BKE_lib_override_library_operations_create(Main *bmain, ID *local, int *r_report_flags)
{
  lib_override_library_operations_create(
      bmain,
      local,
      (RNA_OVERRIDE_COMPARE_CREATE | RNA_OVERRIDE_COMPARE_RESTORE),
      reinterpret_cast<eRNAOverrideMatchResult *>(r_report_flags));
}

void BKE_lib_override_library_operations_restore(Main *bmain, ID *local, int *r_report_flags)
{
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(local) ||
      (local->override_library->runtime->tag & LIBOVERRIDE_TAG_NEEDS_RESTORE) == 0)
  {
    return;
  }

  PointerRNA rnaptr_src = RNA_id_pointer_create(local);
  PointerRNA rnaptr_dst = RNA_id_pointer_create(local->override_library->reference);
  RNA_struct_override_apply(
      bmain,
      &rnaptr_dst,
      &rnaptr_src,
      nullptr,
      local->override_library,
      static_cast<eRNAOverrideApplyFlag>(RNA_OVERRIDE_APPLY_FLAG_SKIP_RESYNC_CHECK |
                                         RNA_OVERRIDE_APPLY_FLAG_RESTORE_ONLY));

  LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryProperty *, op, &local->override_library->properties)
  {
    if (op->tag & LIBOVERRIDE_PROP_TAG_NEEDS_RETORE) {
      LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
        if (opop->tag & LIBOVERRIDE_PROP_TAG_NEEDS_RETORE) {
          BKE_lib_override_library_property_operation_delete(op, opop);
        }
      }
      if (BLI_listbase_is_empty(&local->override_library->properties)) {
        BKE_lib_override_library_property_delete(local->override_library, op);
      }
      else {
        BKE_lib_override_library_operations_tag(op, LIBOVERRIDE_PROP_TAG_NEEDS_RETORE, false);
      }
    }
  }
  local->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_NEEDS_RESTORE;

  if (r_report_flags != nullptr) {
    *r_report_flags |= RNA_OVERRIDE_MATCH_RESULT_RESTORED;
  }
}

struct LibOverrideOpCreateData {
  Main *bmain;
  eRNAOverrideMatchResult report_flags;
};

static void lib_override_library_operations_create_cb(TaskPool *__restrict pool, void *taskdata)
{
  LibOverrideOpCreateData *create_data = static_cast<LibOverrideOpCreateData *>(
      BLI_task_pool_user_data(pool));
  ID *id = static_cast<ID *>(taskdata);

  eRNAOverrideMatchResult report_flags = RNA_OVERRIDE_MATCH_RESULT_INIT;
  lib_override_library_operations_create(
      create_data->bmain,
      id,
      (RNA_OVERRIDE_COMPARE_CREATE | RNA_OVERRIDE_COMPARE_TAG_FOR_RESTORE),
      &report_flags);
  atomic_fetch_and_or_uint32(reinterpret_cast<uint32_t *>(&create_data->report_flags),
                             report_flags);
}

void BKE_lib_override_library_main_operations_create(Main *bmain,
                                                     const bool force_auto,
                                                     int *r_report_flags)
{
  ID *id;

#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_START_AVERAGED(BKE_lib_override_library_main_operations_create);
#endif

  /* When force-auto is set, we also remove all unused existing override properties & operations.
   */
  if (force_auto) {
    BKE_lib_override_library_main_tag(bmain, LIBOVERRIDE_PROP_OP_TAG_UNUSED, true);
  }

  /* Usual pose bones issue, need to be done outside of the threaded process or we may run into
   * concurrency issues here.
   * Note that calling #BKE_pose_ensure again in thread in
   * #BKE_lib_override_library_operations_create is not a problem then. */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ob->type == OB_ARMATURE) {
      BLI_assert(ob->data != nullptr);
      BKE_pose_ensure(bmain, ob, static_cast<bArmature *>(ob->data), true);
    }
  }
  /* Similar issue with view layers, some may not be up-to-date, and re-syncing them from a
   * multi-threaded process is utterly unsafe. Some RNA property access may cause this, see e.g.
   * #147565 and the `node_warnings` property of the Geometry Nodes. */
  const bool resync_success = BKE_main_view_layers_synced_ensure(bmain);
  /* Layer resync should never fail here.
   *
   * This call is fairly high-level and should never happen within a callpath which has already
   * forbidden resync (using #BKE_layer_collection_resync_forbid).
   *
   * Other unlikely reasons for failure (like very old blendfile data before versioning, where
   * scenes have no master collection yet) are also never expected to be met in this code.
   */
  BLI_assert_msg(resync_success,
                 "Ensuring that all view-layers in Main are synced with their collections failed");
  UNUSED_VARS_NDEBUG(resync_success);
  BKE_layer_collection_resync_forbid();

  LibOverrideOpCreateData create_pool_data{};
  create_pool_data.bmain = bmain;
  create_pool_data.report_flags = RNA_OVERRIDE_MATCH_RESULT_INIT;
  TaskPool *task_pool = BLI_task_pool_create(&create_pool_data, TASK_PRIORITY_HIGH);

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (ID_IS_LINKED(id) || !ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      continue;
    }
    /* Propagate potential embedded data tag to the owner ID (see also
     * #BKE_lib_override_id_tag_on_deg_tag_from_user). */
    if (Key *key = BKE_key_from_id(id)) {
      if (key->id.tag & ID_TAG_LIBOVERRIDE_AUTOREFRESH) {
        key->id.tag &= ~ID_TAG_LIBOVERRIDE_AUTOREFRESH;
        id->tag |= ID_TAG_LIBOVERRIDE_AUTOREFRESH;
      }
    }
    if (bNodeTree *ntree = blender::bke::node_tree_from_id(id)) {
      if (ntree->id.tag & ID_TAG_LIBOVERRIDE_AUTOREFRESH) {
        ntree->id.tag &= ~ID_TAG_LIBOVERRIDE_AUTOREFRESH;
        id->tag |= ID_TAG_LIBOVERRIDE_AUTOREFRESH;
      }
    }
    if (GS(id->name) == ID_SCE) {
      if (Collection *scene_collection = reinterpret_cast<Scene *>(id)->master_collection) {
        if (scene_collection->id.tag & ID_TAG_LIBOVERRIDE_AUTOREFRESH) {
          scene_collection->id.tag &= ~ID_TAG_LIBOVERRIDE_AUTOREFRESH;
          id->tag |= ID_TAG_LIBOVERRIDE_AUTOREFRESH;
        }
      }
    }

    if (force_auto || (id->tag & ID_TAG_LIBOVERRIDE_AUTOREFRESH)) {
      /* Usual issue with pose, it's quiet rare but sometimes they may not be up to date when this
       * function is called. */
      if (GS(id->name) == ID_OB) {
        Object *ob = reinterpret_cast<Object *>(id);
        if (ob->type == OB_ARMATURE) {
          BLI_assert(ob->data != nullptr);
          BKE_pose_ensure(bmain, ob, static_cast<bArmature *>(ob->data), true);
        }
      }
      /* Only check overrides if we do have the real reference data available, and not some empty
       * 'placeholder' for missing data (broken links). */
      if ((id->override_library->reference->tag & ID_TAG_MISSING) == 0) {
        BLI_task_pool_push(
            task_pool, lib_override_library_operations_create_cb, id, false, nullptr);
      }
      else {
        BKE_lib_override_library_properties_tag(
            id->override_library, LIBOVERRIDE_PROP_OP_TAG_UNUSED, false);
      }
    }
    else {
      /* Clear 'unused' tag for un-processed IDs, otherwise e.g. linked overrides will loose their
       * list of overridden properties. */
      BKE_lib_override_library_properties_tag(
          id->override_library, LIBOVERRIDE_PROP_OP_TAG_UNUSED, false);
    }
    id->tag &= ~ID_TAG_LIBOVERRIDE_AUTOREFRESH;
  }
  FOREACH_MAIN_ID_END;

  BLI_task_pool_work_and_wait(task_pool);

  BLI_task_pool_free(task_pool);

  BKE_layer_collection_resync_allow();

  if (create_pool_data.report_flags & RNA_OVERRIDE_MATCH_RESULT_RESTORE_TAGGED) {
    BKE_lib_override_library_main_operations_restore(
        bmain, reinterpret_cast<int *>(&create_pool_data.report_flags));
    create_pool_data.report_flags = (create_pool_data.report_flags &
                                     ~RNA_OVERRIDE_MATCH_RESULT_RESTORE_TAGGED);
  }

  if (r_report_flags != nullptr) {
    *r_report_flags |= create_pool_data.report_flags;
  }

  if (force_auto) {
    BKE_lib_override_library_main_unused_cleanup(bmain);
  }

#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_END_AVERAGED(BKE_lib_override_library_main_operations_create);
#endif
}

void BKE_lib_override_library_main_operations_restore(Main *bmain, int *r_report_flags)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (!(!ID_IS_LINKED(id) && ID_IS_OVERRIDE_LIBRARY_REAL(id) && id->override_library->runtime &&
          (id->override_library->runtime->tag & LIBOVERRIDE_TAG_NEEDS_RESTORE) != 0))
    {
      continue;
    }

    /* Only restore overrides if we do have the real reference data available, and not some empty
     * 'placeholder' for missing data (broken links). */
    if (id->override_library->reference->tag & ID_TAG_MISSING) {
      continue;
    }

    BKE_lib_override_library_operations_restore(bmain, id, r_report_flags);
  }
  FOREACH_MAIN_ID_END;
}

static bool lib_override_library_id_reset_do(Main *bmain,
                                             ID *id_root,
                                             const bool do_reset_system_override)
{
  bool was_op_deleted = false;

  if (do_reset_system_override) {
    id_root->override_library->flag |= LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
  }

  LISTBASE_FOREACH_MUTABLE (
      IDOverrideLibraryProperty *, op, &id_root->override_library->properties)
  {
    bool do_op_delete = true;
    const bool is_collection = op->rna_prop_type == PROP_COLLECTION;
    if (is_collection || op->rna_prop_type == PROP_POINTER) {
      PointerRNA ptr, ptr_lib;
      PropertyRNA *prop, *prop_lib;

      PointerRNA ptr_root = RNA_pointer_create_discrete(id_root, &RNA_ID, id_root);
      PointerRNA ptr_root_lib = RNA_pointer_create_discrete(
          id_root->override_library->reference, &RNA_ID, id_root->override_library->reference);

      bool prop_exists = RNA_path_resolve_property(&ptr_root, op->rna_path, &ptr, &prop);
      if (prop_exists) {
        prop_exists = RNA_path_resolve_property(&ptr_root_lib, op->rna_path, &ptr_lib, &prop_lib);

        if (prop_exists) {
          BLI_assert(ELEM(RNA_property_type(prop), PROP_POINTER, PROP_COLLECTION));
          BLI_assert(RNA_property_type(prop) == RNA_property_type(prop_lib));
          if (is_collection) {
            ptr.type = RNA_property_pointer_type(&ptr, prop);
            ptr_lib.type = RNA_property_pointer_type(&ptr_lib, prop_lib);
          }
          else {
            ptr = RNA_property_pointer_get(&ptr, prop);
            ptr_lib = RNA_property_pointer_get(&ptr_lib, prop_lib);
          }
          if (ptr.owner_id != nullptr && ptr_lib.owner_id != nullptr) {
            BLI_assert(ptr.type == ptr_lib.type);
            do_op_delete = !(RNA_struct_is_ID(ptr.type) &&
                             ptr.owner_id->override_library != nullptr &&
                             ptr.owner_id->override_library->reference == ptr_lib.owner_id);
          }
        }
      }
    }

    if (do_op_delete) {
      BKE_lib_override_library_property_delete(id_root->override_library, op);
      was_op_deleted = true;
    }
  }

  if (was_op_deleted) {
    DEG_id_tag_update_ex(bmain, id_root, ID_RECALC_SYNC_TO_EVAL);
    IDOverrideLibraryRuntime *liboverride_runtime = override_library_runtime_ensure(
        id_root->override_library);
    liboverride_runtime->tag |= LIBOVERRIDE_TAG_NEEDS_RELOAD;
  }

  return was_op_deleted;
}

void BKE_lib_override_library_id_reset(Main *bmain,
                                       ID *id_root,
                                       const bool do_reset_system_override)
{
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_root)) {
    return;
  }

  if (lib_override_library_id_reset_do(bmain, id_root, do_reset_system_override)) {
    if (id_root->override_library->runtime != nullptr &&
        (id_root->override_library->runtime->tag & LIBOVERRIDE_TAG_NEEDS_RELOAD) != 0)
    {
      BKE_lib_override_library_update(bmain, id_root);
      id_root->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_NEEDS_RELOAD;
    }
  }
}

static void lib_override_library_id_hierarchy_recursive_reset(Main *bmain,
                                                              ID *id_root,
                                                              const bool do_reset_system_override)
{
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_root)) {
    return;
  }

  void **entry_vp = BLI_ghash_lookup_p(bmain->relations->relations_from_pointers, id_root);
  if (entry_vp == nullptr) {
    /* This ID is not used by nor using any other ID. */
    lib_override_library_id_reset_do(bmain, id_root, do_reset_system_override);
    return;
  }

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(*entry_vp);
  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    /* This ID has already been processed. */
    return;
  }

  lib_override_library_id_reset_do(bmain, id_root, do_reset_system_override);

  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != nullptr;
       to_id_entry = to_id_entry->next)
  {
    if (lib_override_hierarchy_dependencies_relationship_skip_check(to_id_entry)) {
      continue;
    }
    /* We only consider IDs from the same library. */
    if (*to_id_entry->id_pointer.to != nullptr) {
      ID *to_id = *to_id_entry->id_pointer.to;
      if (to_id->override_library != nullptr) {
        lib_override_library_id_hierarchy_recursive_reset(bmain, to_id, do_reset_system_override);
      }
    }
  }
}

void BKE_lib_override_library_id_hierarchy_reset(Main *bmain,
                                                 ID *id_root,
                                                 const bool do_reset_system_override)
{
  BKE_main_relations_create(bmain, 0);

  lib_override_library_id_hierarchy_recursive_reset(bmain, id_root, do_reset_system_override);

  BKE_main_relations_free(bmain);

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id) || id->override_library->runtime == nullptr ||
        (id->override_library->runtime->tag & LIBOVERRIDE_TAG_NEEDS_RELOAD) == 0)
    {
      continue;
    }
    BKE_lib_override_library_update(bmain, id);
    id->override_library->runtime->tag &= ~LIBOVERRIDE_TAG_NEEDS_RELOAD;
  }
  FOREACH_MAIN_ID_END;
}

void BKE_lib_override_library_operations_tag(IDOverrideLibraryProperty *liboverride_property,
                                             const short tag,
                                             const bool do_set)
{
  if (liboverride_property != nullptr) {
    if (do_set) {
      liboverride_property->tag |= tag;
    }
    else {
      liboverride_property->tag &= ~tag;
    }

    LISTBASE_FOREACH (
        IDOverrideLibraryPropertyOperation *, opop, &liboverride_property->operations)
    {
      if (do_set) {
        opop->tag |= tag;
      }
      else {
        opop->tag &= ~tag;
      }
    }
  }
}

void BKE_lib_override_library_properties_tag(IDOverrideLibrary *liboverride,
                                             const short tag,
                                             const bool do_set)
{
  if (liboverride != nullptr) {
    LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &liboverride->properties) {
      BKE_lib_override_library_operations_tag(op, tag, do_set);
    }
  }
}

void BKE_lib_override_library_main_tag(Main *bmain, const short tag, const bool do_set)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (ID_IS_OVERRIDE_LIBRARY(id)) {
      BKE_lib_override_library_properties_tag(id->override_library, tag, do_set);
    }
  }
  FOREACH_MAIN_ID_END;
}

void BKE_lib_override_library_id_unused_cleanup(ID *local)
{
  if (ID_IS_OVERRIDE_LIBRARY_REAL(local)) {
    LISTBASE_FOREACH_MUTABLE (
        IDOverrideLibraryProperty *, op, &local->override_library->properties)
    {
      if (op->tag & LIBOVERRIDE_PROP_OP_TAG_UNUSED) {
        BKE_lib_override_library_property_delete(local->override_library, op);
      }
      else {
        LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
          if (opop->tag & LIBOVERRIDE_PROP_OP_TAG_UNUSED) {
            BKE_lib_override_library_property_operation_delete(op, opop);
          }
        }
        if (BLI_listbase_is_empty(&op->operations)) {
          BKE_lib_override_library_property_delete(local->override_library, op);
        }
      }
    }
  }
}

void BKE_lib_override_library_main_unused_cleanup(Main *bmain)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (ID_IS_OVERRIDE_LIBRARY(id)) {
      BKE_lib_override_library_id_unused_cleanup(id);
    }
  }
  FOREACH_MAIN_ID_END;
}

static void lib_override_id_swap(Main *bmain, ID *id_local, ID *id_temp)
{
  /* Ensure ViewLayers are in sync in case a Scene is being swapped, and prevent any further resync
   * during the swapping itself. */
  if (GS(id_local->name) == ID_SCE) {
    BKE_scene_view_layers_synced_ensure(reinterpret_cast<Scene *>(id_local));
    BKE_scene_view_layers_synced_ensure(reinterpret_cast<Scene *>(id_temp));
  }
  BKE_layer_collection_resync_forbid();

  BKE_lib_id_swap(bmain, id_local, id_temp, true, 0);
  /* We need to keep these tags from temp ID into orig one.
   * ID swap does not swap most of ID data itself. */
  id_local->tag |= (id_temp->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC);

  BKE_layer_collection_resync_allow();
}

void BKE_lib_override_library_update(Main *bmain, ID *local)
{
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(local)) {
    return;
  }

  /* Do not attempt to apply overriding rules over an empty place-holder generated by link code
   * when it cannot find the actual library/ID. Much better to keep the local data-block as loaded
   * from the file in that case, until broken lib is fixed. */
  if (ID_MISSING(local->override_library->reference)) {
    return;
  }

  /* Recursively do 'ancestor' overrides first, if any. */
  if (local->override_library->reference->override_library &&
      (local->override_library->reference->tag & ID_TAG_LIBOVERRIDE_REFOK) == 0)
  {
    BKE_lib_override_library_update(bmain, local->override_library->reference);
  }

  /* We want to avoid having to remap here, however creating up-to-date override is much simpler
   * if based on reference than on current override.
   * So we work on temp copy of reference, and 'swap' its content with local. */

  /* XXX We need a way to get off-Main copies of IDs (similar to localized mats/texts/ etc.)!
   *     However, this is whole bunch of code work in itself, so for now plain stupid ID copy
   *     will do, as inefficient as it is. :/
   *     Actually, maybe not! Since we are swapping with original ID's local content, we want to
   *     keep user-count in correct state when freeing tmp_id
   *     (and that user-counts of IDs used by 'new' local data also remain correct). */
  /* This would imply change in handling of user-count all over RNA
   * (and possibly all over Blender code).
   * Not impossible to do, but would rather see first if extra useless usual user handling
   * is actually a (performances) issue here. */

  ID *tmp_id = BKE_id_copy_ex(bmain,
                              local->override_library->reference,
                              nullptr,
                              LIB_ID_COPY_DEFAULT | LIB_ID_COPY_NO_LIB_OVERRIDE_LOCAL_DATA_FLAG);

  if (tmp_id == nullptr) {
    return;
  }

  /* Remove the pair (idname, lib) of this temp id from the name map. */
  BKE_main_namemap_remove_id(*bmain, *tmp_id);

  tmp_id->lib = local->lib;

  /* This ID name is problematic, since it is an 'rna name property' it should not be editable or
   * different from reference linked ID. But local ID names need to be unique in a given type
   * list of Main, so we cannot always keep it identical, which is why we need this special
   * manual handling here. */
  STRNCPY(tmp_id->name, local->name);

  /* Those ugly loop-back pointers again. Luckily we only need to deal with the shape keys here,
   * collections' parents are fully runtime and reconstructed later. */
  Key *local_key = BKE_key_from_id(local);
  Key *tmp_key = BKE_key_from_id(tmp_id);
  if (local_key != nullptr && tmp_key != nullptr) {
    tmp_key->id.flag |= (local_key->id.flag & ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE);
    BKE_main_namemap_remove_id(*bmain, tmp_key->id);
    tmp_key->id.lib = local_key->id.lib;
    STRNCPY(tmp_key->id.name, local_key->id.name);
  }

  PointerRNA rnaptr_src = RNA_id_pointer_create(local);
  PointerRNA rnaptr_dst = RNA_id_pointer_create(tmp_id);

  RNA_struct_override_apply(bmain,
                            &rnaptr_dst,
                            &rnaptr_src,
                            nullptr,
                            local->override_library,
                            RNA_OVERRIDE_APPLY_FLAG_NOP);

  lib_override_object_posemode_transfer(tmp_id, local);

  /* This also transfers all pointers (memory) owned by local to tmp_id, and vice-versa.
   * So when we'll free tmp_id, we'll actually free old, outdated data from local. */
  lib_override_id_swap(bmain, local, tmp_id);

  if (local_key != nullptr && tmp_key != nullptr) {
    /* This is some kind of hard-coded 'always enforced override'. */
    lib_override_id_swap(bmain, &local_key->id, &tmp_key->id);
    tmp_key->id.flag |= (local_key->id.flag & ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE);
    /* The swap of local and tmp_id inverted those pointers, we need to redefine proper
     * relationships. */
    *BKE_key_from_id_p(local) = local_key;
    *BKE_key_from_id_p(tmp_id) = tmp_key;
    local_key->from = local;
    tmp_key->from = tmp_id;
  }

  /* Again, horribly inefficient in our case, we need something off-Main
   * (aka more generic nolib copy/free stuff).
   * NOTE: Do not remove this tmp_id's name from the namemap here, since this name actually still
   * exists in `bmain`. */
  BKE_id_free_ex(bmain, tmp_id, LIB_ID_FREE_NO_UI_USER | LIB_ID_FREE_NO_NAMEMAP_REMOVE, true);

  if (GS(local->name) == ID_AR) {
    /* Fun times again, thanks to bone pointers in pose data of objects. We keep same ID addresses,
     * but internal data has changed for sure, so we need to invalidate pose-bones caches. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->pose != nullptr && ob->data == local) {
        BLI_assert(ob->type == OB_ARMATURE);
        ob->pose->flag |= POSE_RECALC;
        /* We need to clear pose bone pointers immediately, some code may access those before pose
         * is actually recomputed, which can lead to segfault. */
        BKE_pose_clear_pointers(ob->pose);
      }
    }
  }

  /* NLA Tweak Mode is, in a way, an "edit mode" for certain animation data. However, contrary to
   * mesh/armature edit modes, it doesn't use its own runtime data, but directly changes various
   * DNA pointers & flags. As these need to be all consistently set for the system to behave in a
   * well-defined manner, and the values can come from different files (library NLA tracks/strips
   * vs. override-added NLA tracks/strips), they need to be checked _after_ all overrides have been
   * applied. */
  BKE_animdata_liboverride_post_process(local);

  local->tag |= ID_TAG_LIBOVERRIDE_REFOK;

  /* NOTE: Since we reload full content from linked ID here, potentially from edited local
   * override, we do not really have a way to know *what* is changed, so we need to rely on the
   * massive destruction weapon of `ID_RECALC_ALL` here. */
  DEG_id_tag_update_ex(bmain, local, ID_RECALC_ALL);
  /* For same reason as above, also assume that the relationships between IDs changed. */
  DEG_relations_tag_update(bmain);
}

void BKE_lib_override_library_main_update(Main *bmain)
{
  ID *id;

  /* This temporary swap of G_MAIN is rather ugly,
   * but necessary to avoid asserts checks in some RNA assignment functions,
   * since those always use G_MAIN when they need access to a Main database. */
  Main *orig_gmain = BKE_blender_globals_main_swap(bmain);

  BLI_assert(BKE_main_namemap_validate(*bmain));

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (id->override_library != nullptr) {
      BKE_lib_override_library_update(bmain, id);
    }
  }
  FOREACH_MAIN_ID_END;

  BLI_assert(BKE_main_namemap_validate(*bmain));

  Main *tmp_gmain = BKE_blender_globals_main_swap(orig_gmain);
  BLI_assert(tmp_gmain == bmain);
  UNUSED_VARS_NDEBUG(tmp_gmain);
}

bool BKE_lib_override_library_id_is_user_deletable(Main *bmain, ID *id)
{
  /* The only strong known case currently are objects used by override collections. */
  /* TODO: There are most likely other cases... This may need to be addressed in a better way at
   * some point. */
  if (GS(id->name) != ID_OB) {
    return true;
  }
  Object *ob = reinterpret_cast<Object *>(id);
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    if (!ID_IS_OVERRIDE_LIBRARY(collection)) {
      continue;
    }
    if (BKE_collection_has_object(collection, ob)) {
      return false;
    }
  }
  return true;
}

void BKE_lib_override_debug_print(IDOverrideLibrary *liboverride, const char *intro_txt)
{
  const char *line_prefix = "";
  if (intro_txt != nullptr) {
    std::cout << intro_txt << "\n";
    line_prefix = "\t";
  }

  LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &liboverride->properties) {
    std::cout << line_prefix << op->rna_path << " [";
    if (op->tag & LIBOVERRIDE_PROP_OP_TAG_UNUSED) {
      std::cout << " UNUSED ";
    }
    std::cout << "]\n";

    LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
      std::cout << line_prefix << line_prefix << opop->operation << " [";
      if (opop->tag & LIBOVERRIDE_PROP_OP_TAG_UNUSED) {
        std::cout << " UNUSED ";
      }
      if (opop->flag & LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE) {
        std::cout << " MATCH_REF ";
      }
      std::cout << "] ";
      if (opop->subitem_reference_name || opop->subitem_local_name) {
        std::cout << "(" << opop->subitem_reference_name << " <" << opop->subitem_reference_id
                  << "> -> " << opop->subitem_local_name << " <" << opop->subitem_local_id << ">)";
      }
      else if (opop->subitem_reference_index >= 0 || opop->subitem_local_index >= 0) {
        std::cout << "(" << opop->subitem_reference_index << " -> " << opop->subitem_local_index
                  << ")";
      }
      std::cout << "\n";
    }
  }
}
