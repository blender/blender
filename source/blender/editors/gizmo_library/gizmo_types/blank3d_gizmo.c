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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blank3d_gizmo.c
 *  \ingroup wm
 *
 * \name Blank Gizmo
 *
 * \brief Gizmo to use as a fallback (catch events).
 */

#include "BLI_math.h"

#include "BKE_context.h"

#include "ED_view3d.h"
#include "ED_gizmo_library.h"

#include "WM_types.h"
#include "WM_api.h"

/* own includes */
#include "../gizmo_geometry.h"
#include "../gizmo_library_intern.h"


static void gizmo_blank_draw(const bContext *UNUSED(C), wmGizmo *UNUSED(gz))
{
	/* pass */
}

static int gizmo_blank_invoke(
        bContext *UNUSED(C), wmGizmo *UNUSED(gz), const wmEvent *UNUSED(event))
{
	return OPERATOR_RUNNING_MODAL;
}

static int gizmo_blank_test_select(
        bContext *UNUSED(C), wmGizmo *UNUSED(gz), const int UNUSED(mval[2]))
{
	return 0;
}

/* -------------------------------------------------------------------- */
/** \name Blank Gizmo API
 *
 * \{ */

static void GIZMO_GT_blank_3d(wmGizmoType *gzt)
{
	/* identifiers */
	gzt->idname = "GIZMO_GT_blank_3d";

	/* api callbacks */
	gzt->draw = gizmo_blank_draw;
	gzt->invoke = gizmo_blank_invoke;
	gzt->test_select = gizmo_blank_test_select;

	gzt->struct_size = sizeof(wmGizmo);
}

void ED_gizmotypes_blank_3d(void)
{
	WM_gizmotype_append(GIZMO_GT_blank_3d);
}

/** \} */
