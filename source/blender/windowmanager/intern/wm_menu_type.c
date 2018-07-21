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

/** \file blender/windowmanager/intern/wm_menu_type.c
 *  \ingroup wm
 *
 * Menu Registry.
 */

#include "BLI_sys_types.h"

#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "WM_api.h"
#include "WM_types.h"

static GHash *menutypes_hash = NULL;

MenuType *WM_menutype_find(const char *idname, bool quiet)
{
	MenuType *mt;

	if (idname[0]) {
		mt = BLI_ghash_lookup(menutypes_hash, idname);
		if (mt)
			return mt;
	}

	if (!quiet)
		printf("search for unknown menutype %s\n", idname);

	return NULL;
}

bool WM_menutype_add(MenuType *mt)
{
	BLI_ghash_insert(menutypes_hash, mt->idname, mt);
	return true;
}

void WM_menutype_freelink(MenuType *mt)
{
	bool ok;

	ok = BLI_ghash_remove(menutypes_hash, mt->idname, NULL, MEM_freeN);

	BLI_assert(ok);
	(void)ok;
}

/* called on initialize WM_init() */
void WM_menutype_init(void)
{
	/* reserve size is set based on blender default setup */
	menutypes_hash = BLI_ghash_str_new_ex("menutypes_hash gh", 512);
}

void WM_menutype_free(void)
{
	GHashIterator gh_iter;

	GHASH_ITER (gh_iter, menutypes_hash) {
		MenuType *mt = BLI_ghashIterator_getValue(&gh_iter);
		if (mt->ext.free) {
			mt->ext.free(mt->ext.data);
		}
	}

	BLI_ghash_free(menutypes_hash, NULL, MEM_freeN);
	menutypes_hash = NULL;
}

bool WM_menutype_poll(bContext *C, MenuType *mt)
{
	/* If we're tagged, only use compatible. */
	if (mt->owner_id[0] != '\0') {
		const WorkSpace *workspace = CTX_wm_workspace(C);
		if (BKE_workspace_owner_id_check(workspace, mt->owner_id) == false) {
			return false;
		}
	}

	if (mt->poll != NULL) {
		return mt->poll(C, mt);
	}
	return true;
}
