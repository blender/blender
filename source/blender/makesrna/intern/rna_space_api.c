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
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_space_api.c
 *  \ingroup RNA
 */

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

static void rna_RegionView3D_update(ID *id, RegionView3D *rv3d)
{
	bScreen *sc = (bScreen *)id;

	ScrArea *sa;
	ARegion *ar;

	area_region_from_regiondata(sc, rv3d, &sa, &ar);

	if (sa && ar && sa->spacetype == SPACE_VIEW3D) {
		View3D *v3d;

		v3d = (View3D *)sa->spacedata.first;

		ED_view3d_update_viewmat(sc->scene, v3d, ar, NULL, NULL);
	}
}

#else

void RNA_api_region_view3d(StructRNA *srna)
{
	FunctionRNA *func;

	func = RNA_def_function(srna, "update", "rna_RegionView3D_update");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Recalculate the view matrices");
}

void RNA_api_space_node(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "cursor_location_from_region", "rna_SpaceNodeEditor_cursor_location_from_region");
	RNA_def_function_ui_description(func, "Set the cursor location using region coordinates");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_int(func, "x", 0, INT_MIN, INT_MAX, "x", "Region x coordinate", -10000, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "y", 0, INT_MIN, INT_MAX, "y", "Region y coordinate", -10000, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

#endif
