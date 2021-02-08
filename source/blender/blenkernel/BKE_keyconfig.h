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
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Based on #BKE_addon_pref_type_init and friends */

struct UserDef;
struct wmKeyConfigPref;
struct wmKeyMap;
struct wmKeyMapItem;

/** Actual data is stored in #wmKeyConfigPref. */
#if defined(__RNA_TYPES_H__)
typedef struct wmKeyConfigPrefType_Runtime {
  char idname[64];

  /* RNA integration */
  ExtensionRNA rna_ext;
} wmKeyConfigPrefType_Runtime;

#else
typedef struct wmKeyConfigPrefType_Runtime wmKeyConfigPrefType_Runtime;
#endif

/* KeyConfig preferences (UserDef). */
struct wmKeyConfigPref *BKE_keyconfig_pref_ensure(struct UserDef *userdef, const char *kc_idname);

/* KeyConfig preferences (RNA). */
struct wmKeyConfigPrefType_Runtime *BKE_keyconfig_pref_type_find(const char *idname, bool quiet);
void BKE_keyconfig_pref_type_add(struct wmKeyConfigPrefType_Runtime *kpt_rt);
void BKE_keyconfig_pref_type_remove(const struct wmKeyConfigPrefType_Runtime *kpt_rt);

void BKE_keyconfig_pref_type_init(void);
void BKE_keyconfig_pref_type_free(void);

/* Versioning. */
void BKE_keyconfig_pref_set_select_mouse(struct UserDef *userdef, int value, bool override);

struct wmKeyConfigFilterItemParams {
  uint check_item : 1;
  uint check_diff_item_add : 1;
  uint check_diff_item_remove : 1;
};

void BKE_keyconfig_keymap_filter_item(struct wmKeyMap *keymap,
                                      const struct wmKeyConfigFilterItemParams *params,
                                      bool (*filter_fn)(struct wmKeyMapItem *kmi, void *user_data),
                                      void *user_data);
void BKE_keyconfig_pref_filter_items(struct UserDef *userdef,
                                     const struct wmKeyConfigFilterItemParams *params,
                                     bool (*filter_fn)(struct wmKeyMapItem *kmi, void *user_data),
                                     void *user_data);

#ifdef __cplusplus
}
#endif
