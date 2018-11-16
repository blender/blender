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

/** \file blender/blenkernel/intern/keyconfig.c
 *  \ingroup bke
 */

#include <stddef.h>
#include <stdlib.h>

#include "RNA_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "DNA_listBase.h"

#include "BKE_keyconfig.h"  /* own include */

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/** \name Key-Config Preference API
 *
 * \see #BKE_addon_pref_type_init for logic this is bases on.
 * \{ */

static GHash *global_keyconfigpreftype_hash = NULL;


wmKeyConfigPrefType_Runtime *BKE_keyconfig_pref_type_find(const char *idname, bool quiet)
{
	if (idname[0]) {
		wmKeyConfigPrefType_Runtime *kpt_rt;

		kpt_rt = BLI_ghash_lookup(global_keyconfigpreftype_hash, idname);
		if (kpt_rt) {
			return kpt_rt;
		}

		if (!quiet) {
			printf("search for unknown keyconfig-pref '%s'\n", idname);
		}
	}
	else {
		if (!quiet) {
			printf("search for empty keyconfig-pref\n");
		}
	}

	return NULL;
}

void BKE_keyconfig_pref_type_add(wmKeyConfigPrefType_Runtime *kpt_rt)
{
	BLI_ghash_insert(global_keyconfigpreftype_hash, kpt_rt->idname, kpt_rt);
}

void BKE_keyconfig_pref_type_remove(const wmKeyConfigPrefType_Runtime *kpt_rt)
{
	BLI_ghash_remove(global_keyconfigpreftype_hash, kpt_rt->idname, NULL, MEM_freeN);
}

void BKE_keyconfig_pref_type_init(void)
{
	BLI_assert(global_keyconfigpreftype_hash == NULL);
	global_keyconfigpreftype_hash = BLI_ghash_str_new(__func__);
}

void BKE_keyconfig_pref_type_free(void)
{
	BLI_ghash_free(global_keyconfigpreftype_hash, NULL, MEM_freeN);
	global_keyconfigpreftype_hash = NULL;
}

/** \} */
