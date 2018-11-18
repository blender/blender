/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_KEYCONFIG_H__
#define __BKE_KEYCONFIG_H__

/** \file BKE_keyconfig.h
 *  \ingroup bke
 */

/** Based on #BKE_addon_pref_type_init and friends */

struct UserDef;
struct wmKeyConfigPref;

/** Actual data is stored in #wmKeyConfigPref. */
#if defined(__RNA_TYPES_H__)
typedef struct wmKeyConfigPrefType_Runtime {
	char idname[64];

	/* RNA integration */
	ExtensionRNA ext;
} wmKeyConfigPrefType_Runtime;

#else
typedef struct wmKeyConfigPrefType_Runtime wmKeyConfigPrefType_Runtime;
#endif

/* KeyConfig preferenes (UserDef). */
struct wmKeyConfigPref *BKE_keyconfig_pref_ensure(struct UserDef *userdef, const char *kc_idname);

/* KeyConfig preferenes (RNA). */
struct wmKeyConfigPrefType_Runtime *BKE_keyconfig_pref_type_find(const char *idname, bool quiet);
void BKE_keyconfig_pref_type_add(struct wmKeyConfigPrefType_Runtime *kpt_rt);
void BKE_keyconfig_pref_type_remove(const struct wmKeyConfigPrefType_Runtime *kpt_rt);

void BKE_keyconfig_pref_type_init(void);
void BKE_keyconfig_pref_type_free(void);

#endif  /* __BKE_KEYCONFIG_H__ */
