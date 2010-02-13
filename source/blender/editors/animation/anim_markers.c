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

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_fcurve.h"
#include "BKE_utildefines.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"
#include "UI_resources.h"

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

/* Get the marker that is closest to this point */
/* XXX for select, the min_dist should be small */
TimeMarker *ED_markers_find_nearest_marker (ListBase *markers, float x) 
{
	TimeMarker *marker, *nearest=NULL;
	float dist, min_dist= 1000000;
	
	if (markers) {
		for (marker= markers->first; marker; marker= marker->next) {
			dist = ABS((float)marker->frame - x);
			
			if (dist < min_dist) {
				min_dist= dist;
				nearest= marker;
			}
		}
	}
	
	return nearest;
}

/* Return the time of the marker that occurs on a frame closest to the given time */
int ED_markers_find_nearest_marker_time (ListBase *markers, float x)
{
	TimeMarker *nearest= ED_markers_find_nearest_marker(markers, x);
	return (nearest) ? (nearest->frame) : (int)floor(x + 0.5f);
}


void ED_markers_get_minmax (ListBase *markers, short sel, float *first, float *last)
{
	TimeMarker *marker;
	float min, max;
	int selcount = 0;
	
	/* sanity check */
	//printf("markers = %p -  %p, %p \n", markers, markers->first, markers->last);
	if (markers == NULL) {
		*first = 0.0f;
		*last = 0.0f;
		return;
	}
	
	if (markers->first && markers->last) {
		TimeMarker *fm= markers->first;
		TimeMarker *lm= markers->last;
		
		min= (float)fm->frame;
		max= (float)lm->frame;
	}
	else {
		*first = 0.0f;
		*last = 0.0f;
		return;
	}
	
	/* count how many markers are usable - see later */
	if (sel) {
		for (marker= markers->first; marker; marker= marker->next) {
			if (marker->flag & SELECT)
				selcount++;
		}
	}
	else
		selcount= BLI_countlist(markers);
	
	/* if only selected are to be considered, only consider the selected ones
	 * (optimisation for not searching list) 
	 */
	if (selcount > 1) {
		for (marker= markers->first; marker; marker= marker->next) {
			if (sel) {
				if (marker->flag & SELECT) {
					if (marker->frame < min)
						min= (float)marker->frame;
					if (marker->frame > max)
						max= (float)marker->frame;
				}
			}
			else {
				if (marker->frame < min)
					min= (float)marker->frame;
				if (marker->frame > max)
					max= (float)marker->frame;
			}	
		}
	}
	
	/* set the min/max values */
	*first= min;
	*last= max;
}

/* Adds a marker to list of cfra elems */
void add_marker_to_cfra_elem(ListBase *lb, TimeMarker *marker, short only_sel)
{
	CfraElem *ce, *cen;
	
	/* should this one only be considered if it is selected? */
	if ((only_sel) && ((marker->flag & SELECT)==0))
		return;
	
	/* insertion sort - try to find a previous cfra elem */
	for (ce= lb->first; ce; ce= ce->next) {
		if (ce->cfra == marker->frame) {
			/* do because of double keys */
			if (marker->flag & SELECT) 
				ce->sel= marker->flag;
			return;
		}
		else if (ce->cfra > marker->frame) break;
	}	
	
	cen= MEM_callocN(sizeof(CfraElem), "add_to_cfra_elem");	
	if (ce) BLI_insertlinkbefore(lb, ce, cen);
	else BLI_addtail(lb, cen);

	cen->cfra= marker->frame;
	cen->sel= marker->flag;
}

/* This function makes a list of all the markers. The only_sel
 * argument is used to specify whether only the selected markers
 * are added.
 */
void ED_markers_make_cfra_list(ListBase *markers, ListBase *lb, short only_sel)
{
	TimeMarker *marker;
	
	if (markers == NULL)
		return;
	
	for (marker= markers->first; marker; marker= marker->next)
		add_marker_to_cfra_elem(lb, marker, only_sel);
}

/* ************* Marker Drawing ************ */

/* function to draw markers */
static void draw_marker(View2D *v2d, TimeMarker *marker, int cfra, int flag)
{
	float xpos, ypixels, xscale, yscale;
	int icon_id= 0;
	
	xpos = marker->frame;
	
	/* no time correction for framelen! space is drawn with old values */
	ypixels= v2d->mask.ymax-v2d->mask.ymin;
	UI_view2d_getscale(v2d, &xscale, &yscale);
	
	glScalef(1.0f/xscale, 1.0f, 1.0f);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);			
	
	/* vertical line - dotted */
#ifdef DURIAN_CAMERA_SWITCH
	if ((marker->camera) || (flag & DRAW_MARKERS_LINES)) {
#else
	if (flag & DRAW_MARKERS_LINES) {
#endif
		setlinestyle(3);
		
		if (marker->flag & SELECT)
			glColor4ub(255, 255, 255, 96);
		else
			glColor4ub(0, 0, 0, 96);
		
		glBegin(GL_LINES);
			glVertex2f((xpos*xscale)+0.5f, 12.0f);
			glVertex2f((xpos*xscale)+0.5f, (v2d->cur.ymax+12.0f)*yscale);
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
	
	UI_icon_draw(xpos*xscale-5.0f, 16.0f, icon_id);
	
	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);
	
	/* and the marker name too, shifted slightly to the top-right */
	if (marker->name && marker->name[0]) {
		float x, y;
		
		if (marker->flag & SELECT) {
			UI_ThemeColor(TH_TEXT_HI);
			x= xpos*xscale + 4.0f;
			y= (ypixels <= 39.0f)? (ypixels-10.0f) : 29.0f;
		}
		else {
			UI_ThemeColor(TH_TEXT);
			if((marker->frame <= cfra) && (marker->frame+5 > cfra)) {
				x= xpos*xscale + 4.0f;
				y= (ypixels <= 39.0f)? (ypixels - 10.0f) : 29.0f;
			}
			else {
				x= xpos*xscale + 4.0f;
				y= 17.0f;
			}
		}
		UI_DrawString(x, y, marker->name);
	}
	
	glScalef(xscale, 1.0f, 1.0f);
}

/* Draw Scene-Markers in time window */
void draw_markers_time(const bContext *C, int flag)
{
	ListBase *markers= context_get_markers(C);
	View2D *v2d= UI_view2d_fromcontext(C);
	TimeMarker *marker;
	
	if (markers == NULL)
		return;
	
	/* unselected markers are drawn at the first time */
	for (marker= markers->first; marker; marker= marker->next) {
		if ((marker->flag & SELECT) == 0) 
			draw_marker(v2d, marker, CTX_data_scene(C)->r.cfra, flag);
	}
	
	/* selected markers are drawn later */
	for (marker= markers->first; marker; marker= marker->next) {
		if (marker->flag & SELECT) 
			draw_marker(v2d, marker, CTX_data_scene(C)->r.cfra, flag);
	}
}

/* ************************** add markers *************************** */

/* add TimeMarker at curent frame */
static int ed_marker_add(bContext *C, wmOperator *op)
{
	ListBase *markers= context_get_markers(C);
	TimeMarker *marker;
	int frame= CTX_data_scene(C)->r.cfra;
	
	if (markers == NULL)
		return OPERATOR_CANCELLED;
	
	/* two markers can't be at the same place */
	for (marker= markers->first; marker; marker= marker->next) {
		if (marker->frame == frame) 
			return OPERATOR_CANCELLED;
	}
	
	/* deselect all */
	for(marker= markers->first; marker; marker= marker->next)
		marker->flag &= ~SELECT;
	
	marker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
	marker->flag= SELECT;
	marker->frame= frame;
	sprintf(marker->name, "F_%02d", frame); // XXX - temp code only
	BLI_addtail(markers, marker);
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	
	return OPERATOR_FINISHED;
}

static void MARKER_OT_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Time Marker";
	ot->description= "Add a new time marker";
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
	
	/* free data */
	MEM_freeN(mm->oldframe);
	MEM_freeN(op->customdata);
	op->customdata= NULL;
	
	/* clear custom header prints */
	ED_area_headerprint(CTX_wm_area(C), NULL);
}

static int ed_marker_move_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	if(ed_marker_move_init(C, op)) {
		MarkerMove *mm= op->customdata;
		
		mm->evtx= evt->x;
		mm->firstx= evt->x;
		mm->event_type= evt->type;
		
		/* add temp handler */
		WM_event_add_modal_handler(C, op);
		
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
						SpaceAction *saction= (SpaceAction *)mm->slink;
						if (saction->flag & SACTION_DRAWTIME)
							sprintf(str, "Marker %.2f offset %.2f", FRA2TIME(selmarker->frame), FRA2TIME(offs));
						else
							sprintf(str, "Marker %.2f offset %.2f", (double)(selmarker->frame), (double)(offs));
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
					else if (mm->slink->spacetype == SPACE_ACTION) {
						SpaceAction *saction= (SpaceAction *)mm->slink;
						if (saction->flag & SACTION_DRAWTIME)
							sprintf(str, "Marker offset %.2f ", FRA2TIME(offs));
						else
							sprintf(str, "Marker offset %.2f ", (double)(offs));
					}
					else {
						sprintf(str, "Marker offset %.2f ", (double)(offs));
					}
				}
				
				WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
				ED_area_headerprint(CTX_wm_area(C), str);
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
	ot->description= "Move selected time marker(s)";
	ot->idname= "MARKER_OT_move";
	
	/* api callbacks */
	ot->exec= ed_marker_move_exec;
	ot->invoke= ed_marker_move_invoke;
	ot->modal= ed_marker_move_modal;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING|OPTYPE_GRAB_POINTER;
	
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
	
	if (markers == NULL) 
		return;

	/* go through the list of markers, duplicate selected markers and add duplicated copies
	 * to the begining of the list (unselect original markers) 
	 */
	for (marker= markers->first; marker; marker= marker->next) {
		if (marker->flag & SELECT) {
			/* unselect selected marker */
			marker->flag &= ~SELECT;
			
			/* create and set up new marker */
			newmarker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
			newmarker->flag= SELECT;
			newmarker->frame= marker->frame;
			BLI_strncpy(newmarker->name, marker->name, sizeof(marker->name));
			
#ifdef DURIAN_CAMERA_SWITCH
			newmarker->camera= marker->camera;
#endif

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
	ot->description= "Duplicate selected time marker(s)";
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
	
	for (marker= markers->first; marker; marker= marker->next) {
		/* if Shift is not set, then deselect Markers */
		if (!shift) marker->flag &= ~SELECT;
		
		/* this way a not-shift select will allways give 1 selected marker */
		if ((marker->frame == frame) && (!select)) {
			if (marker->flag & SELECT) 
				marker->flag &= ~SELECT;
			else
				marker->flag |= SELECT;
			select = 1;
		}
	}
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
	
	cfra= ED_markers_find_nearest_marker_time(markers, viewx);
	
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
	ot->description= "Select time marker(s)";
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
	int gesture_mode= RNA_int_get(op->ptr, "gesture_mode");
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
			switch (gesture_mode) {
				case GESTURE_MODAL_SELECT:
					if ((marker->flag & SELECT) == 0) 
						marker->flag |= SELECT;
					break;
				case GESTURE_MODAL_DESELECT:
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
	ot->description= "Select all time markers using border selection";
	ot->idname= "MARKER_OT_select_border";
	
	/* api callbacks */
	ot->exec= ed_marker_border_select_exec;
	ot->invoke= WM_border_select_invoke;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, FALSE);
}

/* *********************** (de)select all ***************** */

static int ed_marker_select_all_exec(bContext *C, wmOperator *op)
{
	ListBase *markers= context_get_markers(C);
	TimeMarker *marker;
	int action = RNA_enum_get(op->ptr, "action");

	if(markers == NULL)
		return OPERATOR_CANCELLED;

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		for(marker= markers->first; marker; marker= marker->next) {
			if(marker->flag & SELECT) {
				action = SEL_DESELECT;
				break;
			}
		}
	}
	
	for(marker= markers->first; marker; marker= marker->next) {
		switch (action) {
		case SEL_SELECT:
			marker->flag |= SELECT;
			break;
		case SEL_DESELECT:
			marker->flag &= ~SELECT;
			break;
		case SEL_INVERT:
			if (marker->flag & SELECT) {
				marker->flag &= ~SELECT;
			} else {
				marker->flag |= SELECT;
			}
			break;
		}
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);

	return OPERATOR_FINISHED;
}

static void MARKER_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "(De)select all markers";
	ot->description= "Change selection of all time markers";
	ot->idname= "MARKER_OT_select_all";
	
	/* api callbacks */
	ot->exec= ed_marker_select_all_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_select_all(ot);
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
	
	if (changed)
		WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	
	return OPERATOR_FINISHED;
}


static void MARKER_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Markers";
	ot->description= "Delete selected time marker(s)";
	ot->idname= "MARKER_OT_delete";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= ed_marker_delete_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}

static int ed_marker_make_links_scene_exec(bContext *C, wmOperator *op)
{
	ListBase *markers= context_get_markers(C);
	Scene *scene_to= BLI_findlink(&CTX_data_main(C)->scene, RNA_enum_get(op->ptr, "type"));
	TimeMarker *marker, *marker_new;

	if(scene_to==NULL) {
		BKE_report(op->reports, RPT_ERROR, "Scene not found");
		return OPERATOR_CANCELLED;
	}

	if(scene_to == CTX_data_scene(C)) {
		BKE_report(op->reports, RPT_ERROR, "Can't link objects into the same scene");
		return OPERATOR_CANCELLED;
	}

	/* copy markers */
	for (marker= markers->first; marker; marker= marker->next) {
		if(marker->flag & SELECT) {
			marker_new= MEM_dupallocN(marker);
			BLI_addtail(&scene_to->markers, marker_new);
		}
	}

	return OPERATOR_FINISHED;
}

static void MARKER_OT_make_links_scene(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Make Links to Scene";
	ot->description= "Link markers to another scene";
	ot->idname= "MARKER_OT_make_links_scene";

	/* api callbacks */
	ot->exec= ed_marker_make_links_scene_exec;
	ot->poll= ED_operator_areaactive;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	prop= RNA_def_enum(ot->srna, "type", DummyRNA_NULL_items, 0, "Type", "");
	RNA_def_enum_funcs(prop, RNA_scene_itemf);

}

#ifdef DURIAN_CAMERA_SWITCH
/* ******************************* camera bind marker ***************** */

/* remove selected TimeMarkers */
static int ed_marker_camera_bind_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ListBase *markers= context_get_markers(C);
	TimeMarker *marker;
	short changed= 0;

	if(markers == NULL)
		return OPERATOR_CANCELLED;

	for(marker= markers->first; marker; marker= marker->next) {
		if(marker->flag & SELECT) {
			marker->camera= scene->camera;
		}
	}

	if (changed)
		WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);

	return OPERATOR_FINISHED;
}

static void MARKER_OT_camera_bind(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Bind Camera to Markers";
	ot->description= "Bind the active camera to selected markers(s)";
	ot->idname= "MARKER_OT_camera_bind";

	/* api callbacks */
	ot->exec= ed_marker_camera_bind_exec;
	ot->poll= ED_operator_areaactive;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
#endif

/* ************************** registration **********************************/

/* called in screen_ops.c:ED_operatortypes_screen() */
void ED_operatortypes_marker(void)
{
	WM_operatortype_append(MARKER_OT_add);
	WM_operatortype_append(MARKER_OT_move);
	WM_operatortype_append(MARKER_OT_duplicate);
	WM_operatortype_append(MARKER_OT_select);
	WM_operatortype_append(MARKER_OT_select_border);
	WM_operatortype_append(MARKER_OT_select_all);
	WM_operatortype_append(MARKER_OT_delete);
	WM_operatortype_append(MARKER_OT_make_links_scene);
#ifdef DURIAN_CAMERA_SWITCH
	WM_operatortype_append(MARKER_OT_camera_bind);
#endif
}

/* called in screen_ops.c:ED_keymap_screen() */
void ED_marker_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap= WM_keymap_find(keyconf, "Markers", 0, 0);
	
	WM_keymap_verify_item(keymap, "MARKER_OT_add", MKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_move", EVT_TWEAK_S, KM_ANY, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "MARKER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", 1);
	WM_keymap_verify_item(keymap, "MARKER_OT_select_border", BKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_select_all", AKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_delete", DELKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "MARKER_OT_move", GKEY, KM_PRESS, 0, 0);
#ifdef DURIAN_CAMERA_SWITCH
	WM_keymap_add_item(keymap, "MARKER_OT_camera_bind", BKEY, KM_PRESS, KM_CTRL, 0);
#endif
}
