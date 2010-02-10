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
 * The Original Code is Copyright (C) 2004 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_text.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_dynstr.h"

#include "BKE_utildefines.h"

#include "ED_armature.h"
#include "ED_particle.h"
#include "ED_curve.h"
#include "ED_mball.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_util.h"
#include "ED_text.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "util_intern.h"

/* ***************** generic undo system ********************* */

void ED_undo_push(bContext *C, char *str)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	Object *obedit= CTX_data_edit_object(C);
	Object *obact= CTX_data_active_object(C);

	if(obedit) {
		if (U.undosteps == 0) return;
		
		if(obedit->type==OB_MESH)
			undo_push_mesh(C, str);
		else if ELEM(obedit->type, OB_CURVE, OB_SURF)
			undo_push_curve(C, str);
		else if (obedit->type==OB_FONT)
			undo_push_font(C, str);
		else if (obedit->type==OB_MBALL)
			undo_push_mball(C, str);
		else if (obedit->type==OB_LATTICE)
			undo_push_lattice(C, str);
		else if (obedit->type==OB_ARMATURE)
			undo_push_armature(C, str);
	}
	else if(obact && obact->mode & OB_MODE_PARTICLE_EDIT) {
		if (U.undosteps == 0) return;
		
		PE_undo_push(CTX_data_scene(C), str);
	}
	else {
		if(U.uiflag & USER_GLOBALUNDO) 
			BKE_write_undo(C, str);
	}
	
	if(wm->file_saved) {
		wm->file_saved= 0;
		WM_event_add_notifier(C, NC_WM|ND_DATACHANGED, NULL);
	}
}

static int ed_undo_step(bContext *C, int step, const char *undoname)
{	
	Object *obedit= CTX_data_edit_object(C);
	Object *obact= CTX_data_active_object(C);
	ScrArea *sa= CTX_wm_area(C);

	if(sa && sa->spacetype==SPACE_IMAGE) {
		SpaceImage *sima= (SpaceImage *)sa->spacedata.first;
		
		if((obact && obact->mode & OB_MODE_TEXTURE_PAINT) || sima->flag & SI_DRAWTOOL) {
			ED_undo_paint_step(C, UNDO_PAINT_IMAGE, step);

			WM_event_add_notifier(C, NC_WINDOW, NULL);
			return OPERATOR_FINISHED;
		}
	}

	if(sa && sa->spacetype==SPACE_TEXT) {
		ED_text_undo_step(C, step);
	}
	else if(obedit) {
		if ELEM7(obedit->type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE) {
			if(undoname)
				undo_editmode_name(C, undoname);
			else
				undo_editmode_step(C, step);
		}
	}
	else {
		int do_glob_undo= 0;
		
		if(obact && obact->mode & OB_MODE_TEXTURE_PAINT)
			ED_undo_paint_step(C, UNDO_PAINT_IMAGE, step);
		else if(obact && obact->mode & OB_MODE_SCULPT)
			ED_undo_paint_step(C, UNDO_PAINT_MESH, step);
		else if(obact && obact->mode & OB_MODE_PARTICLE_EDIT) {
			if(step==1)
				PE_undo(CTX_data_scene(C));
			else
				PE_redo(CTX_data_scene(C));
		}
		else {
			do_glob_undo= 1;
		}
		
		if(do_glob_undo) {
			if(U.uiflag & USER_GLOBALUNDO) {
#ifndef DISABLE_PYTHON
				// XXX		BPY_scripts_clear_pyobjects();
#endif
				if(undoname)
					BKE_undo_name(C, undoname);
				else
					BKE_undo_step(C, step);
			}
			
		}
	}
	
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	
	return OPERATOR_FINISHED;
}

void ED_undo_pop(bContext *C)
{
	ed_undo_step(C, 1, NULL);
}
void ED_undo_redo(bContext *C)
{
	ed_undo_step(C, -1, NULL);
}

void ED_undo_push_op(bContext *C, wmOperator *op)
{
	/* in future, get undo string info? */
	ED_undo_push(C, op->type->name);
}

void ED_undo_pop_op(bContext *C, wmOperator *op)
{
	/* search back a couple of undo's, in case something else added pushes */
	ed_undo_step(C, 0, op->type->name);
}

static int ed_undo_exec(bContext *C, wmOperator *op)
{
	/* "last operator" should disappear, later we can tie ths with undo stack nicer */
	WM_operator_stack_clear(C);
	return ed_undo_step(C, 1, NULL);
}

static int ed_redo_exec(bContext *C, wmOperator *op)
{
	return ed_undo_step(C, -1, NULL);
}

void ED_undo_menu(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	Object *obact= CTX_data_active_object(C);
	
	if(obedit) {
		//if ELEM7(obedit->type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE)
		//	undo_editmode_menu();
	}
	else {
		if(obact && obact->mode & OB_MODE_PARTICLE_EDIT)
			PE_undo_menu(CTX_data_scene(C), CTX_data_active_object(C));
		else if(U.uiflag & USER_GLOBALUNDO) {
			char *menu= BKE_undo_menu_string();
			if(menu) {
				short event= 0; // XXX pupmenu_col(menu, 20);
				MEM_freeN(menu);
				if(event>0) {
					BKE_undo_number(C, event);
				}
			}
		}
	}
}

/* ********************** */

void ED_OT_undo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Undo";
    ot->description= "Undo previous action";
	ot->idname= "ED_OT_undo";
	
	/* api callbacks */
	ot->exec= ed_undo_exec;
	ot->poll= ED_operator_screenactive;
}

void ED_OT_redo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Redo";
    ot->description= "Redo previous action";
	ot->idname= "ED_OT_redo";
	
	/* api callbacks */
	ot->exec= ed_redo_exec;
	ot->poll= ED_operator_screenactive;
}


