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
 * Contributor(s): Blender Foundation, Andrea Weikert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_file/file_panels.c
 *  \ingroup spfile
 */

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLT_translation.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_fileselect.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "file_intern.h"
#include "fsmenu.h"

#include <string.h>

static bool file_panel_operator_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	return (sfile && sfile->op);
}

static void file_panel_operator_header(const bContext *C, Panel *pa)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	wmOperator *op = sfile->op;

	BLI_strncpy(pa->drawname, RNA_struct_ui_name(op->type->srna), sizeof(pa->drawname));
}

static void file_panel_operator(const bContext *C, Panel *pa)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	wmOperator *op = sfile->op;

	UI_block_func_set(uiLayoutGetBlock(pa->layout), file_draw_check_cb, NULL, NULL);

	/* Hack: temporary hide.*/
	const char *hide[4] = {"filepath", "files", "directory", "filename"};
	for (int i = 0; i < ARRAY_SIZE(hide); i++) {
		PropertyRNA *prop = RNA_struct_find_property(op->ptr, hide[i]);
		if (prop) {
			RNA_def_property_flag(prop, PROP_HIDDEN);
		}
	}

	uiTemplateOperatorPropertyButs(
	        C, pa->layout, op, UI_BUT_LABEL_ALIGN_NONE,
	        UI_TEMPLATE_OP_PROPS_SHOW_EMPTY);

	/* Hack: temporary hide.*/
	for (int i = 0; i < ARRAY_SIZE(hide); i++) {
		PropertyRNA *prop = RNA_struct_find_property(op->ptr, hide[i]);
		if (prop) {
			RNA_def_property_clear_flag(prop, PROP_HIDDEN);
		}
	}

	UI_block_func_set(uiLayoutGetBlock(pa->layout), NULL, NULL, NULL);
}

void file_panels_register(ARegionType *art)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), "spacetype file operator properties");
	strcpy(pt->idname, "FILE_PT_operator");
	strcpy(pt->label, N_("Operator"));
	strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->poll = file_panel_operator_poll;
	pt->draw_header = file_panel_operator_header;
	pt->draw = file_panel_operator;
	BLI_addtail(&art->paneltypes, pt);
}
