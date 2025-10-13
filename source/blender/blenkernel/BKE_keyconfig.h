/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_sys_types.h"

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

/* KeyConfig preferences (#UserDef). */

struct wmKeyConfigPref *BKE_keyconfig_pref_ensure(struct UserDef *userdef, const char *kc_idname);

/* KeyConfig preferences (RNA). */

struct wmKeyConfigPrefType_Runtime *BKE_keyconfig_pref_type_find(const char *idname, bool quiet);
void BKE_keyconfig_pref_type_add(struct wmKeyConfigPrefType_Runtime *kpt_rt);
void BKE_keyconfig_pref_type_remove(const struct wmKeyConfigPrefType_Runtime *kpt_rt);

void BKE_keyconfig_pref_type_init(void);
void BKE_keyconfig_pref_type_free(void);

/* Versioning. */

/**
 * Set select mouse, for versioning code.
 */
void BKE_keyconfig_pref_set_select_mouse(struct UserDef *userdef, int value, bool override);

struct wmKeyConfigFilterItemParams {
  uint check_item : 1;
  uint check_diff_item_add : 1;
  uint check_diff_item_remove : 1;
};
/** Use when all items should be manipulated. */
#define WM_KEY_CONFIG_FILTER_ITEM_ALL {true, true, true}

void BKE_keyconfig_keymap_filter_item(struct wmKeyMap *keymap,
                                      const struct wmKeyConfigFilterItemParams *params,
                                      bool (*filter_fn)(struct wmKeyMapItem *kmi, void *user_data),
                                      void *user_data);
/**
 * Filter & optionally remove key-map items,
 * intended for versioning, but may be used in other situations too.
 */
void BKE_keyconfig_pref_filter_items(struct UserDef *userdef,
                                     const struct wmKeyConfigFilterItemParams *params,
                                     bool (*filter_fn)(struct wmKeyMapItem *kmi, void *user_data),
                                     void *user_data);
