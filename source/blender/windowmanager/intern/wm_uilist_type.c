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

/** \file blender/windowmanager/intern/wm_uilist_type.c
 *  \ingroup wm
 *
 * UI List Registry.
 */

#include "BLI_sys_types.h"

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

static GHash *uilisttypes_hash = NULL;

uiListType *WM_uilisttype_find(const char *idname, bool quiet)
{
	uiListType *ult;

	if (idname[0]) {
		ult = BLI_ghash_lookup(uilisttypes_hash, idname);
		if (ult) {
			return ult;
		}
	}

	if (!quiet) {
		printf("search for unknown uilisttype %s\n", idname);
	}

	return NULL;
}

bool WM_uilisttype_add(uiListType *ult)
{
	BLI_ghash_insert(uilisttypes_hash, ult->idname, ult);
	return 1;
}

void WM_uilisttype_freelink(uiListType *ult)
{
	bool ok;

	ok = BLI_ghash_remove(uilisttypes_hash, ult->idname, NULL, MEM_freeN);

	BLI_assert(ok);
	(void)ok;
}

/* called on initialize WM_init() */
void WM_uilisttype_init(void)
{
	uilisttypes_hash = BLI_ghash_str_new_ex("uilisttypes_hash gh", 16);
}

void WM_uilisttype_free(void)
{
	GHashIterator gh_iter;

	GHASH_ITER (gh_iter, uilisttypes_hash) {
		uiListType *ult = BLI_ghashIterator_getValue(&gh_iter);
		if (ult->ext.free) {
			ult->ext.free(ult->ext.data);
		}
	}

	BLI_ghash_free(uilisttypes_hash, NULL, MEM_freeN);
	uilisttypes_hash = NULL;
}
