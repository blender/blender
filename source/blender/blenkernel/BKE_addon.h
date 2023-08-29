/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct bAddon;

#ifdef __RNA_TYPES_H__
typedef struct bAddonPrefType {
  /** Type info, match #bAddon::module. */
  char idname[128];

  /* RNA integration */
  ExtensionRNA rna_ext;
} bAddonPrefType;

#else
typedef struct bAddonPrefType bAddonPrefType;
#endif

bAddonPrefType *BKE_addon_pref_type_find(const char *idname, bool quiet);
void BKE_addon_pref_type_add(bAddonPrefType *apt);
void BKE_addon_pref_type_remove(const bAddonPrefType *apt);

void BKE_addon_pref_type_init(void);
void BKE_addon_pref_type_free(void);

struct bAddon *BKE_addon_new(void);
struct bAddon *BKE_addon_find(struct ListBase *addon_list, const char *module);
struct bAddon *BKE_addon_ensure(struct ListBase *addon_list, const char *module);
bool BKE_addon_remove_safe(struct ListBase *addon_list, const char *module);
void BKE_addon_free(struct bAddon *addon);

#ifdef __cplusplus
}
#endif
