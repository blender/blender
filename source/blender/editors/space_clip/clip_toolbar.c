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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_toolbar.c
 *  \ingroup spclip
 */

#include <string.h>

#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "RNA_access.h"

#include "WM_types.h"
#include "WM_api.h"

#include "ED_screen.h"
#include "ED_undo.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "clip_intern.h" /* own include */

/* ************************ header area region *********************** */

/************************** properties ******************************/

ARegion *ED_clip_has_properties_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar = BKE_area_find_region_type(sa, RGN_TYPE_UI);
	if (ar)
		return ar;

	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

	/* is error! */
	if (ar == NULL)
		return NULL;

	arnew = MEM_callocN(sizeof(ARegion), "clip properties region");

	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_UI;
	arnew->alignment = RGN_ALIGN_RIGHT;

	arnew->flag = RGN_FLAG_HIDDEN;

	return arnew;
}

static bool properties_poll(bContext *C)
{
	return (CTX_wm_space_clip(C) != NULL);
}

static int properties_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = ED_clip_has_properties_region(sa);

	if (ar && ar->alignment != RGN_ALIGN_NONE)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void CLIP_OT_properties(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Sidebar";
	ot->description = "Toggle the properties region visibility";
	ot->idname = "CLIP_OT_properties";

	/* api callbacks */
	ot->exec = properties_exec;
	ot->poll = properties_poll;
}

/************************** tools ******************************/

static ARegion *clip_has_tools_region(ScrArea *sa)
{
	ARegion *ar, *artool = NULL, *arhead;

	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_TOOLS)
			artool = ar;
	}

	/* tool region hide/unhide also hides props */
	if (artool) {
		return artool;
	}

	if (artool == NULL) {
		/* add subdiv level; after header */
		arhead = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

		/* is error! */
		if (arhead == NULL)
			return NULL;

		artool = MEM_callocN(sizeof(ARegion), "clip tools region");

		BLI_insertlinkafter(&sa->regionbase, arhead, artool);
		artool->regiontype = RGN_TYPE_TOOLS;
		artool->alignment = RGN_ALIGN_LEFT;

		artool->flag = RGN_FLAG_HIDDEN;
	}

	return artool;
}

static bool tools_poll(bContext *C)
{
	return (CTX_wm_space_clip(C) != NULL);
}

static int tools_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = clip_has_tools_region(sa);

	if (ar && ar->alignment != RGN_ALIGN_NONE)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void CLIP_OT_tools(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Toolbar";
	ot->description = "Toggle clip tools panel";
	ot->idname = "CLIP_OT_tools";

	/* api callbacks */
	ot->exec = tools_exec;
	ot->poll = tools_poll;
}
