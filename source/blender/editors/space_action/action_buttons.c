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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_action/action_buttons.c
 *  \ingroup spaction
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_screen.h"
#include "BKE_unit.h"


#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "action_intern.h"   // own include

/* ******************* action editor space & buttons ************** */

/* ******************* general ******************************** */

void action_buttons_register(ARegionType *UNUSED(art))
{
#if 0
	PanelType *pt;

	// TODO: AnimData / Actions List

	pt = MEM_callocN(sizeof(PanelType), "spacetype action panel properties");
	strcpy(pt->idname, "ACTION_PT_properties");
	strcpy(pt->label, N_("Active F-Curve"));
	strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = action_anim_panel_properties;
	pt->poll = action_anim_panel_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype action panel properties");
	strcpy(pt->idname, "ACTION_PT_key_properties");
	strcpy(pt->label, N_("Active Keyframe"));
	strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = action_anim_panel_key_properties;
	pt->poll = action_anim_panel_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype action panel modifiers");
	strcpy(pt->idname, "ACTION_PT_modifiers");
	strcpy(pt->label, N_("Modifiers"));
	strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = action_anim_panel_modifiers;
	pt->poll = action_anim_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
#endif
}

static int action_properties_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = action_has_buttons_region(sa);

	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void ACTION_OT_properties(wmOperatorType *ot)
{
	ot->name = "Toggle Sidebar";
	ot->idname = "ACTION_OT_properties";
	ot->description = "Toggle the properties region visibility";

	ot->exec = action_properties_toggle_exec;
	ot->poll = ED_operator_action_active;

	/* flags */
	ot->flag = 0;
}
