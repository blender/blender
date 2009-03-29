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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"
#include "UI_resources.h"
#include "UI_text.h"

#include "ED_markers.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

/* ************* Marker API **************** */

static ListBase *context_get_markers(const bContext *C)
{
	
#if 0
	/* XXX get them from pose */
	if ((slink->spacetype == SPACE_ACTION) && (saction->flag & SACTION_POSEMARKERS_MOVE)) {
		if (saction->action)
			markers= &saction->action->markers;
		else
			markers= NULL;
	}
	else
#endif
	
	return &CTX_data_scene(C)->markers;
}

/* ************* Marker Drawing ************ */

/* XXX */
extern void ui_rasterpos_safe(float x, float y, float aspect);

/* function to draw markers */
static void draw_marker(View2D *v2d, TimeMarker *marker, int cfra, int flag)
{
	float xpos, ypixels, xscale, yscale;
	int icon_id= 0;
	
	xpos = marker->frame;
	
	/* no time correction for framelen! space is drawn with old values */
	ypixels= v2d->mask.ymax-v2d->mask.ymin;
	UI_view2d_getscale(v2d, &xscale, &yscale);
	
	glScalef(1.0/xscale, 1.0, 1.0);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);			
	
	/* vertical line - dotted */
	if (flag & DRAW_MARKERS_LINES) {
		setlinestyle(3);
		
		if (marker->flag & SELECT)
			glColor4ub(255, 255, 255, 96);
		else
			glColor4ub(0, 0, 0, 96);
		
		glBegin(GL_LINES);
			glVertex2f((xpos*xscale)+0.5, 12);
			glVertex2f((xpos*xscale)+0.5, 34*yscale); /* a bit lazy but we know it cant be greater then 34 strips high*/
		glEnd();
		
		setlinestyle(0);
	}
	
	/* 5 px to offset icon to align properly, space / pixels corrects for zoom */
	if (flag & DRAW_MARKERS_LOCAL) {
		icon_id= (marker->flag & ACTIVE) ? ICON_PMARKER_ACT : 
		(marker->flag & SELECT) ? ICON_PMARKER_SEL : 
		ICON_PMARKER;
	}
	else {
		icon_id= (marker->flag & SELECT) ? ICON_MARKER_HLT : 
		ICON_MARKER;
	}
	
	UI_icon_draw(xpos*xscale-5.0, 16.0, icon_id);
	
	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);
	
	/* and the marker name too, shifted slightly to the top-right */
	if (marker->name && marker->name[0]) {
		if(marker->flag & SELECT) {
			UI_ThemeColor(TH_TEXT_HI);
			ui_rasterpos_safe(xpos*xscale+4.0, (ypixels<=39.0)?(ypixels-10.0):29.0, 1.0);
		}
		else {
			UI_ThemeColor(TH_TEXT);
			if((marker->frame <= cfra) && (marker->frame+5 > cfra))
				ui_rasterpos_safe(xpos*xscale+4.0, (ypixels<=39.0)?(ypixels-10.0):29.0, 1.0);
			else
				ui_rasterpos_safe(xpos*xscale+4.0, 17.0, 1.0);
		}
		UI_DrawString(G.font, marker->name, 0);
	}
	glScalef(xscale, 1.0, 1.0);
}

/* Draw Scene-Markers in time window */
void draw_markers_time(const bContext *C, int flag)
{
	ListBase *markers= context_get_markers(C);
	View2D *v2d= UI_view2d_fromcontext(C);
	TimeMarker *marker;
	
	if(markers == NULL)
		return;
	
	/* unselected markers are drawn at the first time */
	for (marker= markers->first; marker; marker= marker->next) {
		if (!(marker->flag & SELECT)) draw_marker(v2d, marker, CTX_data_scene(C)->r.cfra, flag);
	}
	
	/* selected markers are drawn later */
	for (marker= markers->first; marker; marker= marker->next) {
		if (marker->flag & SELECT) draw_marker(v2d, marker, CTX_data_scene(C)->r.cfra, flag);
	}
}



/* ************************** add markers *************************** */

/* add TimeMarker at curent frame */
static int ed_marker_add(bContext *C, wmOperator *op)
{
	ListBase *markers= context_get_markers(C);
	TimeMarker *marker;
	int frame= CTX_data_scene(C)->r.cfra;
	
	if(markers == NULL)
		return OPERATOR_CANCELLED;
	
	/* two markers can't be at the same place */
	for(marker= markers->first; marker; marker= marker->next)
		if(marker->frame == frame) 
			return OPERATOR_CANCELLED;
	
	/* deselect all */
	for(marker= markers->first; marker; marker= marker->next)
		marker->flag &= ~SELECT;
	
	marker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
	marker->flag= SELECT;
	marker->frame= frame;
	sprintf(marker->name, "Frame %d", frame); // XXX - temp code only
	BLI_addtail(markers, marker);
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	
	return OPERATOR_FINISHED;
}

static void MARKER_OT_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Time Marker";
	ot->idname= "MARKER_OT_add";
	
	/* api callbacks */
	ot->exec= ed_marker_add;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************** transform markers *************************** */


/* operator state vars used:  
	frs: delta movement

functions:

	init()   check selection, add customdata with old values and some lookups

	apply()  do the actual movement

	exit()	cleanup, send notifier

    cancel() to escpae from modal

callbacks:

	exec()	calls init, apply, exit 

	invoke() calls init, adds modal handler

	modal()	accept modal events while doing it, ends with apply and exit, or cancel

*/

typedef struct MarkerMove {
	SpaceLink *slink;
	ListBase *markers;
	int event_type;		/* store invoke-event, to verify */
	int *oldframe, evtx, firstx;
} MarkerMove;

/* copy selection to temp buffer */
/* return 0 if not OK */
static int ed_marker_move_init(bContext *C, wmOperator *op)
{
	ListBase *markers= context_get_markers(C);
	MarkerMove *mm;
	TimeMarker *marker;
	int totmark=0;
	int a;

	if(markers == NULL) return 0;
	
	for (marker= markers->first; marker; marker= marker->next)
		if (marker->flag & SELECT) totmark++;
	
	if (totmark==0) return 0;
	
	op->customdata= mm= MEM_callocN(sizeof(MarkerMove), "Markermove");
	mm->slink= CTX_wm_space_data(C);
	mm->markers= markers;
	mm->oldframe= MEM_callocN(totmark*sizeof(int), "MarkerMove oldframe");
	
	for (a=0, marker= markers->first; marker; marker= marker->next) {
		if (marker->flag & SELECT) {
			mm->oldframe[a]= marker->frame;
			a++;
		}
	}
	
	return 1;
}

/* free stuff */
static void ed_marker_move_exit(bContext *C, wmOperator *op)
{
	MarkerMove *mm= op->customdata;
	
	MEM_freeN(mm->oldframe);
	MEM_freeN(op->customdata);
	op->customdata= NULL;
}

static int ed_marker_move_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	if(ed_marker_move_init(C, op)) {
		MarkerMove *mm= op->customdata;
		
		mm->evtx= evt->x;
		mm->firstx= evt->x;
		mm->event_type= evt->type;
		
		/* add temp handler */
		WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
		
		/* reset frs delta */
		RNA_int_set(op->ptr, "frames", 0);
		
		return OPERATOR_RUNNING_MODAL;
	}
	
	return OPERATOR_CANCELLED;
}

/* note, init has to be called succesfully */
static void ed_marker_move_apply(bContext *C, wmOperator *op)
{
	MarkerMove *mm= op->customdata;
	TimeMarker *marker;
	int a, offs;
	
	offs= RNA_int_get(op->ptr, "frames");
	for (a=0, marker= mm->markers->first; marker; marker= marker->next) {
		if (marker->flag & SELECT) {
			marker->frame= mm->oldframe[a] + offs;
			a++;
		}
	}
}

/* only for modal */
static void ed_marker_move_cancel(bContext *C, wmOperator *op)
{
	RNA_int_set(op->ptr, "frames", 0);
	ed_marker_move_apply(C, op);
	ed_marker_move_exit(C, op);	
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
}



static int ed_marker_move_modal(bContext *C, wmOperator *op, wmEvent *evt)
{
	Scene *scene= CTX_data_scene(C);
	MarkerMove *mm= op->customdata;
	View2D *v2d= UI_view2d_fromcontext(C);
	TimeMarker *marker, *selmarker=NULL;
	float dx, fac;
	char str[256];
		
	switch(evt->type) {
		case ESCKEY:
			ed_marker_move_cancel(C, op);
			return OPERATOR_CANCELLED;

		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			if(WM_modal_tweak_exit(evt, mm->event_type)) {
				ed_marker_move_exit(C, op);
				WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
				return OPERATOR_FINISHED;
			}
			
			break;
		case MOUSEMOVE:
	
			dx= v2d->mask.xmax-v2d->mask.xmin;
			dx= (v2d->cur.xmax-v2d->cur.xmin)/dx;
			
			if (evt->x != mm->evtx) {	/* XXX maybe init for firsttime */
				int a, offs, totmark=0;
				
				mm->evtx= evt->x;
				
				fac= ((float)(evt->x - mm->firstx)*dx);
				
				if (ELEM(mm->slink->spacetype, SPACE_TIME, SPACE_SOUND)) 
					apply_keyb_grid(evt->shift, evt->ctrl, &fac, 0.0, FPS, 0.1*FPS, 0);
				else
					apply_keyb_grid(evt->shift, evt->ctrl, &fac, 0.0, 1.0, 0.1, U.flag & USER_AUTOGRABGRID);
				
				offs= (int)fac;
				RNA_int_set(op->ptr, "frames", offs);
				ed_marker_move_apply(C, op);
				
				/* cruft below is for header print */
				for (a=0, marker= mm->markers->first; marker; marker= marker->next) {
					if (marker->flag & SELECT) {
						selmarker= marker;
						a++; totmark++;
					}
				}
				
				if (totmark==1) {	
					/* we print current marker value */
					if (ELEM(mm->slink->spacetype, SPACE_TIME, SPACE_SOUND)) {
						SpaceTime *stime= (SpaceTime *)mm->slink;
						if (stime->flag & TIME_DRAWFRAMES) 
							sprintf(str, "Marker %d offset %d", selmarker->frame, offs);
						else 
							sprintf(str, "Marker %.2f offset %.2f", FRA2TIME(selmarker->frame), FRA2TIME(offs));
					}
					else if (mm->slink->spacetype == SPACE_ACTION) {
#if 0						
XXX						if (saction->flag & SACTION_DRAWTIME)
							sprintf(str, "Marker %.2f offset %.2f", FRA2TIME(selmarker->frame), FRA2TIME(offs));
						else
							sprintf(str, "Marker %.2f offset %.2f", (double)(selmarker->frame), (double)(offs));
#endif					
					}
					else {
						sprintf(str, "Marker %.2f offset %.2f", (double)(selmarker->frame), (double)(offs));
					}
				}
				else {
					/* we only print the offset */
					if (ELEM(mm->slink->spacetype, SPACE_TIME, SPACE_SOUND)) { 
						SpaceTime *stime= (SpaceTime *)mm->slink;
						if (stime->flag & TIME_DRAWFRAMES) 
							sprintf(str, "Marker offset %d ", offs);
						else 
							sprintf(str, "Marker offset %.2f ", FRA2TIME(offs));
					}
#if 0					
XXX					else if (mm->slink->spacetype == SPACE_ACTION) {
						if (saction->flag & SACTION_DRAWTIME)
							sprintf(str, "Marker offset %.2f ", FRA2TIME(offs));
						else
							sprintf(str, "Marker offset %.2f ", (double)(offs));
					}
#endif					
					else {
						sprintf(str, "Marker offset %.2f ", (double)(offs));
					}
				}
				
				WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
				// headerprint(str); XXX
			}
	}

	return OPERATOR_RUNNING_MODAL;
}

static int ed_marker_move_exec(bContext *C, wmOperator *op)
{
	if(ed_marker_move_init(C, op)) {
		ed_marker_move_apply(C, op);
		ed_marker_move_exit(C, op);
		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

static void MARKER_OT_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Time Marker";
	ot->idname= "MARKER_OT_move";
	
	/* api callbacks */
	ot->exec= ed_marker_move_exec;
	ot->invoke= ed_marker_move_invoke;
	ot->modal= ed_marker_move_modal;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna storage */
	RNA_def_int(ot->srna, "frames", 0, INT_MIN, INT_MAX, "Frames", "", INT_MIN, INT_MAX);
}

/* ************************** duplicate markers *************************** */

/* operator state vars used:  
	frs: delta movement

functions:

	apply()  do the actual duplicate

callbacks:

	exec()	calls apply, move_exec

	invoke() calls apply, move_invoke

	modal()	uses move_modal

*/


/* duplicate selected TimeMarkers */
static void ed_marker_duplicate_apply(bContext *C, wmOperator *op)
{
	ListBase *markers= context_get_markers(C);
	TimeMarker *marker, *newmarker;
	
	if(markers == NULL) return;

	/* go through the list of markers, duplicate selected markers and add duplicated copies
	* to the begining of the list (unselect original markers) */
	for(marker= markers->first; marker; marker= marker->next) {
		if(marker->flag & SELECT){
			/* unselect selected marker */
			marker->flag &= ~SELECT;
			/* create and set up new marker */
			newmarker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
			newmarker->flag= SELECT;
			newmarker->frame= marker->frame;
			BLI_strncpy(newmarker->name, marker->name, sizeof(marker->name));
			/* new marker is added to the begining of list */
			BLI_addhead(markers, newmarker);
		}
	}
}

static int ed_marker_duplicate_exec(bContext *C, wmOperator *op)
{
	ed_marker_duplicate_apply(C, op);
	ed_marker_move_exec(C, op);	/* assumes frs delta set */
	
	return OPERATOR_FINISHED;
	
}

static int ed_marker_duplicate_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	ed_marker_duplicate_apply(C, op);
	return ed_marker_move_invoke(C, op, evt);
}

static void MARKER_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate Time Marker";
	ot->idname= "MARKER_OT_duplicate";
	
	/* api callbacks */
	ot->exec= ed_marker_duplicate_exec;
	ot->invoke= ed_marker_duplicate_invoke;
	ot->modal= ed_marker_move_modal;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna storage */
	RNA_def_int(ot->srna, "frames", 0, INT_MIN, INT_MAX, "Frames", "", INT_MIN, INT_MAX);
}

/* ************************** selection ************************************/

/* select/deselect TimeMarker at current frame */
static void select_timeline_marker_frame(ListBase *markers, int frame, unsigned char shift)
{
	TimeMarker *marker;
	int select=0;
	
	for(marker= markers->first; marker; marker= marker->next) {
		/* if Shift is not set, then deselect Markers */
		if(!shift) marker->flag &= ~SELECT;
		/* this way a not-shift select will allways give 1 selected marker */
		if((marker->frame == frame) && (!select)) {
			if(marker->flag & SELECT) 
				marker->flag &= ~SELECT;
			else
				marker->flag |= SELECT;
			select = 1;
		}
	}
}

int find_nearest_marker_time(ListBase *markers, float dx)
{
	TimeMarker *marker, *nearest= NULL;
	float dist, min_dist= 1000000;
	
	for(marker= markers->first; marker; marker= marker->next) {
		dist = ABS((float)marker->frame - dx);
		if(dist < min_dist){
			min_dist= dist;
			nearest= marker;
		}
	}
	
	if(nearest) return nearest->frame;
	else return (int)floor(dx); /* hrmf? */
}


static int ed_marker_select(bContext *C, wmEvent *evt, int extend)
{
	ListBase *markers= context_get_markers(C);
	View2D *v2d= UI_view2d_fromcontext(C);
	float viewx;
	int x, y, cfra;
	
	if(markers == NULL)
		return OPERATOR_PASS_THROUGH;

	x= evt->x - CTX_wm_region(C)->winrct.xmin;
	y= evt->y - CTX_wm_region(C)->winrct.ymin;
	
	UI_view2d_region_to_view(v2d, x, y, &viewx, NULL);	
	
	cfra= find_nearest_marker_time(markers, viewx);
	
	if (extend)
		select_timeline_marker_frame(markers, cfra, 1);
	else
		select_timeline_marker_frame(markers, cfra, 0);
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);

	/* allowing tweaks */
	return OPERATOR_PASS_THROUGH;
}

static int ed_marker_select_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	short extend= RNA_boolean_get(op->ptr, "extend");
	return ed_marker_select(C, evt, extend);
}

static void MARKER_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Time Marker";
	ot->idname= "MARKER_OT_select";
	
	/* api callbacks */
	ot->invoke= ed_marker_select_invoke;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "extend the selection");
}

/* *************************** border select markers **************** */

/* operator state vars used: (added by default WM callbacks)   
	xmin, ymin     
	xmax, ymax     

customdata: the wmGesture pointer, with subwindow

callbacks:

	exec()	has to be filled in by user

	invoke() default WM function
			adds modal handler

	modal()	default WM function 
			accept modal events while doing it, calls exec(), handles ESC and border drawing

	poll()	has to be filled in by user for context
*/

static int ed_marker_border_select_exec(bContext *C, wmOperator *op)
{
	View2D *v2d= UI_view2d_fromcontext(C);
	ListBase *markers= context_get_markers(C);
	TimeMarker *marker;
	float xminf, xmaxf, yminf, ymaxf;
	int event_type= RNA_int_get(op->ptr, "event_type");
	int xmin= RNA_int_get(op->ptr, "xmin");
	int xmax= RNA_int_get(op->ptr, "xmax");
	int ymin= RNA_int_get(op->ptr, "ymin");
	int ymax= RNA_int_get(op->ptr, "ymax");
	
	UI_view2d_region_to_view(v2d, xmin, ymin, &xminf, &yminf);	
	UI_view2d_region_to_view(v2d, xmax, ymax, &xmaxf, &ymaxf);	
	
	/* XXX disputable */
	if(yminf > 30.0f || ymaxf < 0.0f)
		return 0;
	
	if(markers == NULL)
		return 0;
	
	/* XXX marker context */
	for(marker= markers->first; marker; marker= marker->next) {
		if ((marker->frame > xminf) && (marker->frame <= xmaxf)) {
			switch (event_type) {
				case LEFTMOUSE:
					if ((marker->flag & SELECT) == 0) 
						marker->flag |= SELECT;
					break;
				case RIGHTMOUSE:
					if (marker->flag & SELECT) 
						marker->flag &= ~SELECT;
					break;
			}
		}
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);

	return 1;
}

static void MARKER_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Marker Border select";
	ot->idname= "MARKER_OT_select_border";
	
	/* api callbacks */
	ot->exec= ed_marker_border_select_exec;
	ot->invoke= WM_border_select_invoke;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);
}

/* *********************** (de)select all ***************** */

static int ed_marker_select_all_exec(bContext *C, wmOperator *op)
{
	ListBase *markers= context_get_markers(C);
	TimeMarker *marker;
	int select= RNA_int_get(op->ptr, "select_type");

	if(markers == NULL)
		return OPERATOR_CANCELLED;
	
	if(RNA_boolean_get(op->ptr, "select_swap")) {
		for(marker= markers->first; marker; marker= marker->next) {
			if(marker->flag & SELECT)
				break;
		}
		if(marker)
			select= 0;
		else
			select= 1;
	}
	
	for(marker= markers->first; marker; marker= marker->next) {
		if(select)
			marker->flag |= SELECT;
		else
			marker->flag &= ~SELECT;
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);

	return OPERATOR_FINISHED;
}

static int ed_marker_select_all_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	RNA_boolean_set(op->ptr, "select_swap", 1);
	
	return ed_marker_select_all_exec(C, op);
}

static void MARKER_OT_select_all_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "(De)select all markers";
	ot->idname= "MARKER_OT_select_all_toggle";
	
	/* api callbacks */
	ot->exec= ed_marker_select_all_exec;
	ot->invoke= ed_marker_select_all_invoke;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	RNA_def_boolean(ot->srna, "select_swap", 0, "Select Swap", "");
	RNA_def_int(ot->srna, "select_type", 0, INT_MIN, INT_MAX, "Select Type", "", INT_MIN, INT_MAX);
}

/* ******************************* remove marker ***************** */

/* remove selected TimeMarkers */
static int ed_marker_delete_exec(bContext *C, wmOperator *op)
{
	ListBase *markers= context_get_markers(C);
	TimeMarker *marker, *nmarker;
	short changed= 0;
	
	if(markers == NULL)
		return OPERATOR_CANCELLED;
	
	for(marker= markers->first; marker; marker= nmarker) {
		nmarker= marker->next;
		if(marker->flag & SELECT) {
			BLI_freelinkN(markers, marker);
			changed= 1;
		}
	}
	
	if(changed) {
		WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	}
	return OPERATOR_FINISHED;
}


static void MARKER_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Markers";
	ot->idname= "MARKER_OT_delete";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= ed_marker_delete_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}

/* ************************** registration **********************************/

/* called in screen_ops.c:ED_operatortypes_screen() */
void ED_marker_operatortypes(void)
{
	WM_operatortype_append(MARKER_OT_add);
	WM_operatortype_append(MARKER_OT_move);
	WM_operatortype_append(MARKER_OT_duplicate);
	WM_operatortype_append(MARKER_OT_select);
	WM_operatortype_append(MARKER_OT_select_border);
	WM_operatortype_append(MARKER_OT_select_all_toggle);
	WM_operatortype_append(MARKER_OT_delete);
}

/* called in screen_ops.c:ED_keymap_screen() */
void ED_marker_keymap(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "Markers", 0, 0);
	
	WM_keymap_verify_item(keymap, "MARKER_OT_add", MKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_move", EVT_TWEAK_S, KM_ANY, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "MARKER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", 1);
	WM_keymap_verify_item(keymap, "MARKER_OT_select_border", BKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_delete", XKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "MARKER_OT_move", GKEY, KM_PRESS, 0, 0);
	
}
