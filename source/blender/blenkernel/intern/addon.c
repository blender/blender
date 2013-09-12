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

/** \file blender/blenkernel/intern/addon.c
 *  \ingroup bke
 */

#include <stddef.h>
#include <stdlib.h>

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_string.h"

#include "BKE_addon.h"  /* own include */

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLF_translation.h"

#include "MEM_guardedalloc.h"

static GHash *global_addonpreftype_hash = NULL;


bAddonPrefType *BKE_addon_pref_type_find(const char *idname, int quiet)
{
	if (idname[0]) {
		bAddonPrefType *apt;

		apt = BLI_ghash_lookup(global_addonpreftype_hash, idname);
		if (apt) {
			return apt;
		}

		if (!quiet) {
			printf("search for unknown addon-pref '%s'\n", idname);
		}
	}
	else {
		if (!quiet) {
			printf("search for empty addon-pref");
		}
	}

	return NULL;
}

void BKE_addon_pref_type_add(bAddonPrefType *apt)
{
	BLI_ghash_insert(global_addonpreftype_hash, (void *)apt->idname, apt);
}

void BKE_addon_pref_type_remove(bAddonPrefType *apt)
{
	BLI_ghash_remove(global_addonpreftype_hash, (void *)apt->idname, NULL, MEM_freeN);
}

void BKE_addon_pref_type_init(void)
{
	BLI_assert(global_addonpreftype_hash == NULL);
	global_addonpreftype_hash = BLI_ghash_str_new(__func__);
}

void BKE_addon_pref_type_free(void)
{
	BLI_ghash_free(global_addonpreftype_hash, NULL, MEM_freeN);
	global_addonpreftype_hash = NULL;
}
