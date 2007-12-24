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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"

#include "BIF_toolbox.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_window.h"
#include "wm_event_system.h"

static ListBase global_ops= {NULL, NULL};

/* ************ operator API, exported ********** */

wmOperatorType *WM_operatortype_find(const char *idname)
{
	wmOperatorType *ot;
	
	for(ot= global_ops.first; ot; ot= ot->next) {
		if(strncmp(ot->idname, idname, OP_MAX_TYPENAME)==0)
		   return ot;
	}
	return NULL;
}

/* ************ default ops, exported *********** */

int WM_operator_confirm(bContext *C, wmOperator *op, wmEvent *event)
{
//	if(okee(op->type->name)) {
//		return op->type->exec(C, op);
//	}
	return 0;
}
int WM_operator_winactive(bContext *C)
{
	if(C->window==NULL) return 0;
	return 1;
}

/* ************ window / screen operator definitions ************** */

static void WM_OT_window_duplicate(wmOperatorType *ot)
{
	ot->name= "Duplicate Window";
	ot->idname= "WM_OT_window_duplicate";
	
	ot->interactive= NULL; //WM_operator_confirm;
	ot->exec= wm_window_duplicate_op;
	ot->poll= WM_operator_winactive;
}

static void WM_OT_save_homefile(wmOperatorType *ot)
{
	ot->name= "Save User Settings";
	ot->idname= "WM_OT_save_homefile";
	
	ot->interactive= NULL; //WM_operator_confirm;
	ot->exec= WM_write_homefile;
	ot->poll= WM_operator_winactive;
	
	ot->flag= OPTYPE_REGISTER;
}



#define ADD_OPTYPE(opfunc)	ot= MEM_callocN(sizeof(wmOperatorType), "operatortype"); \
							opfunc(ot);  \
							BLI_addtail(&global_ops, ot)


/* called on initialize WM_exit() */
void wm_operatortype_free(void)
{
	BLI_freelistN(&global_ops);
}

/* called on initialize WM_init() */
void wm_operatortype_init(void)
{
	wmOperatorType *ot;
	
	ADD_OPTYPE(WM_OT_window_duplicate);
	ADD_OPTYPE(WM_OT_save_homefile);
}


