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
 * Contains management of ID's for freeing & deletion.
 */

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_ID.h"
#include "DNA_key_types.h"

#include "BLI_utildefines.h"

#include "BLI_listbase.h"

#include "BKE_anim_data.h"
#include "BKE_asset.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_remap.h"
#include "BKE_library.h"
#include "BKE_main.h"

#include "lib_intern.h"

#include "DEG_depsgraph.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

/* Not used currently. */
// static CLG_LogRef LOG = {.identifier = "bke.lib_id_delete"};

void BKE_libblock_free_data(ID *id, const bool do_id_user)
{
  if (id->properties) {
    IDP_FreePropertyContent_ex(id->properties, do_id_user);
    MEM_freeN(id->properties);
    id->properties = NULL;
  }

  if (id->override_library) {
    BKE_lib_override_library_free(&id->override_library, do_id_user);
    id->override_library = NULL;
  }

  if (id->asset_data) {
    BKE_asset_metadata_free(&id->asset_data);
  }

  BKE_animdata_free(id, do_id_user);
}

void BKE_libblock_free_datablock(ID *id, const int UNUSED(flag))
{
  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info != NULL) {
    if (idtype_info->free_data != NULL) {
      idtype_info->free_data(id);
    }
    return;
  }

  BLI_assert(!"IDType Missing IDTypeInfo");
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
    BLI_assert(bmain->is_locked_for_linking == false);

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

  Key *key = ((flag & LIB_ID_FREE_NO_MAIN) == 0) ? BKE_key_from_id(id) : NULL;

  if ((flag & LIB_ID_FREE_NO_USER_REFCOUNT) == 0) {
    BKE_libblock_relink_ex(bmain, id, NULL, NULL, 0);
  }

  if ((flag & LIB_ID_FREE_NO_MAIN) == 0 && key != NULL) {
    BKE_id_free_ex(bmain, &key->id, flag, use_flag_from_idtag);
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

static size_t id_delete(Main *bmain, const bool do_tagged_deletion)
{
  const int tag = LIB_TAG_DOIT;
  ListBase *lbarray[INDEX_ID_MAX];
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
             * code has some specific handling of 'no main' IDs that would be a problem in that
             * case). */
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
        BKE_libblock_remap_locked(bmain,
                                  id,
                                  NULL,
                                  (ID_REMAP_FLAG_NEVER_NULL_USAGE |
                                   ID_REMAP_FORCE_NEVER_NULL_USAGE |
                                   ID_REMAP_FORCE_INTERNAL_RUNTIME_POINTERS));
        /* Since we removed ID from Main,
         * we also need to unlink its own other IDs usages ourself. */
        BKE_libblock_relink_ex(bmain, id, NULL, NULL, ID_REMAP_FORCE_INTERNAL_RUNTIME_POINTERS);
      }
    }

    /* Now we can safely mark that ID as not being in Main database anymore. */
    /* NOTE: This needs to be done in a separate loop than above, otherwise some usercounts of
     * deleted IDs may not be properly decreased by the remappings (since `NO_MAIN` ID usercounts
     * is never affected). */
    for (ID *id = tagged_deleted_ids.first; id; id = id->next) {
      id->tag |= LIB_TAG_NO_MAIN;
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
          BKE_libblock_remap_locked(bmain,
                                    id,
                                    NULL,
                                    (ID_REMAP_FLAG_NEVER_NULL_USAGE |
                                     ID_REMAP_FORCE_NEVER_NULL_USAGE |
                                     ID_REMAP_FORCE_INTERNAL_RUNTIME_POINTERS));
        }
      }
    }
  }
  BKE_main_unlock(bmain);

  /* In usual reversed order, such that all usage of a given ID, even 'never NULL' ones,
   * have been already cleared when we reach it
   * (e.g. Objects being processed before meshes, they'll have already released their 'reference'
   * over meshes when we come to freeing obdata). */
  size_t num_datablocks_deleted = 0;
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
        ++num_datablocks_deleted;
      }
    }
  }

  bmain->is_memfile_undo_written = false;
  return num_datablocks_deleted;
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
 * \return Number of deleted datablocks.
 */
size_t BKE_id_multi_tagged_delete(Main *bmain)
{
  return id_delete(bmain, true);
}
