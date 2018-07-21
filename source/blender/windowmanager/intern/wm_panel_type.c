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

/** \file blender/windowmanager/intern/wm_panel_type.c
 *  \ingroup wm
 *
 * Panel Registry.
 *
 * \note Unlike menu, and other registries, this doesn't *own* the PanelType.
 *
 * For popups/popovers only, regions handle panel types by including them in local lists.
 */

#include <stdio.h>

#include "BLI_sys_types.h"

#include "DNA_windowmanager_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_screen.h"

#include "WM_api.h"

static GHash *g_paneltypes_hash = NULL;

PanelType *WM_paneltype_find(const char *idname, bool quiet)
{
	PanelType *pt;

	if (idname[0]) {
		pt = BLI_ghash_lookup(g_paneltypes_hash, idname);
		if (pt)
			return pt;
	}

	if (!quiet)
		printf("search for unknown paneltype %s\n", idname);

	return NULL;
}

bool WM_paneltype_add(PanelType *pt)
{
	BLI_ghash_insert(g_paneltypes_hash, pt->idname, pt);
	return true;
}

void WM_paneltype_remove(PanelType *pt)
{
	bool ok;

	ok = BLI_ghash_remove(g_paneltypes_hash, pt->idname, NULL, NULL);

	BLI_assert(ok);
	(void)ok;
}

/* called on initialize WM_init() */
void WM_paneltype_init(void)
{
	/* reserve size is set based on blender default setup */
	g_paneltypes_hash = BLI_ghash_str_new_ex("g_paneltypes_hash gh", 512);
}

void WM_paneltype_clear(void)
{
	BLI_ghash_free(g_paneltypes_hash, NULL, NULL);
}
