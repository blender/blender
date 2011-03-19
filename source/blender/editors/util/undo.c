/*
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/editors/util/undo.c
 *  \ingroup edutil
 */



#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

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

#include "RNA_access.h"
#include "RNA_define.h"

#include "util_intern.h"

#define MAXUNDONAME 64 /* XXX, make common define */

/* ***************** generic undo system ********************* */

void ED_undo_push(bContext *C, const char *str)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	Object *obedit= CTX_data_edit_object(C);
	Object *obact= CTX_data_active_object(C);

	if (G.f & G_DEBUG)
		printf("undo push %s\n", str);
	
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
			if(!ED_undo_paint_step(C, UNDO_PAINT_IMAGE, step, undoname) && undoname)
				if(U.uiflag & USER_GLOBALUNDO)
					BKE_undo_name(C, undoname);

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

			WM_event_add_notifier(C, NC_GEOM|ND_DATA, NULL);
		}
	}
	else {
		int do_glob_undo= 0;
		
		if(obact && obact->mode & OB_MODE_TEXTURE_PAINT) {
			if(!ED_undo_paint_step(C, UNDO_PAINT_IMAGE, step, undoname) && undoname)
				do_glob_undo= 1;
		}
		else if(obact && obact->mode & OB_MODE_SCULPT) {
			if(!ED_undo_paint_step(C, UNDO_PAINT_MESH, step, undoname) && undoname)
				do_glob_undo= 1;
		}
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
				// note python defines not valid here anymore.
				//#ifdef WITH_PYTHON
				// XXX		BPY_scripts_clear_pyobjects();
				//#endif
				if(undoname)
					BKE_undo_name(C, undoname);
				else
					BKE_undo_step(C, step);

				WM_event_add_notifier(C, NC_SCENE|ND_LAYER_CONTENT, CTX_data_scene(C));
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

/* name optionally, function used to check for operator redo panel */
int ED_undo_valid(const bContext *C, const char *undoname)
{
	Object *obedit= CTX_data_edit_object(C);
	Object *obact= CTX_data_active_object(C);
	ScrArea *sa= CTX_wm_area(C);
	
	if(sa && sa->spacetype==SPACE_IMAGE) {
		SpaceImage *sima= (SpaceImage *)sa->spacedata.first;
		
		if((obact && obact->mode & OB_MODE_TEXTURE_PAINT) || sima->flag & SI_DRAWTOOL) {
			return 1;
		}
	}
	
	if(sa && sa->spacetype==SPACE_TEXT) {
		return 1;
	}
	else if(obedit) {
		if ELEM7(obedit->type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE) {
			return undo_editmode_valid(undoname);
		}
	}
	else {
		
		/* if below tests fail, global undo gets executed */
		
		if(obact && obact->mode & OB_MODE_TEXTURE_PAINT) {
			if( ED_undo_paint_valid(UNDO_PAINT_IMAGE, undoname) )
				return 1;
		}
		else if(obact && obact->mode & OB_MODE_SCULPT) {
			if( ED_undo_paint_valid(UNDO_PAINT_MESH, undoname) )
				return 1;
		}
		else if(obact && obact->mode & OB_MODE_PARTICLE_EDIT) {
			return PE_undo_valid(CTX_data_scene(C));
		}
		
		if(U.uiflag & USER_GLOBALUNDO) {
			return BKE_undo_valid(undoname);
		}
	}
	return 0;
}

static int ed_undo_exec(bContext *C, wmOperator *UNUSED(op))
{
	/* "last operator" should disappear, later we can tie ths with undo stack nicer */
	WM_operator_stack_clear(C);
	return ed_undo_step(C, 1, NULL);
}

static int ed_undo_push_exec(bContext *C, wmOperator *op)
{
	char str[MAXUNDONAME];
	RNA_string_get(op->ptr, "message", str);
	ED_undo_push(C, str);
	return OPERATOR_FINISHED;
}

static int ed_redo_exec(bContext *C, wmOperator *UNUSED(op))
{
	return ed_undo_step(C, -1, NULL);
}

#if 0 /* UNUSED */
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
#endif

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

void ED_OT_undo_push(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Undo Push";
	ot->description= "Add an undo state (internal use only)";
	ot->idname= "ED_OT_undo_push";
	
	/* api callbacks */
	ot->exec= ed_undo_push_exec;

	RNA_def_string(ot->srna, "message", "Add an undo step *function may be moved*", MAXUNDONAME, "Undo Message", "");
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


/* ui callbacks should call this rather then calling WM_operator_repeat() themselves */
int ED_undo_operator_repeat(bContext *C, struct wmOperator *op)
{
	int ret= 0;

	if(op) {
		ARegion *ar= CTX_wm_region(C);
		ARegion *ar1= BKE_area_find_region_type(CTX_wm_area(C), RGN_TYPE_WINDOW);

		if(ar1)
			CTX_wm_region_set(C, ar1);

		if(WM_operator_repeat_check(C, op) && WM_operator_poll(C, op->type)) {
			int retval;

			if (G.f & G_DEBUG)
				printf("redo_cb: operator redo %s\n", op->type->name);
			ED_undo_pop_op(C, op);

			if(op->type->check) {
				op->type->check(C, op); /* ignore return value since its running again anyway */
			}

			retval= WM_operator_repeat(C, op);
			if((retval & OPERATOR_FINISHED)==0) {
				if (G.f & G_DEBUG)
					printf("redo_cb: operator redo failed: %s, return %d\n", op->type->name, retval);
				ED_undo_redo(C);
			}
			else {
				ret= 1;
			}
		}

		/* set region back */
		CTX_wm_region_set(C, ar);
	}
	else {
		if (G.f & G_DEBUG) {
			printf("redo_cb: WM_operator_repeat_check returned false %s\n", op->type->name);
		}
	}

	return ret;
}

void ED_undo_operator_repeat_cb(bContext *C, void *arg_op, void *UNUSED(arg_unused))
{
	ED_undo_operator_repeat(C, (wmOperator *)arg_op);
}

void ED_undo_operator_repeat_cb_evt(bContext *C, void *arg_op, int UNUSED(arg_event))
{
	ED_undo_operator_repeat(C, (wmOperator *)arg_op);
}
