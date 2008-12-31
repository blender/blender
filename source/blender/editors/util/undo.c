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

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_dynstr.h"

#include "BKE_utildefines.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

/* ***************** generic undo system ********************* */

/* ********* XXX **************** */
static void undo_push_curve() {}
static void undo_push_font() {}
static void undo_push_mball() {}
static void undo_push_lattice() {}
static void undo_push_armature() {}
static void PE_undo_push() {}
static void PE_undo() {}
static void PE_redo() {}
static void PE_undo_menu() {}
static void undo_imagepaint_step() {}
static void sound_initialize_sounds() {}
/* ********* XXX **************** */

void ED_undo_push(bContext *C, char *str)
{
	if(G.obedit) {
		if (U.undosteps == 0) return;
		
		if(G.obedit->type==OB_MESH)
			undo_push_mesh(str);
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF)
			undo_push_curve(str);
		else if (G.obedit->type==OB_FONT)
			undo_push_font(str);
		else if (G.obedit->type==OB_MBALL)
			undo_push_mball(str);
		else if (G.obedit->type==OB_LATTICE)
			undo_push_lattice(str);
		else if (G.obedit->type==OB_ARMATURE)
			undo_push_armature(str);
	}
	else if(G.f & G_PARTICLEEDIT) {
		if (U.undosteps == 0) return;
		
		PE_undo_push(str);
	}
	else {
		if(U.uiflag & USER_GLOBALUNDO) 
			BKE_write_undo(C, str);
	}
}

static void undo_do(bContext *C)
{
	if(U.uiflag & USER_GLOBALUNDO) {
#ifndef DISABLE_PYTHON
// XXX		BPY_scripts_clear_pyobjects();
#endif
		BKE_undo_step(C, 1);
		sound_initialize_sounds();
	}
	
}

static int ed_undo_exec(bContext *C, wmOperator *op)
{	
	ScrArea *sa= CTX_wm_area(C);
	
	if(G.obedit) {
		if ELEM7(G.obedit->type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE)
			undo_editmode_step(1);
	}
	else {
		if(G.f & G_TEXTUREPAINT)
			undo_imagepaint_step(1);
		else if(sa->spacetype==SPACE_IMAGE) {
			SpaceImage *sima= (SpaceImage *)sa->spacedata.first;
			if(sima->flag & SI_DRAWTOOL)
				undo_imagepaint_step(1);
			else
				undo_do(C);
		}
		else if(G.f & G_PARTICLEEDIT)
			PE_undo();
		else {
			undo_do(C);
		}
	}
	
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	
	return OPERATOR_FINISHED;
}

static int ed_redo_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	
	if(G.obedit) {
		//if ELEM7(G.obedit->type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE)
		//	undo_editmode_step(-1);
	}
	else {
		if(G.f & G_TEXTUREPAINT)
			undo_imagepaint_step(-1);
		else if(sa->spacetype==SPACE_IMAGE) {
			SpaceImage *sima= (SpaceImage *)sa->spacedata.first;
			if(sima->flag & SI_DRAWTOOL)
				undo_imagepaint_step(-1);
			else {
				BKE_undo_step(C, -1);
				sound_initialize_sounds();
			}
		}
		else if(G.f & G_PARTICLEEDIT)
			PE_redo();
		else {
			/* includes faceselect now */
			if(U.uiflag & USER_GLOBALUNDO) {
				BKE_undo_step(C, -1);
				sound_initialize_sounds();
			}
		}
	}
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	return OPERATOR_FINISHED;

}

void ED_undo_menu(bContext *C)
{
	if(G.obedit) {
		//if ELEM7(G.obedit->type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE)
		//	undo_editmode_menu();
	}
	else {
		if(G.f & G_PARTICLEEDIT)
			PE_undo_menu();
		else if(U.uiflag & USER_GLOBALUNDO) {
			char *menu= BKE_undo_menu_string();
			if(menu) {
				short event= 0; // XXX pupmenu_col(menu, 20);
				MEM_freeN(menu);
				if(event>0) {
					BKE_undo_number(C, event);
					sound_initialize_sounds();
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
	ot->idname= "ED_OT_undo";
	
	/* api callbacks */
	ot->exec= ed_undo_exec;
	ot->poll= ED_operator_screenactive;
}

void ED_OT_redo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Redo";
	ot->idname= "ED_OT_redo";
	
	/* api callbacks */
	ot->exec= ed_redo_exec;
	ot->poll= ED_operator_screenactive;
}


