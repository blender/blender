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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_toolbar.c
 *  \ingroup spview3d
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "view3d_intern.h"  /* own include */

/* ********** operator to open/close toolshelf region */

static int view3d_toolshelf_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = view3d_has_tools_region(sa);

	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_toolshelf(wmOperatorType *ot)
{
	ot->name = "Toggle Toolbar";
	ot->description = "Toggles tool shelf display";
	ot->idname = "VIEW3D_OT_toolshelf";

	ot->exec = view3d_toolshelf_toggle_exec;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = 0;
}
