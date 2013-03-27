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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_header.c
 *  \ingroup spnode
 */

#include <string.h>

#include "DNA_space_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "node_intern.h"  /* own include */

/* ************************ add menu *********************** */

static void node_menu_add(const bContext *C, Menu *menu)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	uiLayout *layout = menu->layout;
	bNodeTree *ntree = snode->edittree;

	if (!ntree || !ntree->typeinfo || !ntree->typeinfo->draw_add_menu) {
		uiLayoutSetActive(layout, FALSE);
		return;
	}

	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
	uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Search ..."), 0, "NODE_OT_add_search");
	
	ntree->typeinfo->draw_add_menu(C, layout, ntree);
}

void node_menus_register(void)
{
	MenuType *mt;

	mt = MEM_callocN(sizeof(MenuType), "spacetype node menu add");
	strcpy(mt->idname, "NODE_MT_add");
	strcpy(mt->label, N_("Add"));
	strcpy(mt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	mt->draw = node_menu_add;
	WM_menutype_add(mt);
}
