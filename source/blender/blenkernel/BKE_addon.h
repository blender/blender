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

#include "RNA_types.h"

typedef struct bAddonPrefType {
	/* type info */
	char idname[64]; // best keep the same size as BKE_ST_MAXNAME

	/* RNA integration */
	ExtensionRNA ext;
} bAddonPrefType;

bAddonPrefType *BKE_addon_pref_type_find(const char *idname, int quiet);
void            BKE_addon_pref_type_add(bAddonPrefType *apt);
void            BKE_addon_pref_type_remove(bAddonPrefType *apt);

void            BKE_addon_pref_type_init(void);
void            BKE_addon_pref_type_free(void);

#endif  /* __BKE_ADDON_H__ */
