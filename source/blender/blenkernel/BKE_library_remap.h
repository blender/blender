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
#ifndef __BKE_LIBRARY_REMAP_H__
#define __BKE_LIBRARY_REMAP_H__

/** \file
 * \ingroup bke
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

/* BKE_libblock_free, delete are declared in BKE_library.h for convenience. */

/* Also IDRemap->flag. */
enum {
  /** Do not remap indirect usages of IDs (that is, when user is some linked data). */
  ID_REMAP_SKIP_INDIRECT_USAGE = 1 << 0,
  /**
   * This flag should always be set, *except for 'unlink' scenarios*
   * (only relevant when new_id == NULL).
   * Basically, when unset, NEVER_NULL ID usages will keep pointing to old_id, but (if needed)
   * old_id user count will still be decremented.
   * This is mandatory for 'delete ID' case,
   * but in all other situation this would lead to invalid user counts!
   */
  ID_REMAP_SKIP_NEVER_NULL_USAGE = 1 << 1,
  /**
   * This tells the callback func to flag with #LIB_DOIT all IDs
   * using target one with a 'never NULL' pointer (like e.g. #Object.data).
   */
  ID_REMAP_FLAG_NEVER_NULL_USAGE = 1 << 2,
  /**
   * This tells the callback func to force setting IDs
   * using target one with a 'never NULL' pointer to NULL.
   * \warning Use with extreme care, this will leave database in broken state
   * and can cause crashes very easily!
   */
  ID_REMAP_FORCE_NEVER_NULL_USAGE = 1 << 3,
  /**
   * Do not consider proxy/_group pointers of local objects as indirect usages...
   * Our oh-so-beloved proxies again...
   * Do not consider data used by local proxy object as indirect usage.
   * This is needed e.g. in reload scenario,
   * since we have to ensure remapping of Armature data of local proxy
   * is also performed. Usual nightmare...
   */
  ID_REMAP_NO_INDIRECT_PROXY_DATA_USAGE = 1 << 4,
  /** Do not remap static override pointers. */
  ID_REMAP_SKIP_STATIC_OVERRIDE = 1 << 5,
};

/* Note: Requiring new_id to be non-null, this *may* not be the case ultimately,
 * but makes things simpler for now. */
void BKE_libblock_remap_locked(struct Main *bmain,
                               void *old_idv,
                               void *new_idv,
                               const short remap_flags) ATTR_NONNULL(1, 2);
void BKE_libblock_remap(struct Main *bmain, void *old_idv, void *new_idv, const short remap_flags)
    ATTR_NONNULL(1, 2);

void BKE_libblock_unlink(struct Main *bmain,
                         void *idv,
                         const bool do_flag_never_null,
                         const bool do_skip_indirect) ATTR_NONNULL();

void BKE_libblock_relink_ex(struct Main *bmain,
                            void *idv,
                            void *old_idv,
                            void *new_idv,
                            const bool us_min_never_null) ATTR_NONNULL(1, 2);

void BKE_libblock_relink_to_newid(struct ID *id) ATTR_NONNULL();

typedef void (*BKE_library_free_window_manager_cb)(struct bContext *, struct wmWindowManager *);
typedef void (*BKE_library_free_notifier_reference_cb)(const void *);
typedef void (*BKE_library_remap_editor_id_reference_cb)(struct ID *, struct ID *);

void BKE_library_callback_free_window_manager_set(BKE_library_free_window_manager_cb func);
void BKE_library_callback_free_notifier_reference_set(BKE_library_free_notifier_reference_cb func);
void BKE_library_callback_remap_editor_id_reference_set(
    BKE_library_remap_editor_id_reference_cb func);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_LIBRARY_REMAP_H__ */
