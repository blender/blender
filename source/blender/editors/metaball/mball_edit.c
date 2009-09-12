/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "BKE_utildefines.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_context.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_transform.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

/* This function is used to free all MetaElems from MetaBall */
void free_editMball(Object *obedit)
{
}

/* This function is called, when MetaBall Object is
 * switched from object mode to edit mode */
void make_editMball(Object *obedit)
{
	MetaBall *mb = (MetaBall*)obedit->data;
	MetaElem *ml;/*, *newml;*/

	ml= mb->elems.first;
	
	while(ml) {
		if(ml->flag & SELECT) mb->lastelem = ml;
		ml= ml->next;
	}

	mb->editelems = &mb->elems;
}

/* This function is called, when MetaBall Object switched from
 * edit mode to object mode. List od MetaElements is copied
 * from object->data->edit_elems to to object->data->elems. */
void load_editMball(Object *obedit)
{
	MetaBall *mb = (MetaBall*)obedit->data;
	
	mb->editelems= NULL;
	mb->lastelem= NULL;
}

/* Add metaelem primitive to metaball object (which is in edit mode) */
MetaElem *add_metaball_primitive(bContext *C, int type, int newname)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	Object *obedit= CTX_data_edit_object(C);
	MetaBall *mball = (MetaBall*)obedit->data;
	MetaElem *ml;
	float *curs, mat[3][3], cent[3], imat[3][3], cmat[3][3];

	if(!obedit) return NULL;

	/* Deselect all existing metaelems */
	ml= mball->editelems->first;
	while(ml) {
		ml->flag &= ~SELECT;
		ml= ml->next;
	}

	Mat3CpyMat4(mat, obedit->obmat);
	if(v3d) {
		curs= give_cursor(scene, v3d);
		VECCOPY(cent, curs);
	}
	else
		cent[0]= cent[1]= cent[2]= 0.0f;
	
	cent[0]-= obedit->obmat[3][0];
	cent[1]-= obedit->obmat[3][1];
	cent[2]-= obedit->obmat[3][2];

	if (rv3d) {
		if (!(newname) || U.flag & USER_ADD_VIEWALIGNED || !rv3d)
			Mat3CpyMat4(imat, rv3d->viewmat);
		else
			Mat3One(imat);
		Mat3MulVecfl(imat, cent);
		Mat3MulMat3(cmat, imat, mat);
		Mat3Inv(imat,cmat);
		Mat3MulVecfl(imat, cent);
	}
	else
		Mat3One(imat);

	ml= MEM_callocN(sizeof(MetaElem), "metaelem");

	ml->x= cent[0];
	ml->y= cent[1];
	ml->z= cent[2];
	ml->quat[0]= 1.0;
	ml->quat[1]= 0.0;
	ml->quat[2]= 0.0;
	ml->quat[3]= 0.0;
	ml->rad= 2.0;
	ml->s= 2.0;
	ml->flag= SELECT | MB_SCALE_RAD;

	switch(type) {
	case MB_BALL:
		ml->type = MB_BALL;
		ml->expx= ml->expy= ml->expz= 1.0;
		break;
	case MB_TUBE:
		ml->type = MB_TUBE;
		ml->expx= ml->expy= ml->expz= 1.0;
		break;
	case MB_PLANE:
		ml->type = MB_PLANE;
		ml->expx= ml->expy= ml->expz= 1.0;
		break;
	case MB_ELIPSOID:
		ml->type = MB_ELIPSOID;
		ml->expx= 1.2f;
		ml->expy= 0.8f;
		ml->expz= 1.0;
		break;
	case MB_CUBE:
		ml->type = MB_CUBE;
		ml->expx= ml->expy= ml->expz= 1.0;
		break;
	default:
		break;
	}
	
	mball->lastelem= ml;
	
	return ml;
}

/***************************** Select/Deselect operator *****************************/

/* Select or deselect all MetaElements */
static int select_deselect_all_metaelems_exec(bContext *C, wmOperator *op)
{
	//Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall*)obedit->data;
	MetaElem *ml;
	int any_sel= 0;
	
	/* Is any metaelem selected? */
	ml= mb->editelems->first;
	if(ml) {
		while(ml) {
			if(ml->flag & SELECT) break;
			ml= ml->next;
		}
		if(ml) any_sel= 1;

		ml= mb->editelems->first;
		while(ml) {
			if(any_sel) ml->flag &= ~SELECT;
			else ml->flag |= SELECT;
			ml= ml->next;
		}
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, mb);
		//DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	}

	return OPERATOR_FINISHED;
}

void MBALL_OT_select_deselect_all_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select/Deselect All";
    ot->description= "(de)select all metaelements.";
	ot->idname= "MBALL_OT_select_deselect_all_metaelems";

	/* callback functions */
	ot->exec= select_deselect_all_metaelems_exec;
	ot->poll= ED_operator_editmball;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;	
}

/***************************** Select inverse operator *****************************/

/* Invert metaball selection */
static int select_inverse_metaelems_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall*)obedit->data;
	MetaElem *ml;
	
	ml= mb->editelems->first;
	if(ml) {
		while(ml) {
			if(ml->flag & SELECT)
				ml->flag &= ~SELECT;
			else
				ml->flag |= SELECT;
			ml= ml->next;
		}
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, mb);
	}
	
	return OPERATOR_FINISHED;
}

void MBALL_OT_select_inverse_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Inverse";
    ot->description= "Select inverse of (un)selected metaelements.";
	ot->idname= "MBALL_OT_select_inverse_metaelems";

	/* callback functions */
	ot->exec= select_inverse_metaelems_exec;
	ot->poll= ED_operator_editmball;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;	
}

/***************************** Select random operator *****************************/

/* Random metaball selection */
static int select_random_metaelems_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall*)obedit->data;
	MetaElem *ml;
	float percent= RNA_float_get(op->ptr, "percent");
	
	if(percent == 0.0)
		return OPERATOR_CANCELLED;
	
	ml= mb->editelems->first;
	BLI_srand( BLI_rand() );	/* Random seed */
	
	/* Stupid version of random selection. Should be improved. */
	while(ml) {
		if(BLI_frand() < percent)
			ml->flag |= SELECT;
		else
			ml->flag &= ~SELECT;
		ml= ml->next;
	}
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, mb);
	
	return OPERATOR_FINISHED;
}


void MBALL_OT_select_random_metaelems(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Random...";
    ot->description= "Randomly select metaelements.";
	ot->idname= "MBALL_OT_select_random_metaelems";
	
	/* callback functions */
	ot->exec= select_random_metaelems_exec;
	ot->invoke= WM_operator_props_popup;
	ot->poll= ED_operator_editmball;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_float_percentage(ot->srna, "percent", 0.5f, 0.0f, 1.0f, "Percent", "Percentage of metaelems to select randomly.", 0.0001f, 1.0f);
}

/***************************** Duplicate operator *****************************/

/* Duplicate selected MetaElements */
static int duplicate_metaelems_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall*)obedit->data;
	MetaElem *ml, *newml;
	
	ml= mb->editelems->last;
	if(ml) {
		while(ml) {
			if(ml->flag & SELECT) {
				newml= MEM_dupallocN(ml);
				BLI_addtail(mb->editelems, newml);
				mb->lastelem= newml;
				ml->flag &= ~SELECT;
			}
			ml= ml->prev;
		}
		WM_event_add_notifier(C, NC_GEOM|ND_DATA, mb);
		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	}

	return OPERATOR_FINISHED;
}

static int duplicate_metaelems_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	int retv= duplicate_metaelems_exec(C, op);
	
	if (retv == OPERATOR_FINISHED) {
		RNA_int_set(op->ptr, "mode", TFM_TRANSLATION);
		WM_operator_name_call(C, "TFM_OT_transform", WM_OP_INVOKE_REGION_WIN, op->ptr);
	}
	
	return retv;
}


void MBALL_OT_duplicate_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate";
    ot->description= "Delete selected metaelement(s).";
	ot->idname= "MBALL_OT_duplicate_metaelems";

	/* callback functions */
	ot->exec= duplicate_metaelems_exec;
	ot->invoke= duplicate_metaelems_invoke;
	ot->poll= ED_operator_editmball;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* to give to transform */
	RNA_def_int(ot->srna, "mode", TFM_TRANSLATION, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

/***************************** Delete operator *****************************/

/* Delete all selected MetaElems (not MetaBall) */
static int delete_metaelems_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	MetaBall *mb= (MetaBall*)obedit->data;
	MetaElem *ml, *next;
	
	ml= mb->editelems->first;
	if(ml) {
		while(ml) {
			next= ml->next;
			if(ml->flag & SELECT) {
				if(mb->lastelem==ml) mb->lastelem= NULL;
				BLI_remlink(mb->editelems, ml);
				MEM_freeN(ml);
			}
			ml= next;
		}
		WM_event_add_notifier(C, NC_GEOM|ND_DATA, mb);
		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	}

	return OPERATOR_FINISHED;
}

void MBALL_OT_delete_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete";
    ot->description= "Delete selected metaelement(s).";
	ot->idname= "MBALL_OT_delete_metaelems";

	/* callback functions */
	ot->exec= delete_metaelems_exec;
	ot->poll= ED_operator_editmball;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;	
}

/***************************** Hide operator *****************************/

/* Hide selected MetaElems */
static int hide_metaelems_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	MetaBall *mb= (MetaBall*)obedit->data;
	MetaElem *ml;
	int hide_unselected= RNA_boolean_get(op->ptr, "unselected");

	ml= mb->editelems->first;

	if(ml) {
		/* Hide unselected metaelems */
		if(hide_unselected) {
			while(ml){
				if(!(ml->flag & SELECT))
					ml->flag |= MB_HIDE;
				ml= ml->next;
			}
		/* Hide selected metaelems */	
		} else {
			while(ml){
				if(ml->flag & SELECT)
					ml->flag |= MB_HIDE;
				ml= ml->next;
			}
		}
		WM_event_add_notifier(C, NC_GEOM|ND_DATA, mb);
		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	}

	return OPERATOR_FINISHED;
}

void MBALL_OT_hide_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide";
    ot->description= "Hide (un)selected metaelement(s).";
	ot->idname= "MBALL_OT_hide_metaelems";

	/* callback functions */
	ot->exec= hide_metaelems_exec;
	ot->poll= ED_operator_editmball;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected.");
}

/***************************** Unhide operator *****************************/

/* Unhide all edited MetaElems */
static int reveal_metaelems_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	MetaBall *mb= (MetaBall*)obedit->data;
	MetaElem *ml;

	ml= mb->editelems->first;

	if(ml) {
		while(ml) {
			ml->flag &= ~MB_HIDE;
			ml= ml->next;
		}
		WM_event_add_notifier(C, NC_GEOM|ND_DATA, mb);
		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	}
	
	return OPERATOR_FINISHED;
}

void MBALL_OT_reveal_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reveal";
    ot->description= "Reveal all hidden metaelements.";
	ot->idname= "MBALL_OT_reveal_metaelems";
	
	/* callback functions */
	ot->exec= reveal_metaelems_exec;
	ot->poll= ED_operator_editmball;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;	
}

/* Select MetaElement with mouse click (user can select radius circle or
 * stiffness circle) */
void mouse_mball(bContext *C, short mval[2], int extend)
{
	static MetaElem *startelem=NULL;
	Object *obedit= CTX_data_edit_object(C);
	ViewContext vc;
	MetaBall *mb = (MetaBall*)obedit->data;
	MetaElem *ml, *act=NULL;
	int a, hits;
	unsigned int buffer[4*MAXPICKBUF];
	rcti rect;

	view3d_set_viewcontext(C, &vc);

	rect.xmin= mval[0]-12;
	rect.xmax= mval[0]+12;
	rect.ymin= mval[1]-12;
	rect.ymax= mval[1]+12;

	hits= view3d_opengl_select(&vc, buffer, MAXPICKBUF, &rect);

	/* does startelem exist? */
	ml= mb->editelems->first;
	while(ml) {
		if(ml==startelem) break;
		ml= ml->next;
	}

	if(ml==NULL) startelem= mb->editelems->first;
	
	if(hits>0) {
		ml= startelem;
		while(ml) {
			for(a=0; a<hits; a++) {
				/* index converted for gl stuff */
				if(ml->selcol1==buffer[ 4 * a + 3 ]){
					ml->flag |= MB_SCALE_RAD;
					act= ml;
				}
				if(ml->selcol2==buffer[ 4 * a + 3 ]){
					ml->flag &= ~MB_SCALE_RAD;
					act= ml;
				}
			}
			if(act) break;
			ml= ml->next;
			if(ml==NULL) ml= mb->editelems->first;
			if(ml==startelem) break;
		}
		
		/* When some metaelem was found, then it is neccessary to select or
		 * deselet it. */
		if(act) {
			if(extend==0) {
				/* Deselect all existing metaelems */
				ml= mb->editelems->first;
				while(ml) {
					ml->flag &= ~SELECT;
					ml= ml->next;
				}
				/* Select only metaelem clicked on */
				act->flag |= SELECT;
			}
			else {
				if(act->flag & SELECT)
					act->flag &= ~SELECT;
				else
					act->flag |= SELECT;
			}
			mb->lastelem= act;
			
			WM_event_add_notifier(C, NC_GEOM|ND_SELECT, mb);
		}
	}
}


/*  ************* undo for MetaBalls ************* */

/* free all MetaElems from ListBase */
static void freeMetaElemlist(ListBase *lb)
{
	MetaElem *ml, *next;

	if(lb==NULL) return;

	ml= lb->first;
	while(ml){
		next= ml->next;
		BLI_remlink(lb, ml);
		MEM_freeN(ml);
		ml= next;
	}

	lb->first= lb->last= NULL;
}


static void undoMball_to_editMball(void *lbu, void *lbe)
{
	ListBase *lb= lbu;
	ListBase *editelems= lbe;
	MetaElem *ml, *newml;
	
	freeMetaElemlist(editelems);

	/* copy 'undo' MetaElems to 'edit' MetaElems */
	ml= lb->first;
	while(ml){
		newml= MEM_dupallocN(ml);
		BLI_addtail(editelems, newml);
		ml= ml->next;
	}
	
}

static void *editMball_to_undoMball(void *lbe)
{
	ListBase *editelems= lbe;
	ListBase *lb;
	MetaElem *ml, *newml;

	/* allocate memory for undo ListBase */
	lb= MEM_callocN(sizeof(ListBase), "listbase undo");
	lb->first= lb->last= NULL;
	
	/* copy contents of current ListBase to the undo ListBase */
	ml= editelems->first;
	while(ml){
		newml= MEM_dupallocN(ml);
		BLI_addtail(lb, newml);
		ml= ml->next;
	}
	
	return lb;
}

/* free undo ListBase of MetaElems */
static void free_undoMball(void *lbv)
{
	ListBase *lb= lbv;
	
	freeMetaElemlist(lb);
	MEM_freeN(lb);
}

ListBase *metaball_get_editelems(Object *ob)
{
	if(ob && ob->type==OB_MBALL) {
		struct MetaBall *mb= (struct MetaBall*)ob->data;
		return mb->editelems;
	}
	return NULL;
}


static void *get_data(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	return metaball_get_editelems(obedit);
}

/* this is undo system for MetaBalls */
void undo_push_mball(bContext *C, char *name)
{
	undo_editmode_push(C, name, get_data, free_undoMball, undoMball_to_editMball, editMball_to_undoMball, NULL);
}

