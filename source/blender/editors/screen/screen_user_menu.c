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

/** \file blender/editors/screen/screen_user_menu.c
 *  \ingroup spview3d
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_idprop.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void ED_screen_user_menu_add(
        bContext *C, const char *ui_name,
        wmOperatorType *ot, IDProperty *prop, short opcontext)
{
	SpaceLink *sl = CTX_wm_space_data(C);
	bUserMenuItem *umi = MEM_callocN(sizeof(bUserMenuItem), __func__);
	umi->space_type = sl ? sl->spacetype : SPACE_EMPTY;
	umi->opcontext = opcontext;
	if (!STREQ(ui_name, ot->name)) {
		BLI_strncpy(umi->ui_name, ui_name, OP_MAX_TYPENAME);
	}
	BLI_strncpy(umi->opname, ot->idname, OP_MAX_TYPENAME);
	BLI_strncpy(umi->context, CTX_data_mode_string(C), OP_MAX_TYPENAME);
	umi->prop = prop ? IDP_CopyProperty(prop) : NULL;
	BLI_addtail(&U.user_menu_items, umi);
}

void ED_screen_user_menu_remove(bUserMenuItem *umi)
{
	BLI_remlink(&U.user_menu_items, umi);
	if (umi->prop) {
		IDP_FreeProperty(umi->prop);
		MEM_freeN(umi->prop);
	}
	MEM_freeN(umi);
}

bUserMenuItem *ED_screen_user_menu_find(
        bContext *C,
        wmOperatorType *ot, IDProperty *prop, short opcontext)
{
	SpaceLink *sl = CTX_wm_space_data(C);
	const char *context = CTX_data_mode_string(C);
	for (bUserMenuItem *umi = U.user_menu_items.first; umi; umi = umi->next) {
		if (STREQ(ot->idname, umi->opname) &&
		    (opcontext == umi->opcontext) &&
		    (IDP_EqualsProperties(prop, umi->prop)))
		{
			if ((ELEM(umi->space_type, SPACE_TOPBAR) || (sl->spacetype == umi->space_type)) &&
			    (STREQLEN(context, umi->context, OP_MAX_TYPENAME)))
			{
				return umi;
			}
		}
	}
	return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Definition
 * \{ */

static void screen_user_menu_draw(const bContext *C, Menu *menu)
{
	SpaceLink *sl = CTX_wm_space_data(C);
	const char *context = CTX_data_mode_string(C);
	for (bUserMenuItem *umi = U.user_menu_items.first; umi; umi = umi->next) {
		if ((ELEM(umi->space_type, SPACE_TOPBAR) || (sl->spacetype == umi->space_type)) &&
		    (STREQLEN(context, umi->context, OP_MAX_TYPENAME)))
		{
			IDProperty *prop = umi->prop ? IDP_CopyProperty(umi->prop) : NULL;
			uiItemFullO(
			        menu->layout, umi->opname, umi->ui_name[0] ? umi->ui_name : NULL,
			        ICON_NONE, prop, umi->opcontext, 0, NULL);
		}
	}
}

void ED_screen_user_menu_register(void)
{
	MenuType *mt = MEM_callocN(sizeof(MenuType), __func__);
	strcpy(mt->idname, "SCREEN_MT_user_menu");
	strcpy(mt->label, "Quick Favorites");
	strcpy(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
	mt->draw = screen_user_menu_draw;
	WM_menutype_add(mt);
}

/** \} */
