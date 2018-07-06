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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_ADDON_H__
#define __BKE_ADDON_H__

/** \file BKE_addon.h
 *  \ingroup bke
 */


struct ListBase;
struct bAddon;

#ifdef __RNA_TYPES_H__
typedef struct bAddonPrefType {
	/* type info */
	char idname[64]; // best keep the same size as BKE_ST_MAXNAME

	/* RNA integration */
	ExtensionRNA ext;
} bAddonPrefType;

#else
typedef struct bAddonPrefType bAddonPrefType;
#endif

bAddonPrefType *BKE_addon_pref_type_find(const char *idname, bool quiet);
void            BKE_addon_pref_type_add(bAddonPrefType *apt);
void            BKE_addon_pref_type_remove(const bAddonPrefType *apt);

void            BKE_addon_pref_type_init(void);
void            BKE_addon_pref_type_free(void);

struct bAddon  *BKE_addon_new(void);
struct bAddon  *BKE_addon_find(struct ListBase *addon_list, const char *module);
struct bAddon  *BKE_addon_ensure(struct ListBase *addon_list, const char *module);
bool            BKE_addon_remove_safe(struct ListBase *addon_list, const char *module);
void            BKE_addon_free(struct bAddon *addon);

#endif  /* __BKE_ADDON_H__ */
