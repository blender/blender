/*
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup edinterface
 *
 * General Interface Region Code
 *
 * \note Most logic is now in 'interface_region_*.c'
 */

#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "wm_draw.h"

#include "ED_screen.h"

#include "interface_regions_intern.h"

ARegion *ui_region_temp_add(bScreen *sc)
{
	ARegion *ar;

	ar = MEM_callocN(sizeof(ARegion), "area region");
	BLI_addtail(&sc->regionbase, ar);

	ar->regiontype = RGN_TYPE_TEMPORARY;
	ar->alignment = RGN_ALIGN_FLOAT;

	return ar;
}

void ui_region_temp_remove(bContext *C, bScreen *sc, ARegion *ar)
{
	wmWindow *win = CTX_wm_window(C);

	BLI_assert(ar->regiontype == RGN_TYPE_TEMPORARY);
	BLI_assert(BLI_findindex(&sc->regionbase, ar) != -1);
	if (win)
		wm_draw_region_clear(win, ar);

	ED_region_exit(C, ar);
	BKE_area_region_free(NULL, ar);     /* NULL: no spacetype */
	BLI_freelinkN(&sc->regionbase, ar);
}
