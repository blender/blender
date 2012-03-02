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

/** \file blender/editors/animation/anim_markers.c
 *  \ingroup edanimation
 */


#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"
#include "UI_resources.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_numinput.h"
#include "ED_object.h"
#include "ED_transform.h"
#include "ED_types.h"

/* ************* Marker API **************** */

/* helper function for getting the list of markers to work on */
static ListBase *context_get_markers(Scene *scene, ScrArea *sa)
{
	/* local marker sets... */
	if (sa) {
		if (sa->spacetype == SPACE_ACTION) {
			SpaceAction *saction = (SpaceAction *)sa->spacedata.first;
			
			/* local markers can only be shown when there's only a single active action to grab them from 
			 * 	- flag only takes effect when there's an action, otherwise it can get too confusing?
			 */
			if (ELEM(saction->mode, SACTCONT_ACTION, SACTCONT_SHAPEKEY) && (saction->action)) 
			{
				if (saction->flag & SACTION_POSEMARKERS_SHOW)
					return &saction->action->markers;
			}
		}
	}
	
	/* default to using the scene's markers */
	return &scene->markers;
}

/* ............. */

/* public API for getting markers from context */
ListBase *ED_context_get_markers(const bContext *C)
{
	return context_get_markers(CTX_data_scene(C), CTX_wm_area(C));
}

/* public API for getting markers from "animation" context */
ListBase *ED_animcontext_get_markers(const bAnimContext *ac)
{
	if (ac)
		return context_get_markers(ac->scene, ac->sa);
	else
		return NULL;
}

/* --------------------------------- */

/* Apply some transformation to markers after the fact 
 * < markers: list of markers to affect - this may or may not be the scene markers list, so don't assume anything
 * < scene: current scene (for getting current frame)
 * < mode: (TfmMode) transform mode that this transform is for
 * < value: from the transform code, this is t->vec[0] (which is delta transform for grab/extend, and scale factor for scale)
 * < side: (B/L/R) for 'extend' functionality, which side of current frame to use
 */
int ED_markers_post_apply_transform (ListBase *markers, Scene *scene, int mode, float value, char side)
{
	TimeMarker *marker;
	float cfra = (float)CFRA;
	int changed = 0;
	
	/* sanity check */
	if (markers == NULL)
		return changed;
	
	/* affect selected markers - it's unlikely that we will want to affect all in this way? */
	for (marker = markers->first; marker; marker = marker->next) {
		if (marker->flag & SELECT) {
			switch (mode) {
				case TFM_TIME_TRANSLATE:
				case TFM_TIME_EXTEND:
				{
					/* apply delta if marker is on the right side of the current frame */
					if ((side=='B') ||
						(side=='L' && marker->frame < cfra) || 
					    (side=='R' && marker->frame >= cfra))
					{
						marker->frame += (int)floorf(value + 0.5f);
						changed++;
					}
				}
					break;
					
				case TFM_TIME_SCALE:
				{	
					/* rescale the distance between the marker and the current frame */
					marker->frame= cfra + (int)floorf(((float)(marker->frame - cfra) * value) + 0.5f);
					changed++;
				}
					break;
			}
		}
	}
	
	return changed;
}

/* --------------------------------- */

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
	 * (optimization for not searching list)
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

/* --------------------------------- */

/* Adds a marker to list of cfra elems */
static void add_marker_to_cfra_elem(ListBase *lb, TimeMarker *marker, short only_sel)
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

/* --------------------------------- */

/* Get the first selected marker */
TimeMarker *ED_markers_get_first_selected(ListBase *markers)
{
	TimeMarker *marker;
	
	if (markers) {
		for (marker = markers->first; marker; marker = marker->next) {
			if (marker->flag & SELECT)
				return marker;
		}
	}
	
	return NULL;
}

/* --------------------------------- */

/* Print debugging prints of list of markers 
 * BSI's: do NOT make static or put in if-defs as "unused code". That's too much trouble when we need to use for quick debugging!
 */
void debug_markers_print_list(ListBase *markers)
{
	TimeMarker *marker;
	
	if (markers == NULL) {
		printf("No markers list to print debug for\n");
		return;
	}
	
	printf("List of markers follows: -----\n");
	
	for (marker = markers->first; marker; marker = marker->next) {
		printf("\t'%s' on %d at %p with %u\n", marker->name, marker->frame, (void *)marker, marker->flag);
	}
	
	printf("End of list ------------------\n");
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
	if ((marker->camera) || (flag & DRAW_MARKERS_LINES))
#else
	if (flag & DRAW_MARKERS_LINES)
#endif
	{
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

#ifdef DURIAN_CAMERA_SWITCH
		if(marker->camera && (marker->camera->restrictflag & OB_RESTRICT_RENDER)) {
			float col[4];
			glGetFloatv(GL_CURRENT_COLOR, col);
			col[3]= 0.4;
			glColor4fv(col);
		}
#endif

		UI_DrawString(x, y, marker->name);
	}
	
	glScalef(xscale, 1.0f, 1.0f);
}

/* Draw Scene-Markers in time window */
void draw_markers_time(const bContext *C, int flag)
{
	ListBase *markers= ED_context_get_markers(C);
	View2D *v2d;
	TimeMarker *marker;
	Scene *scene;

	if (markers == NULL)
		return;

	scene = CTX_data_scene(C);
	v2d = UI_view2d_fromcontext(C);

	/* unselected markers are drawn at the first time */
	for (marker= markers->first; marker; marker= marker->next) {
		if ((marker->flag & SELECT) == 0) {
			draw_marker(v2d, marker, scene->r.cfra, flag);
		}
	}
	
	/* selected markers are drawn later */
	for (marker= markers->first; marker; marker= marker->next) {
		if (marker->flag & SELECT) {
			draw_marker(v2d, marker, scene->r.cfra, flag);
		}
	}
}

/* ************************ Marker Wrappers API ********************* */
/* These wrappers allow marker operators to function within the confines 
 * of standard animation editors, such that they can coexist with the 
 * primary operations of those editors.
 */

/* ------------------------ */

/* special poll() which checks if there are selected markers first */
static int ed_markers_poll_selected_markers(bContext *C)
{
	ListBase *markers = ED_context_get_markers(C);
	
	/* first things first: markers can only exist in timeline views */
	if (ED_operator_animview_active(C) == 0)
		return 0;
		
	/* check if some marker is selected */
	return ED_markers_get_first_selected(markers) != NULL;
}

/* special poll() which checks if there are any markers at all first */
static int ed_markers_poll_markers_exist(bContext *C)
{
	ListBase *markers = ED_context_get_markers(C);
	
	/* first things first: markers can only exist in timeline views */
	if (ED_operator_animview_active(C) == 0)
		return 0;
		
	/* list of markers must exist, as well as some markers in it! */
	return (markers && markers->first);
}
 
/* ------------------------ */ 

/* Second-tier invoke() callback that performs context validation before running the  
 * "custom"/third-tier invoke() callback supplied as the last arg (which would normally
 * be the operator's invoke() callback elsewhere)
 *
 * < invoke_func: (fn(bContext*, wmOperator*, wmEvent*)=int) "standard" invoke function 
 *			that operator would otherwise have used. If NULL, the operator's standard
 *			exec() callback will be called instead in the appropriate places.
 */
static int ed_markers_opwrap_invoke_custom(bContext *C, wmOperator *op, wmEvent *evt, 
		int (*invoke_func)(bContext*,wmOperator*,wmEvent*))
{
	ScrArea *sa = CTX_wm_area(C);
	int retval = OPERATOR_PASS_THROUGH;
	
	/* removed check for Y coord of event, keymap has bounbox now */
	
	/* allow operator to run now */
	if (invoke_func)
		retval = invoke_func(C, op, evt);
	else if (op->type->exec)
		retval = op->type->exec(C, op);
	else
		BKE_report(op->reports, RPT_ERROR, "Programming error: operator doesn't actually have code to do anything!");
		
	/* return status modifications - for now, make this spacetype dependent as above */
	if (sa->spacetype != SPACE_TIME) {
		/* unless successful, must add "pass-through" to let normal operator's have a chance at tackling this event */
		if (retval != OPERATOR_FINISHED)
			retval |= OPERATOR_PASS_THROUGH;
	}
	
	return retval;
}

/* standard wrapper - first-tier invoke() callback to be directly assigned to operator typedata
 * for operators which don't need any special invoke calls. Any operators with special invoke calls
 * though will need to implement their own wrapper which calls the second-tier callback themselves
 * (passing through the custom invoke function they use)
 */
static int ed_markers_opwrap_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	return ed_markers_opwrap_invoke_custom(C, op, evt, NULL);
}

/* ************************** add markers *************************** */

/* add TimeMarker at curent frame */
static int ed_marker_add(bContext *C, wmOperator *UNUSED(op))
{
	ListBase *markers= ED_context_get_markers(C);
	TimeMarker *marker;
	int frame= CTX_data_scene(C)->r.cfra;
	
	if (markers == NULL)
		return OPERATOR_CANCELLED;
	
	/* prefer not having 2 markers at the same place,
	 * though the user can move them to overlap once added */
	for (marker= markers->first; marker; marker= marker->next) {
		if (marker->frame == frame) 
			return OPERATOR_CANCELLED;
	}
	
	/* deselect all */
	for (marker= markers->first; marker; marker= marker->next)
		marker->flag &= ~SELECT;
	
	marker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
	marker->flag= SELECT;
	marker->frame= frame;
	BLI_snprintf(marker->name, sizeof(marker->name), "F_%02d", frame); // XXX - temp code only
	BLI_addtail(markers, marker);
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);
	
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
	ot->invoke = ed_markers_opwrap_invoke;
	ot->poll= ED_operator_animview_active;
	
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

	cancel() to escape from modal

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
	NumInput num;
} MarkerMove;

/* copy selection to temp buffer */
/* return 0 if not OK */
static int ed_marker_move_init(bContext *C, wmOperator *op)
{
	ListBase *markers= ED_context_get_markers(C);
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

	initNumInput(&mm->num);
	mm->num.idx_max = 0; /* one axis */
	mm->num.flag |= NUM_NO_FRACTION;
	mm->num.increment = 1.0f;
	
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

static int ed_marker_move_invoke_wrapper(bContext *C, wmOperator *op, wmEvent *evt)
{
	return ed_markers_opwrap_invoke_custom(C, op, evt, ed_marker_move_invoke);
}

/* note, init has to be called succesfully */
static void ed_marker_move_apply(wmOperator *op)
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
static int ed_marker_move_cancel(bContext *C, wmOperator *op)
{
	RNA_int_set(op->ptr, "frames", 0);
	ed_marker_move_apply(op);
	ed_marker_move_exit(C, op);	
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);

	return OPERATOR_CANCELLED;
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
		
		case RIGHTMOUSE:
			/* press = user manually demands transform to be cancelled */
			if (evt->val == KM_PRESS) {
				ed_marker_move_cancel(C, op);
				return OPERATOR_CANCELLED;
			}
			/* else continue; <--- see if release event should be caught for tweak-end */
		
		case RETKEY:
		case PADENTER:
		case LEFTMOUSE:
		case MIDDLEMOUSE:
			if (WM_modal_tweak_exit(evt, mm->event_type)) {
				ed_marker_move_exit(C, op);
				WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
				WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);
				return OPERATOR_FINISHED;
			}
			break;
		case MOUSEMOVE:
			if (hasNumInput(&mm->num))
				break;
			
			dx= v2d->mask.xmax-v2d->mask.xmin;
			dx= (v2d->cur.xmax-v2d->cur.xmin)/dx;
			
			if (evt->x != mm->evtx) {	/* XXX maybe init for firsttime */
				int a, offs, totmark=0;
				
				mm->evtx= evt->x;
				
				fac= ((float)(evt->x - mm->firstx)*dx);
				
				if (mm->slink->spacetype == SPACE_TIME) 
					apply_keyb_grid(evt->shift, evt->ctrl, &fac, 0.0, FPS, 0.1*FPS, 0);
				else
					apply_keyb_grid(evt->shift, evt->ctrl, &fac, 0.0, 1.0, 0.1, 0 /*was: U.flag & USER_AUTOGRABGRID*/);
				
				offs= (int)fac;
				RNA_int_set(op->ptr, "frames", offs);
				ed_marker_move_apply(op);
				
				/* cruft below is for header print */
				for (a=0, marker= mm->markers->first; marker; marker= marker->next) {
					if (marker->flag & SELECT) {
						selmarker= marker;
						a++; totmark++;
					}
				}
				
				if (totmark==1) {	
					/* we print current marker value */
					if (mm->slink->spacetype == SPACE_TIME) {
						SpaceTime *stime= (SpaceTime *)mm->slink;
						if (stime->flag & TIME_DRAWFRAMES) 
							BLI_snprintf(str, sizeof(str), "Marker %d offset %d", selmarker->frame, offs);
						else 
							BLI_snprintf(str, sizeof(str), "Marker %.2f offset %.2f", FRA2TIME(selmarker->frame), FRA2TIME(offs));
					}
					else if (mm->slink->spacetype == SPACE_ACTION) {
						SpaceAction *saction= (SpaceAction *)mm->slink;
						if (saction->flag & SACTION_DRAWTIME)
							BLI_snprintf(str, sizeof(str), "Marker %.2f offset %.2f", FRA2TIME(selmarker->frame), FRA2TIME(offs));
						else
							BLI_snprintf(str, sizeof(str), "Marker %.2f offset %.2f", (double)(selmarker->frame), (double)(offs));
					}
					else {
						BLI_snprintf(str, sizeof(str), "Marker %.2f offset %.2f", (double)(selmarker->frame), (double)(offs));
					}
				}
				else {
					/* we only print the offset */
					if (mm->slink->spacetype == SPACE_TIME) { 
						SpaceTime *stime= (SpaceTime *)mm->slink;
						if (stime->flag & TIME_DRAWFRAMES) 
							BLI_snprintf(str, sizeof(str), "Marker offset %d ", offs);
						else 
							BLI_snprintf(str, sizeof(str), "Marker offset %.2f ", FRA2TIME(offs));
					}
					else if (mm->slink->spacetype == SPACE_ACTION) {
						SpaceAction *saction= (SpaceAction *)mm->slink;
						if (saction->flag & SACTION_DRAWTIME)
							BLI_snprintf(str, sizeof(str), "Marker offset %.2f ", FRA2TIME(offs));
						else
							BLI_snprintf(str, sizeof(str), "Marker offset %.2f ", (double)(offs));
					}
					else {
						BLI_snprintf(str, sizeof(str), "Marker offset %.2f ", (double)(offs));
					}
				}
				
				WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
				WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);
				ED_area_headerprint(CTX_wm_area(C), str);
			}
	}

	if (evt->val==KM_PRESS) {
		float vec[3];
		char str_tx[256];
		
		if (handleNumInput(&mm->num, evt))
		{
			applyNumInput(&mm->num, vec);
			outputNumInput(&mm->num, str_tx);
			
			RNA_int_set(op->ptr, "frames", vec[0]);
			ed_marker_move_apply(op);
			// ed_marker_header_update(C, op, str, (int)vec[0]);
			// strcat(str, str_tx);
			BLI_snprintf(str, sizeof(str), "Marker offset %s", str_tx);
			ED_area_headerprint(CTX_wm_area(C), str);
			
			WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
			WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

static int ed_marker_move_exec(bContext *C, wmOperator *op)
{
	if(ed_marker_move_init(C, op)) {
		ed_marker_move_apply(op);
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
	ot->invoke= ed_marker_move_invoke_wrapper;
	ot->modal= ed_marker_move_modal;
	ot->poll= ed_markers_poll_selected_markers;
	ot->cancel= ed_marker_move_cancel;
	
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
static void ed_marker_duplicate_apply(bContext *C)
{
	ListBase *markers= ED_context_get_markers(C);
	TimeMarker *marker, *newmarker;
	
	if (markers == NULL) 
		return;

	/* go through the list of markers, duplicate selected markers and add duplicated copies
	 * to the beginning of the list (unselect original markers)
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

			/* new marker is added to the beginning of list */
			// FIXME: bad ordering!
			BLI_addhead(markers, newmarker);
		}
	}
}

static int ed_marker_duplicate_exec(bContext *C, wmOperator *op)
{
	ed_marker_duplicate_apply(C);
	ed_marker_move_exec(C, op);	/* assumes frs delta set */
	
	return OPERATOR_FINISHED;
	
}

static int ed_marker_duplicate_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	ed_marker_duplicate_apply(C);
	return ed_marker_move_invoke(C, op, evt);
}

static int ed_marker_duplicate_invoke_wrapper(bContext *C, wmOperator *op, wmEvent *evt)
{
	return ed_markers_opwrap_invoke_custom(C, op, evt, ed_marker_duplicate_invoke);
}

static void MARKER_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate Time Marker";
	ot->description= "Duplicate selected time marker(s)";
	ot->idname= "MARKER_OT_duplicate";
	
	/* api callbacks */
	ot->exec= ed_marker_duplicate_exec;
	ot->invoke= ed_marker_duplicate_invoke_wrapper;
	ot->modal= ed_marker_move_modal;
	ot->poll= ed_markers_poll_selected_markers;
	ot->cancel= ed_marker_move_cancel;
	
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

static int ed_marker_select(bContext *C, wmEvent *evt, int extend, int camera)
{
	ListBase *markers= ED_context_get_markers(C);
	View2D *v2d= UI_view2d_fromcontext(C);
	float viewx;
	int x, y, cfra;
	
	if (markers == NULL)
		return OPERATOR_PASS_THROUGH;

	x= evt->x - CTX_wm_region(C)->winrct.xmin;
	y= evt->y - CTX_wm_region(C)->winrct.ymin;
	
	UI_view2d_region_to_view(v2d, x, y, &viewx, NULL);	
	
	cfra= ED_markers_find_nearest_marker_time(markers, viewx);
	
	if (extend)
		select_timeline_marker_frame(markers, cfra, 1);
	else
		select_timeline_marker_frame(markers, cfra, 0);
	
#ifdef DURIAN_CAMERA_SWITCH

	if (camera) {
		Scene *scene= CTX_data_scene(C);
		Base *base;
		TimeMarker *marker;
		int sel= 0;
		
		if (!extend)
			scene_deselect_all(scene);
		
		for (marker= markers->first; marker; marker= marker->next) {
			if(marker->frame==cfra) {
				sel= (marker->flag & SELECT);
				break;
			}
		}
		
		for (marker= markers->first; marker; marker= marker->next) {
			if (marker->camera) {
				if (marker->frame==cfra) {
					base= object_in_scene(marker->camera, scene);
					if (base) {
						ED_base_object_select(base, sel);
						if(sel)
							ED_base_object_activate(C, base);
					}
				}
			}
		}
		
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);
	}
#else
	(void)camera;
#endif

	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);

	/* allowing tweaks, but needs OPERATOR_FINISHED, otherwise renaming fails... [#25987] */
	return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;
}

static int ed_marker_select_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	short extend= RNA_boolean_get(op->ptr, "extend");
	short camera= 0;
#ifdef DURIAN_CAMERA_SWITCH
	camera= RNA_boolean_get(op->ptr, "camera");
#endif
	return ed_marker_select(C, evt, extend, camera);
}

static int ed_marker_select_invoke_wrapper(bContext *C, wmOperator *op, wmEvent *evt)
{
	return ed_markers_opwrap_invoke_custom(C, op, evt, ed_marker_select_invoke);
}

static void MARKER_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Time Marker";
	ot->description= "Select time marker(s)";
	ot->idname= "MARKER_OT_select";
	
	/* api callbacks */
	ot->invoke= ed_marker_select_invoke_wrapper;
	ot->poll= ed_markers_poll_markers_exist;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
#ifdef DURIAN_CAMERA_SWITCH
	RNA_def_boolean(ot->srna, "camera", 0, "Camera", "Select the camera");
#endif
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
	ListBase *markers= ED_context_get_markers(C);
	TimeMarker *marker;
	float xminf, xmaxf, yminf, ymaxf;
	int gesture_mode= RNA_int_get(op->ptr, "gesture_mode");
	int xmin= RNA_int_get(op->ptr, "xmin");
	int xmax= RNA_int_get(op->ptr, "xmax");
	int ymin= RNA_int_get(op->ptr, "ymin");
	int ymax= RNA_int_get(op->ptr, "ymax");
	int extend= RNA_boolean_get(op->ptr, "extend");
	
	UI_view2d_region_to_view(v2d, xmin, ymin, &xminf, &yminf);	
	UI_view2d_region_to_view(v2d, xmax, ymax, &xmaxf, &ymaxf);	
	
	if (markers == NULL)
		return 0;
	
	/* XXX marker context */
	for (marker= markers->first; marker; marker= marker->next) {
		if ((marker->frame > xminf) && (marker->frame <= xmaxf)) {
			switch (gesture_mode) {
				case GESTURE_MODAL_SELECT:
					marker->flag |= SELECT;
					break;
				case GESTURE_MODAL_DESELECT:
					marker->flag &= ~SELECT;
					break;
			}
		}
		else if (!extend) {
			marker->flag &= ~SELECT;
		}
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);

	return 1;
}

static int ed_marker_select_border_invoke_wrapper(bContext *C, wmOperator *op, wmEvent *evt)
{
	return ed_markers_opwrap_invoke_custom(C, op, evt, WM_border_select_invoke);
}

static void MARKER_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Marker Border select";
	ot->description= "Select all time markers using border selection";
	ot->idname= "MARKER_OT_select_border";
	
	/* api callbacks */
	ot->exec= ed_marker_border_select_exec;
	ot->invoke= ed_marker_select_border_invoke_wrapper;
	ot->modal= WM_border_select_modal;
	ot->cancel= WM_border_select_cancel;
	
	ot->poll= ed_markers_poll_markers_exist;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, TRUE);
}

/* *********************** (de)select all ***************** */

static int ed_marker_select_all_exec(bContext *C, wmOperator *op)
{
	ListBase *markers= ED_context_get_markers(C);
	TimeMarker *marker;
	int action = RNA_enum_get(op->ptr, "action");

	if (markers == NULL)
		return OPERATOR_CANCELLED;

	if (action == SEL_TOGGLE) {
		action = (ED_markers_get_first_selected(markers) != NULL) ? SEL_DESELECT : SEL_SELECT;
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
			marker->flag ^= SELECT;
			break;
		}
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);

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
	ot->invoke = ed_markers_opwrap_invoke;
	ot->poll= ed_markers_poll_markers_exist;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_select_all(ot);
}

/* ***************** remove marker *********************** */

/* remove selected TimeMarkers */
static int ed_marker_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	ListBase *markers= ED_context_get_markers(C);
	TimeMarker *marker, *nmarker;
	short changed= 0;
	
	if (markers == NULL)
		return OPERATOR_CANCELLED;
	
	for (marker= markers->first; marker; marker= nmarker) {
		nmarker= marker->next;
		if (marker->flag & SELECT) {
			BLI_freelinkN(markers, marker);
			changed= 1;
		}
	}
	
	if (changed) {
		WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
		WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);
	}
	
	return OPERATOR_FINISHED;
}

static int ed_marker_delete_invoke_wrapper(bContext *C, wmOperator *op, wmEvent *evt)
{
	// XXX: must we keep these confirmations?
	return ed_markers_opwrap_invoke_custom(C, op, evt, WM_operator_confirm);
}

static void MARKER_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Markers";
	ot->description= "Delete selected time marker(s)";
	ot->idname= "MARKER_OT_delete";
	
	/* api callbacks */
	ot->invoke= ed_marker_delete_invoke_wrapper;
	ot->exec= ed_marker_delete_exec;
	ot->poll= ed_markers_poll_selected_markers;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;	
}


/* **************** rename marker ***************** */

/* rename first selected TimeMarker */
static int ed_marker_rename_exec(bContext *C, wmOperator *op)
{
	TimeMarker *marker= ED_markers_get_first_selected(ED_context_get_markers(C));

	if (marker) {
		RNA_string_get(op->ptr, "name", marker->name);
		
		WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
		WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);
		
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static int ed_marker_rename_invoke_wrapper(bContext *C, wmOperator *op, wmEvent *evt)
{
	/* must initialize the marker name first if there is a marker selected */
	TimeMarker *marker = ED_markers_get_first_selected(ED_context_get_markers(C));
	if (marker)
		RNA_string_set(op->ptr, "name", marker->name);
	
	/* now see if the operator is usable */
	return ed_markers_opwrap_invoke_custom(C, op, evt, WM_operator_props_popup);
}

static void MARKER_OT_rename(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Rename Marker";
	ot->description= "Rename first selected time marker";
	ot->idname= "MARKER_OT_rename";
	
	/* api callbacks */
	ot->invoke= ed_marker_rename_invoke_wrapper;
	ot->exec= ed_marker_rename_exec;
	ot->poll= ed_markers_poll_selected_markers;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;	
	
	/* properties */
	ot->prop = RNA_def_string(ot->srna, "name", "RenamedMarker", sizeof(((TimeMarker *)NULL)->name), "Name", "New name for marker");
	//RNA_def_boolean(ot->srna, "ensure_unique", 0, "Ensure Unique", "Ensure that new name is unique within collection of markers");
}

/* **************** make links to scene ***************** */

static int ed_marker_make_links_scene_exec(bContext *C, wmOperator *op)
{
	ListBase *markers= ED_context_get_markers(C);
	Scene *scene_to= BLI_findlink(&CTX_data_main(C)->scene, RNA_enum_get(op->ptr, "scene"));
	TimeMarker *marker, *marker_new;

	if (scene_to==NULL) {
		BKE_report(op->reports, RPT_ERROR, "Scene not found");
		return OPERATOR_CANCELLED;
	}

	if (scene_to == CTX_data_scene(C)) {
		BKE_report(op->reports, RPT_ERROR, "Can't re-link markers into the same scene");
		return OPERATOR_CANCELLED;
	}

	/* copy markers */
	for (marker= markers->first; marker; marker= marker->next) {
		if (marker->flag & SELECT) {
			marker_new= MEM_dupallocN(marker);
			marker_new->prev= marker_new->next = NULL;
			
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
	ot->description= "Copy selected markers to another scene";
	ot->idname= "MARKER_OT_make_links_scene";

	/* api callbacks */
	ot->exec= ed_marker_make_links_scene_exec;
	ot->invoke = ed_markers_opwrap_invoke;
	ot->poll= ed_markers_poll_selected_markers;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	prop= RNA_def_enum(ot->srna, "scene", DummyRNA_NULL_items, 0, "Scene", "");
	RNA_def_enum_funcs(prop, RNA_scene_itemf);
	ot->prop= prop;

}

#ifdef DURIAN_CAMERA_SWITCH
/* ******************************* camera bind marker ***************** */

static int ed_marker_camera_bind_exec(bContext *C, wmOperator *UNUSED(op))
{
	bScreen *sc= CTX_wm_screen(C);
	Scene *scene= CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	ListBase *markers= ED_context_get_markers(C);
	TimeMarker *marker;

	marker= ED_markers_get_first_selected(markers);
	if(marker == NULL)
		return OPERATOR_CANCELLED;

	marker->camera= ob;

	/* camera may have changes */
	scene_camera_switch_update(scene);
	BKE_screen_view3d_scene_sync(sc);

	WM_event_add_notifier(C, NC_SCENE|ND_MARKERS, NULL);
	WM_event_add_notifier(C, NC_ANIMATION|ND_MARKERS, NULL);
	WM_event_add_notifier(C, NC_SCENE|NA_EDITED, scene); /* so we get view3d redraws */

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
	ot->invoke = ed_markers_opwrap_invoke;
	ot->poll= ed_markers_poll_selected_markers;

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
	WM_operatortype_append(MARKER_OT_rename);
	WM_operatortype_append(MARKER_OT_make_links_scene);
#ifdef DURIAN_CAMERA_SWITCH
	WM_operatortype_append(MARKER_OT_camera_bind);
#endif
}

/* called in screen_ops.c:ED_keymap_screen() */
void ED_marker_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap= WM_keymap_find(keyconf, "Markers", 0, 0);
	wmKeyMapItem *kmi;
	
	WM_keymap_verify_item(keymap, "MARKER_OT_add", MKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_move", EVT_TWEAK_S, KM_ANY, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "MARKER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);

#ifdef DURIAN_CAMERA_SWITCH
	kmi= WM_keymap_add_item(keymap, "MARKER_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	RNA_boolean_set(kmi->ptr, "camera", TRUE);

	kmi= WM_keymap_add_item(keymap, "MARKER_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);
	RNA_boolean_set(kmi->ptr, "camera", TRUE);
#else
	(void)kmi;
#endif
	
	WM_keymap_verify_item(keymap, "MARKER_OT_select_border", BKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_select_all", AKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_delete", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "MARKER_OT_rename", MKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "MARKER_OT_move", GKEY, KM_PRESS, 0, 0);
#ifdef DURIAN_CAMERA_SWITCH
	WM_keymap_add_item(keymap, "MARKER_OT_camera_bind", BKEY, KM_PRESS, KM_CTRL, 0);
#endif
}

/* to be called from animation editor keymaps, see note below */
void ED_marker_keymap_animedit_conflictfree(wmKeyMap *keymap)
{
	/* duplicate of some marker-hotkeys but without the bounds checking
	 * since these are handy to be able to do unrestricted and won't conflict
	 * with primary function hotkeys (Usability tweak [#27469])
	 */
	WM_keymap_add_item(keymap, "MARKER_OT_add", MKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MARKER_OT_rename", MKEY, KM_PRESS, KM_CTRL, 0);
}
