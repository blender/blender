/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_area.h"
#include "ED_screen.h"

#include "screen_intern.h"	/* own module include */

static ListBase local_ops;

int ED_operator_screenactive(bContext *C)
{
	if(C->window==NULL) return 0;
	if(C->screen==NULL) return 0;
	return 1;
}



static void ED_SCR_OT_move_areas(wmOperatorType *ot)
{
    ot->name= "Move area edges";
    ot->idname= "ED_SCR_OT_move_areas";
	
    ot->interactive= NULL;
    ot->exec= NULL;
    ot->poll= ED_operator_screenactive;
}

static void ED_SCR_OT_cursor_type(wmOperatorType *ot)
{
    ot->name= "Cursor type";
    ot->idname= "ED_SCR_OT_cursor_type";
	
    ot->interactive= screen_cursor_test;
    ot->exec= NULL;
    ot->poll= ED_operator_screenactive;
}


#define ADD_OPTYPE(opfunc)	ot= MEM_callocN(sizeof(wmOperatorType), "operatortype"); \
							opfunc(ot);  \
							BLI_addtail(&local_ops, ot)



/* called via wm_init_exit.c ED_spacetypes_init() */
void ED_operatortypes_screen(void)
{
	wmOperatorType *ot;
	
	ADD_OPTYPE( ED_SCR_OT_move_areas );
	ADD_OPTYPE( ED_SCR_OT_cursor_type );
	
	
	
	WM_operatortypelist_append(&local_ops);
}

/* called in wm.c */
void ed_screen_keymap(wmWindowManager *wm)
{
	WM_keymap_verify_item(&wm->screenkeymap, "ED_SCR_OT_cursor_type", MOUSEMOVE, 0, 0, 0);

}



