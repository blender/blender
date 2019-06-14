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
 */

/** \file
 * \ingroup bke
 *
 * Contains management of ID's and libraries remap, unlink and free logic.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_light_types.h"
#include "DNA_lattice_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_speaker_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_vfont_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_brush.h"
#include "BKE_camera.h"
#include "BKE_cachefile.h"
#include "BKE_collection.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_font.h"
#include "BKE_gpencil.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_light.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_override.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_linestyle.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_movieclip.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_lightprobe.h"
#include "BKE_speaker.h"
#include "BKE_sound.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_text.h"
#include "BKE_texture.h"
#include "BKE_workspace.h"
#include "BKE_world.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

static CLG_LogRef LOG = {"bke.library_remap"};

static BKE_library_free_window_manager_cb free_windowmanager_cb = NULL;

void BKE_library_callback_free_window_manager_set(BKE_library_free_window_manager_cb func)
{
  free_windowmanager_cb = func;
}

static BKE_library_free_notifier_reference_cb free_notifier_reference_cb = NULL;

void BKE_library_callback_free_notifier_reference_set(BKE_library_free_notifier_reference_cb func)
{
  free_notifier_reference_cb = func;
}

static BKE_library_remap_editor_id_reference_cb remap_editor_id_reference_cb = NULL;

void BKE_library_callback_remap_editor_id_reference_set(
    BKE_library_remap_editor_id_reference_cb func)
{
  remap_editor_id_reference_cb = func;
}

typedef struct IDRemap {
  Main *bmain; /* Only used to trigger depsgraph updates in the right bmain. */
  ID *old_id;
  ID *new_id;
  /** The ID in which we are replacing old_id by new_id usages. */
  ID *id;
  short flag;

  /* 'Output' data. */
  short status;
  /** Number of direct usecases that could not be remapped (e.g.: obdata when in edit mode). */
  int skipped_direct;
  /** Number of indirect usecases that could not be remapped. */
  int skipped_indirect;
  /** Number of skipped usecases that refcount the datablock. */
  int skipped_refcounted;
} IDRemap;

/* IDRemap->flag enums defined in BKE_library.h */

/* IDRemap->status */
enum {
  /* *** Set by callback. *** */
  ID_REMAP_IS_LINKED_DIRECT = 1 << 0,    /* new_id is directly linked in current .blend. */
  ID_REMAP_IS_USER_ONE_SKIPPED = 1 << 1, /* There was some skipped 'user_one' usages of old_id. */
};

static int foreach_libblock_remap_callback(void *user_data, ID *id_self, ID **id_p, int cb_flag)
{
  if (cb_flag & IDWALK_CB_PRIVATE) {
    return IDWALK_RET_NOP;
  }

  IDRemap *id_remap_data = user_data;
  ID *old_id = id_remap_data->old_id;
  ID *new_id = id_remap_data->new_id;
  ID *id = id_remap_data->id;

  if (!old_id) { /* Used to cleanup all IDs used by a specific one. */
    BLI_assert(!new_id);
    old_id = *id_p;
  }

  if (*id_p && (*id_p == old_id)) {
    /* Better remap to NULL than not remapping at all,
     * then we can handle it as a regular remap-to-NULL case. */
    if ((cb_flag & IDWALK_CB_NEVER_SELF) && (new_id == id_self)) {
      new_id = NULL;
    }

    const bool is_reference = (cb_flag & IDWALK_CB_OVERRIDE_LIBRARY_REFERENCE) != 0;
    const bool is_indirect = (cb_flag & IDWALK_CB_INDIRECT_USAGE) != 0;
    const bool skip_indirect = (id_remap_data->flag & ID_REMAP_SKIP_INDIRECT_USAGE) != 0;
    /* Note: proxy usage implies LIB_TAG_EXTERN, so on this aspect it is direct,
     * on the other hand since they get reset to lib data on file open/reload it is indirect too.
     * Edit Mode is also a 'skip direct' case. */
    const bool is_obj = (GS(id->name) == ID_OB);
    const bool is_obj_proxy = (is_obj && (((Object *)id)->proxy || ((Object *)id)->proxy_group));
    const bool is_obj_editmode = (is_obj && BKE_object_is_in_editmode((Object *)id));
    const bool is_never_null = ((cb_flag & IDWALK_CB_NEVER_NULL) && (new_id == NULL) &&
                                (id_remap_data->flag & ID_REMAP_FORCE_NEVER_NULL_USAGE) == 0);
    const bool skip_reference = (id_remap_data->flag & ID_REMAP_SKIP_OVERRIDE_LIBRARY) != 0;
    const bool skip_never_null = (id_remap_data->flag & ID_REMAP_SKIP_NEVER_NULL_USAGE) != 0;

#ifdef DEBUG_PRINT
    printf(
        "In %s (lib %p): Remapping %s (%p) to %s (%p) "
        "(is_indirect: %d, skip_indirect: %d, is_reference: %d, skip_reference: %d)\n",
        id->name,
        id->lib,
        old_id->name,
        old_id,
        new_id ? new_id->name : "<NONE>",
        new_id,
        is_indirect,
        skip_indirect,
        is_reference,
        skip_reference);
#endif

    if ((id_remap_data->flag & ID_REMAP_FLAG_NEVER_NULL_USAGE) &&
        (cb_flag & IDWALK_CB_NEVER_NULL)) {
      id->tag |= LIB_TAG_DOIT;
    }

    /* Special hack in case it's Object->data and we are in edit mode, and new_id is not NULL
     * (otherwise, we follow common NEVER_NULL flags).
     * (skipped_indirect too). */
    if ((is_never_null && skip_never_null) ||
        (is_obj_editmode && (((Object *)id)->data == *id_p) && new_id != NULL) ||
        (skip_indirect && is_indirect) || (is_reference && skip_reference)) {
      if (is_indirect) {
        id_remap_data->skipped_indirect++;
        if (is_obj) {
          Object *ob = (Object *)id;
          if (ob->data == *id_p && ob->proxy != NULL) {
            /* And another 'Proudly brought to you by Proxy Hell' hack!
             * This will allow us to avoid clearing 'LIB_EXTERN' flag of obdata of proxies... */
            id_remap_data->skipped_direct++;
          }
        }
      }
      else if (is_never_null || is_obj_editmode || is_reference) {
        id_remap_data->skipped_direct++;
      }
      else {
        BLI_assert(0);
      }
      if (cb_flag & IDWALK_CB_USER) {
        id_remap_data->skipped_refcounted++;
      }
      else if (cb_flag & IDWALK_CB_USER_ONE) {
        /* No need to count number of times this happens, just a flag is enough. */
        id_remap_data->status |= ID_REMAP_IS_USER_ONE_SKIPPED;
      }
    }
    else {
      if (!is_never_null) {
        *id_p = new_id;
        DEG_id_tag_update_ex(id_remap_data->bmain,
                             id_self,
                             ID_RECALC_COPY_ON_WRITE | ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      }
      if (cb_flag & IDWALK_CB_USER) {
        /* NOTE: We don't user-count IDs which are not in the main database.
         * This is because in certain conditions we can have data-blocks in
         * the main which are referencing data-blocks outside of it.
         * For example, BKE_mesh_new_from_object() called on an evaluated
         * object will cause such situation.
         */
        if ((old_id->tag & LIB_TAG_NO_MAIN) == 0) {
          id_us_min(old_id);
        }
        if (new_id != NULL && (new_id->tag & LIB_TAG_NO_MAIN) == 0) {
          /* We do not want to handle LIB_TAG_INDIRECT/LIB_TAG_EXTERN here. */
          new_id->us++;
        }
      }
      else if (cb_flag & IDWALK_CB_USER_ONE) {
        id_us_ensure_real(new_id);
        /* We cannot affect old_id->us directly, LIB_TAG_EXTRAUSER(_SET)
         * are assumed to be set as needed, that extra user is processed in final handling. */
      }
      if (!is_indirect || is_obj_proxy) {
        id_remap_data->status |= ID_REMAP_IS_LINKED_DIRECT;
      }
      /* We need to remap proxy_from pointer of remapped proxy... sigh. */
      if (is_obj_proxy && new_id != NULL) {
        Object *ob = (Object *)id;
        if (ob->proxy == (Object *)new_id) {
          ob->proxy->proxy_from = ob;
        }
      }
    }
  }

  return IDWALK_RET_NOP;
}

static void libblock_remap_data_preprocess(IDRemap *r_id_remap_data)
{
  switch (GS(r_id_remap_data->id->name)) {
    case ID_OB: {
      ID *old_id = r_id_remap_data->old_id;
      if (!old_id || GS(old_id->name) == ID_AR) {
        Object *ob = (Object *)r_id_remap_data->id;
        /* Object's pose holds reference to armature bones... sic */
        /* Note that in theory, we should have to bother about
         * linked/non-linked/never-null/etc. flags/states.
         * Fortunately, this is just a tag, so we can accept to 'over-tag' a bit for pose recalc,
         * and avoid another complex and risky condition nightmare like the one we have in
         * foreach_libblock_remap_callback()... */
        if (ob->pose && (!old_id || ob->data == old_id)) {
          BLI_assert(ob->type == OB_ARMATURE);
          ob->pose->flag |= POSE_RECALC;
          /* We need to clear pose bone pointers immediately, things like undo writefile may be
           * called before pose is actually recomputed, can lead to segfault... */
          BKE_pose_clear_pointers(ob->pose);
        }
      }
      break;
    }
    default:
      break;
  }
}

/**
 * Can be called with both old_ob and new_ob being NULL,
 * this means we have to check whole Main database then.
 */
static void libblock_remap_data_postprocess_object_update(Main *bmain,
                                                          Object *old_ob,
                                                          Object *new_ob)
{
  if (new_ob == NULL) {
    /* In case we unlinked old_ob (new_ob is NULL), the object has already
     * been removed from the scenes and their collections. We still have
     * to remove the NULL children from collections not used in any scene. */
    BKE_collections_object_remove_nulls(bmain);
  }

  BKE_main_collection_sync_remap(bmain);

  if (old_ob == NULL) {
    for (Object *ob = bmain->objects.first; ob != NULL; ob = ob->id.next) {
      if (ob->type == OB_MBALL && BKE_mball_is_basis(ob)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
    }
  }
  else {
    for (Object *ob = bmain->objects.first; ob != NULL; ob = ob->id.next) {
      if (ob->type == OB_MBALL && BKE_mball_is_basis_for(ob, old_ob)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        break; /* There is only one basis... */
      }
    }
  }
}

/* Can be called with both old_collection and new_collection being NULL,
 * this means we have to check whole Main database then. */
static void libblock_remap_data_postprocess_collection_update(Main *bmain,
                                                              Collection *UNUSED(old_collection),
                                                              Collection *new_collection)
{
  if (new_collection == NULL) {
    /* XXX Complex cases can lead to NULL pointers in other collections than old_collection,
     * and BKE_main_collection_sync_remap() does not tolerate any of those, so for now always check
     * whole existing collections for NULL pointers.
     * I'd consider optimizing that whole collection remapping process a TODO for later. */
    BKE_collections_child_remove_nulls(bmain, NULL /*old_collection*/);
  }
  else {
    /* Temp safe fix, but a "tad" brute force... We should probably be able to use parents from
     * old_collection instead? */
    BKE_main_collections_parent_relations_rebuild(bmain);
  }

  BKE_main_collection_sync_remap(bmain);
}

static void libblock_remap_data_postprocess_obdata_relink(Main *bmain, Object *ob, ID *new_id)
{
  if (ob->data == new_id) {
    switch (GS(new_id->name)) {
      case ID_ME:
        multires_force_update(ob);
        break;
      case ID_CU:
        BKE_curve_type_test(ob);
        break;
      default:
        break;
    }
    test_object_modifiers(ob);
    test_object_materials(bmain, ob, new_id);
  }
}

static void libblock_remap_data_postprocess_nodetree_update(Main *bmain, ID *new_id)
{
  /* Update all group nodes using a node group. */
  ntreeUpdateAllUsers(bmain, new_id);
}

/**
 * Execute the 'data' part of the remapping (that is, all ID pointers from other ID data-blocks).
 *
 * Behavior differs depending on whether given \a id is NULL or not:
 * - \a id NULL: \a old_id must be non-NULL, \a new_id may be NULL (unlinking \a old_id) or not
 *   (remapping \a old_id to \a new_id).
 *   The whole \a bmain database is checked, and all pointers to \a old_id
 *   are remapped to \a new_id.
 * - \a id is non-NULL:
 *   + If \a old_id is NULL, \a new_id must also be NULL,
 *     and all ID pointers from \a id are cleared
 *     (i.e. \a id does not references any other data-block anymore).
 *   + If \a old_id is non-NULL, behavior is as with a NULL \a id, but only within given \a id.
 *
 * \param bmain: the Main data storage to operate on (must never be NULL).
 * \param id: the data-block to operate on
 * (can be NULL, in which case we operate over all IDs from given bmain).
 * \param old_id: the data-block to dereference (may be NULL if \a id is non-NULL).
 * \param new_id: the new data-block to replace \a old_id references with (may be NULL).
 * \param r_id_remap_data: if non-NULL, the IDRemap struct to use
 * (uselful to retrieve info about remapping process).
 */
ATTR_NONNULL(1)
static void libblock_remap_data(
    Main *bmain, ID *id, ID *old_id, ID *new_id, const short remap_flags, IDRemap *r_id_remap_data)
{
  IDRemap id_remap_data;
  const int foreach_id_flags = (remap_flags & ID_REMAP_NO_INDIRECT_PROXY_DATA_USAGE) != 0 ?
                                   IDWALK_NO_INDIRECT_PROXY_DATA_USAGE :
                                   IDWALK_NOP;

  if (r_id_remap_data == NULL) {
    r_id_remap_data = &id_remap_data;
  }
  r_id_remap_data->bmain = bmain;
  r_id_remap_data->old_id = old_id;
  r_id_remap_data->new_id = new_id;
  r_id_remap_data->id = NULL;
  r_id_remap_data->flag = remap_flags;
  r_id_remap_data->status = 0;
  r_id_remap_data->skipped_direct = 0;
  r_id_remap_data->skipped_indirect = 0;
  r_id_remap_data->skipped_refcounted = 0;

  if (id) {
#ifdef DEBUG_PRINT
    printf("\tchecking id %s (%p, %p)\n", id->name, id, id->lib);
#endif
    r_id_remap_data->id = id;
    libblock_remap_data_preprocess(r_id_remap_data);
    BKE_library_foreach_ID_link(
        NULL, id, foreach_libblock_remap_callback, (void *)r_id_remap_data, foreach_id_flags);
  }
  else {
    /* Note that this is a very 'brute force' approach,
     * maybe we could use some depsgraph to only process objects actually using given old_id...
     * sounds rather unlikely currently, though, so this will do for now. */
    ID *id_curr;

    FOREACH_MAIN_ID_BEGIN (bmain, id_curr) {
      if (BKE_library_id_can_use_idtype(id_curr, GS(old_id->name))) {
        /* Note that we cannot skip indirect usages of old_id here (if requested),
         * we still need to check it for the user count handling...
         * XXX No more true (except for debug usage of those skipping counters). */
        r_id_remap_data->id = id_curr;
        libblock_remap_data_preprocess(r_id_remap_data);
        BKE_library_foreach_ID_link(NULL,
                                    id_curr,
                                    foreach_libblock_remap_callback,
                                    (void *)r_id_remap_data,
                                    foreach_id_flags);
      }
    }
    FOREACH_MAIN_ID_END;
  }

  /* XXX We may not want to always 'transfer' fake-user from old to new id...
   *     Think for now it's desired behavior though,
   *     we can always add an option (flag) to control this later if needed. */
  if (old_id && (old_id->flag & LIB_FAKEUSER)) {
    id_fake_user_clear(old_id);
    id_fake_user_set(new_id);
  }

  id_us_clear_real(old_id);

  if (new_id && (new_id->tag & LIB_TAG_INDIRECT) &&
      (r_id_remap_data->status & ID_REMAP_IS_LINKED_DIRECT)) {
    new_id->tag &= ~LIB_TAG_INDIRECT;
    new_id->tag |= LIB_TAG_EXTERN;
  }

#ifdef DEBUG_PRINT
  printf("%s: %d occurrences skipped (%d direct and %d indirect ones)\n",
         __func__,
         r_id_remap_data->skipped_direct + r_id_remap_data->skipped_indirect,
         r_id_remap_data->skipped_direct,
         r_id_remap_data->skipped_indirect);
#endif
}

/**
 * Replace all references in given Main to \a old_id by \a new_id
 * (if \a new_id is NULL, it unlinks \a old_id).
 */
void BKE_libblock_remap_locked(Main *bmain, void *old_idv, void *new_idv, const short remap_flags)
{
  IDRemap id_remap_data;
  ID *old_id = old_idv;
  ID *new_id = new_idv;
  int skipped_direct, skipped_refcounted;

  BLI_assert(old_id != NULL);
  BLI_assert((new_id == NULL) || GS(old_id->name) == GS(new_id->name));
  BLI_assert(old_id != new_id);

  libblock_remap_data(bmain, NULL, old_id, new_id, remap_flags, &id_remap_data);

  if (free_notifier_reference_cb) {
    free_notifier_reference_cb(old_id);
  }

  /* We assume editors do not hold references to their IDs... This is false in some cases
   * (Image is especially tricky here),
   * editors' code is to handle refcount (id->us) itself then. */
  if (remap_editor_id_reference_cb) {
    remap_editor_id_reference_cb(old_id, new_id);
  }

  skipped_direct = id_remap_data.skipped_direct;
  skipped_refcounted = id_remap_data.skipped_refcounted;

  /* If old_id was used by some ugly 'user_one' stuff (like Image or Clip editors...), and user
   * count has actually been incremented for that, we have to decrease once more its user count...
   * unless we had to skip some 'user_one' cases. */
  if ((old_id->tag & LIB_TAG_EXTRAUSER_SET) &&
      !(id_remap_data.status & ID_REMAP_IS_USER_ONE_SKIPPED)) {
    id_us_clear_real(old_id);
  }

  if (old_id->us - skipped_refcounted < 0) {
    CLOG_ERROR(&LOG,
               "Error in remapping process from '%s' (%p) to '%s' (%p): "
               "wrong user count in old ID after process (summing up to %d)",
               old_id->name,
               old_id,
               new_id ? new_id->name : "<NULL>",
               new_id,
               old_id->us - skipped_refcounted);
    BLI_assert(0);
  }

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
      libblock_remap_data_postprocess_object_update(bmain, (Object *)old_id, (Object *)new_id);
      break;
    case ID_GR:
      libblock_remap_data_postprocess_collection_update(
          bmain, (Collection *)old_id, (Collection *)new_id);
      break;
    case ID_ME:
    case ID_CU:
    case ID_MB:
      if (new_id) { /* Only affects us in case obdata was relinked (changed). */
        for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
          libblock_remap_data_postprocess_obdata_relink(bmain, ob, new_id);
        }
      }
      break;
    default:
      break;
  }

  /* Node trees may virtually use any kind of data-block... */
  /* XXX Yuck!!!! nodetree update can do pretty much any thing when talking about py nodes,
   *     including creating new data-blocks (see T50385), so we need to unlock main here. :(
   *     Why can't we have re-entrent locks? */
  BKE_main_unlock(bmain);
  libblock_remap_data_postprocess_nodetree_update(bmain, new_id);
  BKE_main_lock(bmain);

  /* Full rebuild of DEG! */
  DEG_relations_tag_update(bmain);
}

void BKE_libblock_remap(Main *bmain, void *old_idv, void *new_idv, const short remap_flags)
{
  BKE_main_lock(bmain);

  BKE_libblock_remap_locked(bmain, old_idv, new_idv, remap_flags);

  BKE_main_unlock(bmain);
}

/**
 * Unlink given \a id from given \a bmain
 * (does not touch to indirect, i.e. library, usages of the ID).
 *
 * \param do_flag_never_null: If true, all IDs using \a idv in a 'non-NULL' way are flagged by
 * #LIB_TAG_DOIT flag (quite obviously, 'non-NULL' usages can never be unlinked by this function).
 */
void BKE_libblock_unlink(Main *bmain,
                         void *idv,
                         const bool do_flag_never_null,
                         const bool do_skip_indirect)
{
  const short remap_flags = (do_skip_indirect ? ID_REMAP_SKIP_INDIRECT_USAGE : 0) |
                            (do_flag_never_null ? ID_REMAP_FLAG_NEVER_NULL_USAGE : 0);

  BKE_main_lock(bmain);

  BKE_libblock_remap_locked(bmain, idv, NULL, remap_flags);

  BKE_main_unlock(bmain);
}

/** Similar to libblock_remap, but only affects IDs used by given \a idv ID.
 *
 * \param old_idv: Unlike BKE_libblock_remap, can be NULL,
 * in which case all ID usages by given \a idv will be cleared.
 * \param us_min_never_null: If \a true and new_id is NULL,
 * 'NEVER_NULL' ID usages keep their old id, but this one still gets its user count decremented
 * (needed when given \a idv is going to be deleted right after being unlinked).
 */
/* Should be able to replace all _relink() funcs (constraints, rigidbody, etc.) ? */
/* XXX Arg! Naming... :(
 *     _relink? avoids confusion with _remap, but is confusing with _unlink
 *     _remap_used_ids?
 *     _remap_datablocks?
 *     BKE_id_remap maybe?
 *     ... sigh
 */
void BKE_libblock_relink_ex(
    Main *bmain, void *idv, void *old_idv, void *new_idv, const bool us_min_never_null)
{
  ID *id = idv;
  ID *old_id = old_idv;
  ID *new_id = new_idv;
  int remap_flags = us_min_never_null ? 0 : ID_REMAP_SKIP_NEVER_NULL_USAGE;

  /* No need to lock here, we are only affecting given ID, not bmain database. */

  BLI_assert(id);
  if (old_id) {
    BLI_assert((new_id == NULL) || GS(old_id->name) == GS(new_id->name));
    BLI_assert(old_id != new_id);
  }
  else {
    BLI_assert(new_id == NULL);
  }

  libblock_remap_data(bmain, id, old_id, new_id, remap_flags, NULL);

  /* Some after-process updates.
   * This is a bit ugly, but cannot see a way to avoid it.
   * Maybe we should do a per-ID callback for this instead?
   */
  switch (GS(id->name)) {
    case ID_SCE: {
      if (old_id) {
        switch (GS(old_id->name)) {
          case ID_OB:
            libblock_remap_data_postprocess_object_update(
                bmain, (Object *)old_id, (Object *)new_id);
            break;
          case ID_GR:
            libblock_remap_data_postprocess_collection_update(
                bmain, (Collection *)old_id, (Collection *)new_id);
            break;
          default:
            break;
        }
      }
      else {
        /* No choice but to check whole objects/collections. */
        libblock_remap_data_postprocess_collection_update(bmain, NULL, NULL);
        libblock_remap_data_postprocess_object_update(bmain, NULL, NULL);
      }
      break;
    }
    case ID_OB:
      if (new_id) { /* Only affects us in case obdata was relinked (changed). */
        libblock_remap_data_postprocess_obdata_relink(bmain, (Object *)id, new_id);
      }
      break;
    default:
      break;
  }
}

static int id_relink_to_newid_looper(void *UNUSED(user_data),
                                     ID *UNUSED(self_id),
                                     ID **id_pointer,
                                     const int cb_flag)
{
  if (cb_flag & IDWALK_CB_PRIVATE) {
    return IDWALK_RET_NOP;
  }

  ID *id = *id_pointer;
  if (id) {
    /* See: NEW_ID macro */
    if (id->newid) {
      BKE_library_update_ID_link_user(id->newid, id, cb_flag);
      *id_pointer = id->newid;
    }
    else if (id->tag & LIB_TAG_NEW) {
      id->tag &= ~LIB_TAG_NEW;
      BKE_libblock_relink_to_newid(id);
    }
  }
  return IDWALK_RET_NOP;
}

/**
 * Similar to #libblock_relink_ex,
 * but is remapping IDs to their newid value if non-NULL, in given \a id.
 *
 * Very specific usage, not sure we'll keep it on the long run,
 * currently only used in Object/Collection duplication code...
 */
void BKE_libblock_relink_to_newid(ID *id)
{
  if (ID_IS_LINKED(id)) {
    return;
  }

  BKE_library_foreach_ID_link(NULL, id, id_relink_to_newid_looper, NULL, 0);
}

void BKE_libblock_free_data(ID *id, const bool do_id_user)
{
  if (id->properties) {
    IDP_FreePropertyContent_ex(id->properties, do_id_user);
    MEM_freeN(id->properties);
  }

  if (id->override_library) {
    BKE_override_library_free(&id->override_library);
  }

  /* XXX TODO remove animdata handling from each type's freeing func,
   * and do it here, like for copy! */
}

void BKE_libblock_free_datablock(ID *id, const int UNUSED(flag))
{
  const short type = GS(id->name);
  switch (type) {
    case ID_SCE:
      BKE_scene_free_ex((Scene *)id, false);
      break;
    case ID_LI:
      BKE_library_free((Library *)id);
      break;
    case ID_OB:
      BKE_object_free((Object *)id);
      break;
    case ID_ME:
      BKE_mesh_free((Mesh *)id);
      break;
    case ID_CU:
      BKE_curve_free((Curve *)id);
      break;
    case ID_MB:
      BKE_mball_free((MetaBall *)id);
      break;
    case ID_MA:
      BKE_material_free((Material *)id);
      break;
    case ID_TE:
      BKE_texture_free((Tex *)id);
      break;
    case ID_IM:
      BKE_image_free((Image *)id);
      break;
    case ID_LT:
      BKE_lattice_free((Lattice *)id);
      break;
    case ID_LA:
      BKE_light_free((Light *)id);
      break;
    case ID_CA:
      BKE_camera_free((Camera *)id);
      break;
    case ID_IP: /* Deprecated. */
      BKE_ipo_free((Ipo *)id);
      break;
    case ID_KE:
      BKE_key_free((Key *)id);
      break;
    case ID_WO:
      BKE_world_free((World *)id);
      break;
    case ID_SCR:
      BKE_screen_free((bScreen *)id);
      break;
    case ID_VF:
      BKE_vfont_free((VFont *)id);
      break;
    case ID_TXT:
      BKE_text_free((Text *)id);
      break;
    case ID_SPK:
      BKE_speaker_free((Speaker *)id);
      break;
    case ID_LP:
      BKE_lightprobe_free((LightProbe *)id);
      break;
    case ID_SO:
      BKE_sound_free((bSound *)id);
      break;
    case ID_GR:
      BKE_collection_free((Collection *)id);
      break;
    case ID_AR:
      BKE_armature_free((bArmature *)id);
      break;
    case ID_AC:
      BKE_action_free((bAction *)id);
      break;
    case ID_NT:
      ntreeFreeTree((bNodeTree *)id);
      break;
    case ID_BR:
      BKE_brush_free((Brush *)id);
      break;
    case ID_PA:
      BKE_particlesettings_free((ParticleSettings *)id);
      break;
    case ID_WM:
      if (free_windowmanager_cb) {
        free_windowmanager_cb(NULL, (wmWindowManager *)id);
      }
      break;
    case ID_GD:
      BKE_gpencil_free((bGPdata *)id, true);
      break;
    case ID_MC:
      BKE_movieclip_free((MovieClip *)id);
      break;
    case ID_MSK:
      BKE_mask_free((Mask *)id);
      break;
    case ID_LS:
      BKE_linestyle_free((FreestyleLineStyle *)id);
      break;
    case ID_PAL:
      BKE_palette_free((Palette *)id);
      break;
    case ID_PC:
      BKE_paint_curve_free((PaintCurve *)id);
      break;
    case ID_CF:
      BKE_cachefile_free((CacheFile *)id);
      break;
    case ID_WS:
      BKE_workspace_free((WorkSpace *)id);
      break;
  }
}

/**
 * Complete ID freeing, extended version for corner cases.
 * Can override default (and safe!) freeing process, to gain some speed up.
 *
 * At that point, given id is assumed to not be used by any other data-block already
 * (might not be actually true, in case e.g. several inter-related IDs get freed together...).
 * However, they might still be using (referencing) other IDs, this code takes care of it if
 * #LIB_TAG_NO_USER_REFCOUNT is not defined.
 *
 * \param bmain: #Main database containing the freed #ID,
 * can be NULL in case it's a temp ID outside of any #Main.
 * \param idv: Pointer to ID to be freed.
 * \param flag: Set of \a LIB_ID_FREE_... flags controlling/overriding usual freeing process,
 * 0 to get default safe behavior.
 * \param use_flag_from_idtag: Still use freeing info flags from given #ID datablock,
 * even if some overriding ones are passed in \a flag parameter.
 */
void BKE_id_free_ex(Main *bmain, void *idv, int flag, const bool use_flag_from_idtag)
{
  ID *id = idv;

  if (use_flag_from_idtag) {
    if ((id->tag & LIB_TAG_NO_MAIN) != 0) {
      flag |= LIB_ID_FREE_NO_MAIN | LIB_ID_FREE_NO_UI_USER | LIB_ID_FREE_NO_DEG_TAG;
    }
    else {
      flag &= ~LIB_ID_FREE_NO_MAIN;
    }

    if ((id->tag & LIB_TAG_NO_USER_REFCOUNT) != 0) {
      flag |= LIB_ID_FREE_NO_USER_REFCOUNT;
    }
    else {
      flag &= ~LIB_ID_FREE_NO_USER_REFCOUNT;
    }

    if ((id->tag & LIB_TAG_NOT_ALLOCATED) != 0) {
      flag |= LIB_ID_FREE_NOT_ALLOCATED;
    }
    else {
      flag &= ~LIB_ID_FREE_NOT_ALLOCATED;
    }
  }

  BLI_assert((flag & LIB_ID_FREE_NO_MAIN) != 0 || bmain != NULL);
  BLI_assert((flag & LIB_ID_FREE_NO_MAIN) != 0 || (flag & LIB_ID_FREE_NOT_ALLOCATED) == 0);
  BLI_assert((flag & LIB_ID_FREE_NO_MAIN) != 0 || (flag & LIB_ID_FREE_NO_USER_REFCOUNT) == 0);

  const short type = GS(id->name);

  if (bmain && (flag & LIB_ID_FREE_NO_DEG_TAG) == 0) {
    DEG_id_type_tag(bmain, type);
  }

#ifdef WITH_PYTHON
#  ifdef WITH_PYTHON_SAFETY
  BPY_id_release(id);
#  endif
  if (id->py_instance) {
    BPY_DECREF_RNA_INVALIDATE(id->py_instance);
  }
#endif

  if ((flag & LIB_ID_FREE_NO_USER_REFCOUNT) == 0) {
    BKE_libblock_relink_ex(bmain, id, NULL, NULL, true);
  }

  BKE_libblock_free_datablock(id, flag);

  /* avoid notifying on removed data */
  if ((flag & LIB_ID_FREE_NO_MAIN) == 0) {
    BKE_main_lock(bmain);
  }

  if ((flag & LIB_ID_FREE_NO_UI_USER) == 0) {
    if (free_notifier_reference_cb) {
      free_notifier_reference_cb(id);
    }

    if (remap_editor_id_reference_cb) {
      remap_editor_id_reference_cb(id, NULL);
    }
  }

  if ((flag & LIB_ID_FREE_NO_MAIN) == 0) {
    ListBase *lb = which_libbase(bmain, type);
    BLI_remlink(lb, id);
  }

  BKE_libblock_free_data(id, (flag & LIB_ID_FREE_NO_USER_REFCOUNT) == 0);

  if ((flag & LIB_ID_FREE_NO_MAIN) == 0) {
    BKE_main_unlock(bmain);
  }

  if ((flag & LIB_ID_FREE_NOT_ALLOCATED) == 0) {
    MEM_freeN(id);
  }
}

/**
 * Complete ID freeing, should be usable in most cases (even for out-of-Main IDs).
 *
 * See #BKE_id_free_ex description for full details.
 *
 * \param bmain: Main database containing the freed ID,
 * can be NULL in case it's a temp ID outside of any Main.
 * \param idv: Pointer to ID to be freed.
 */
void BKE_id_free(Main *bmain, void *idv)
{
  BKE_id_free_ex(bmain, idv, 0, true);
}

/**
 * Not really a freeing function by itself,
 * it decrements usercount of given id, and only frees it if it reaches 0.
 */
void BKE_id_free_us(Main *bmain, void *idv) /* test users */
{
  ID *id = idv;

  id_us_min(id);

  /* XXX This is a temp (2.77) hack so that we keep same behavior as in 2.76 regarding collections
   *     when deleting an object. Since only 'user_one' usage of objects is collections,
   *     and only 'real user' usage of objects is scenes, removing that 'user_one' tag when there
   *     is no more real (scene) users of an object ensures it gets fully unlinked.
   *     But only for local objects, not linked ones!
   *     Otherwise, there is no real way to get rid of an object anymore -
   *     better handling of this is TODO.
   */
  if ((GS(id->name) == ID_OB) && (id->us == 1) && (id->lib == NULL)) {
    id_us_clear_real(id);
  }

  if (id->us == 0) {
    BKE_libblock_unlink(bmain, id, false, false);

    BKE_id_free(bmain, id);
  }
}

static void id_delete(Main *bmain, const bool do_tagged_deletion)
{
  const int tag = LIB_TAG_DOIT;
  ListBase *lbarray[MAX_LIBARRAY];
  Link dummy_link = {0};
  int base_count, i;

  /* Used by batch tagged deletion, when we call BKE_id_free then, id is no more in Main database,
   * and has already properly unlinked its other IDs usages.
   * UI users are always cleared in BKE_libblock_remap_locked() call, so we can always skip it. */
  const int free_flag = LIB_ID_FREE_NO_UI_USER |
                        (do_tagged_deletion ? LIB_ID_FREE_NO_MAIN | LIB_ID_FREE_NO_USER_REFCOUNT :
                                              0);
  ListBase tagged_deleted_ids = {NULL};

  base_count = set_listbasepointers(bmain, lbarray);

  BKE_main_lock(bmain);
  if (do_tagged_deletion) {
    /* Main idea of batch deletion is to remove all IDs to be deleted from Main database.
     * This means that we won't have to loop over all deleted IDs to remove usages
     * of other deleted IDs.
     * This gives tremendous speed-up when deleting a large amount of IDs from a Main
     * containing thousands of those.
     * This also means that we have to be very careful here, as we by-pass many 'common'
     * processing, hence risking to 'corrupt' at least user counts, if not IDs themselves. */
    bool keep_looping = true;
    while (keep_looping) {
      ID *id, *id_next;
      ID *last_remapped_id = tagged_deleted_ids.last;
      keep_looping = false;

      /* First tag and remove from Main all datablocks directly from target lib.
       * Note that we go forward here, since we want to check dependencies before users
       * (e.g. meshes before objects). Avoids to have to loop twice. */
      for (i = 0; i < base_count; i++) {
        ListBase *lb = lbarray[i];

        for (id = lb->first; id; id = id_next) {
          id_next = id->next;
          /* Note: in case we delete a library, we also delete all its datablocks! */
          if ((id->tag & tag) || (id->lib != NULL && (id->lib->id.tag & tag))) {
            BLI_remlink(lb, id);
            BLI_addtail(&tagged_deleted_ids, id);
            /* Do not tag as no_main now, we want to unlink it first (lower-level ID management
             * code has some specific handling of 'nom main'
             * IDs that would be a problem in that case). */
            id->tag |= tag;
            keep_looping = true;
          }
        }
      }
      if (last_remapped_id == NULL) {
        dummy_link.next = tagged_deleted_ids.first;
        last_remapped_id = (ID *)(&dummy_link);
      }
      for (id = last_remapped_id->next; id; id = id->next) {
        /* Will tag 'never NULL' users of this ID too.
         * Note that we cannot use BKE_libblock_unlink() here,
         * since it would ignore indirect (and proxy!)
         * links, this can lead to nasty crashing here in second, actual deleting loop.
         * Also, this will also flag users of deleted data that cannot be unlinked
         * (object using deleted obdata, etc.), so that they also get deleted. */
        BKE_libblock_remap_locked(
            bmain, id, NULL, ID_REMAP_FLAG_NEVER_NULL_USAGE | ID_REMAP_FORCE_NEVER_NULL_USAGE);
        /* Since we removed ID from Main,
         * we also need to unlink its own other IDs usages ourself. */
        BKE_libblock_relink_ex(bmain, id, NULL, NULL, true);
        /* Now we can safely mark that ID as not being in Main database anymore. */
        id->tag |= LIB_TAG_NO_MAIN;
        /* This is needed because we may not have remapped usages
         * of that ID by other deleted ones. */
        // id->us = 0;  /* Is it actually? */
      }
    }
  }
  else {
    /* First tag all datablocks directly from target lib.
     * Note that we go forward here, since we want to check dependencies before users
     * (e.g. meshes before objects).
     * Avoids to have to loop twice. */
    for (i = 0; i < base_count; i++) {
      ListBase *lb = lbarray[i];
      ID *id, *id_next;

      for (id = lb->first; id; id = id_next) {
        id_next = id->next;
        /* Note: in case we delete a library, we also delete all its datablocks! */
        if ((id->tag & tag) || (id->lib != NULL && (id->lib->id.tag & tag))) {
          id->tag |= tag;

          /* Will tag 'never NULL' users of this ID too.
           * Note that we cannot use BKE_libblock_unlink() here, since it would ignore indirect
           * (and proxy!) links, this can lead to nasty crashing here in second,
           * actual deleting loop.
           * Also, this will also flag users of deleted data that cannot be unlinked
           * (object using deleted obdata, etc.), so that they also get deleted. */
          BKE_libblock_remap_locked(
              bmain, id, NULL, ID_REMAP_FLAG_NEVER_NULL_USAGE | ID_REMAP_FORCE_NEVER_NULL_USAGE);
        }
      }
    }
  }
  BKE_main_unlock(bmain);

  /* In usual reversed order, such that all usage of a given ID, even 'never NULL' ones,
   * have been already cleared when we reach it
   * (e.g. Objects being processed before meshes, they'll have already released their 'reference'
   * over meshes when we come to freeing obdata). */
  for (i = do_tagged_deletion ? 1 : base_count; i--;) {
    ListBase *lb = lbarray[i];
    ID *id, *id_next;

    for (id = do_tagged_deletion ? tagged_deleted_ids.first : lb->first; id; id = id_next) {
      id_next = id->next;
      if (id->tag & tag) {
        if (id->us != 0) {
#ifdef DEBUG_PRINT
          printf("%s: deleting %s (%d)\n", __func__, id->name, id->us);
#endif
          BLI_assert(id->us == 0);
        }
        BKE_id_free_ex(bmain, id, free_flag, !do_tagged_deletion);
      }
    }
  }

  bmain->is_memfile_undo_written = false;
}

/**
 * Properly delete a single ID from given \a bmain database.
 */
void BKE_id_delete(Main *bmain, void *idv)
{
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  ((ID *)idv)->tag |= LIB_TAG_DOIT;

  id_delete(bmain, false);
}

/**
 * Properly delete all IDs tagged with \a LIB_TAG_DOIT, in given \a bmain database.
 *
 * This is more efficient than calling #BKE_id_delete repetitively on a large set of IDs
 * (several times faster when deleting most of the IDs at once)...
 *
 * \warning Considered experimental for now, seems to be working OK but this is
 *          risky code in a complicated area.
 */
void BKE_id_multi_tagged_delete(Main *bmain)
{
  id_delete(bmain, true);
}
