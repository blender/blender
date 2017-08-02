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

/** \file blender/editors/space_view3d/view3d_manipulator_forcefield.c
 *  \ingroup spview3d
 */


#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"

#include "DNA_object_types.h"
#include "DNA_object_force.h"

#include "ED_screen.h"
#include "ED_manipulator_library.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */

/** \name Force Field Manipulators
 * \{ */

static bool WIDGETGROUP_forcefield_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	Object *ob = CTX_data_active_object(C);

	return (ob && ob->pd && ob->pd->forcefield);
}

static void WIDGETGROUP_forcefield_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	const float col[4] = {0.8f, 0.8f, 0.45f, 0.5f};
	const float col_hi[4] = {0.8f, 0.8f, 0.45f, 1.0f};

	/* only wind effector for now */
	wmManipulatorWrapper *wwrapper = MEM_mallocN(sizeof(wmManipulatorWrapper), __func__);
	mgroup->customdata = wwrapper;

	wwrapper->manipulator = WM_manipulator_new("MANIPULATOR_WT_arrow_3d", mgroup, NULL);
	RNA_enum_set(wwrapper->manipulator->ptr, "draw_options",  ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED);
	ED_manipulator_arrow3d_set_ui_range(wwrapper->manipulator, -200.0f, 200.0f);
	ED_manipulator_arrow3d_set_range_fac(wwrapper->manipulator, 6.0f);
	WM_manipulator_set_color(wwrapper->manipulator, col);
	WM_manipulator_set_color_highlight(wwrapper->manipulator, col_hi);
}

static void WIDGETGROUP_forcefield_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = mgroup->customdata;
	Object *ob = CTX_data_active_object(C);
	PartDeflect *pd = ob->pd;

	if (pd->forcefield == PFIELD_WIND) {
		const float size = (ob->type == OB_EMPTY) ? ob->empty_drawsize : 1.0f;
		const float ofs[3] = {0.0f, -size, 0.0f};
		PointerRNA field_ptr;

		RNA_pointer_create(&ob->id, &RNA_FieldSettings, pd, &field_ptr);
		WM_manipulator_set_matrix_location(wwrapper->manipulator, ob->obmat[3]);
		WM_manipulator_set_matrix_rotation_from_z_axis(wwrapper->manipulator, ob->obmat[2]);
		WM_manipulator_set_matrix_offset_location(wwrapper->manipulator, ofs);
		WM_manipulator_set_flag(wwrapper->manipulator, WM_MANIPULATOR_HIDDEN, false);
		WM_manipulator_target_property_def_rna(wwrapper->manipulator, "offset", &field_ptr, "strength", -1);
	}
	else {
		WM_manipulator_set_flag(wwrapper->manipulator, WM_MANIPULATOR_HIDDEN, true);
	}
}

void VIEW3D_WGT_force_field(wmManipulatorGroupType *wgt)
{
	wgt->name = "Force Field Widgets";
	wgt->idname = "VIEW3D_WGT_force_field";

	wgt->flag |= (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	              WM_MANIPULATORGROUPTYPE_3D |
	              WM_MANIPULATORGROUPTYPE_SCALE |
	              WM_MANIPULATORGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_forcefield_poll;
	wgt->setup = WIDGETGROUP_forcefield_setup;
	wgt->refresh = WIDGETGROUP_forcefield_refresh;
}

/** \} */

