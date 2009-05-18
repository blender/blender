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

#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "DNA_armature_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_multires.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"
#include "BKE_mesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_util.h"
#include "ED_screen.h"
#include "ED_mesh.h"
#include "ED_screen_types.h"

#include "RE_pipeline.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "screen_intern.h"	/* own module include */

/* ************** Exported Poll tests ********************** */

int ED_operator_regionactive(bContext *C)
{
	if(CTX_wm_window(C)==NULL) return 0;
	if(CTX_wm_screen(C)==NULL) return 0;
	if(CTX_wm_region(C)==NULL) return 0;
	return 1;
}

int ED_operator_areaactive(bContext *C)
{
	if(CTX_wm_window(C)==NULL) return 0;
	if(CTX_wm_screen(C)==NULL) return 0;
	if(CTX_wm_area(C)==NULL) return 0;
	return 1;
}

int ED_operator_screenactive(bContext *C)
{
	if(CTX_wm_window(C)==NULL) return 0;
	if(CTX_wm_screen(C)==NULL) return 0;
	return 1;
}

/* when mouse is over area-edge */
int ED_operator_screen_mainwinactive(bContext *C)
{
	if(CTX_wm_window(C)==NULL) return 0;
	if(CTX_wm_screen(C)==NULL) return 0;
	if (CTX_wm_screen(C)->subwinactive!=CTX_wm_screen(C)->mainwin) return 0;
	return 1;
}

int ED_operator_scene_editable(bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	if(scene && scene->id.lib==NULL)
		return 1;
	return 0;
}

static int ed_spacetype_test(bContext *C, int type)
{
	if(ED_operator_areaactive(C)) {
		SpaceLink *sl= (SpaceLink *)CTX_wm_space_data(C);
		return sl && (sl->spacetype == type);
	}
	return 0;
}

int ED_operator_view3d_active(bContext *C)
{
	return ed_spacetype_test(C, SPACE_VIEW3D);
}

int ED_operator_timeline_active(bContext *C)
{
	return ed_spacetype_test(C, SPACE_TIME);
}

int ED_operator_outliner_active(bContext *C)
{
	return ed_spacetype_test(C, SPACE_OUTLINER);
}

int ED_operator_file_active(bContext *C)
{
	return ed_spacetype_test(C, SPACE_FILE);
}

int ED_operator_action_active(bContext *C)
{
	return ed_spacetype_test(C, SPACE_ACTION);
}

int ED_operator_buttons_active(bContext *C)
{
	return ed_spacetype_test(C, SPACE_BUTS);
}

int ED_operator_node_active(bContext *C)
{
	if(ed_spacetype_test(C, SPACE_NODE)) {
		SpaceNode *snode= (SpaceNode *)CTX_wm_space_data(C);
		if(snode->edittree)
			return 1;
	}
	return 0;
}

int ED_operator_ipo_active(bContext *C)
{
	return ed_spacetype_test(C, SPACE_IPO);
}

int ED_operator_sequencer_active(bContext *C)
{
	return ed_spacetype_test(C, SPACE_SEQ);
}

int ED_operator_image_active(bContext *C)
{
	return ed_spacetype_test(C, SPACE_IMAGE);
}

int ED_operator_object_active(bContext *C)
{
	return NULL != CTX_data_active_object(C);
}

int ED_operator_editmesh(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_MESH)
		return NULL != ((Mesh *)obedit->data)->edit_btmesh;
	return 0;
}

int ED_operator_editarmature(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_ARMATURE)
		return NULL != ((bArmature *)obedit->data)->edbo;
	return 0;
}

int ED_operator_posemode(bContext *C)
{
	Object *obact= CTX_data_active_object(C);
	Object *obedit= CTX_data_edit_object(C);
	
	if ((obact != obedit) && (obact) && (obact->type==OB_ARMATURE))
		return (obact->flag & OB_POSEMODE)!=0;
		
	return 0;
}


int ED_operator_uvedit(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= NULL;

	if(obedit && obedit->type==OB_MESH)
		em= EM_GetEditMesh((Mesh *)obedit->data);

	if(em && (em->faces.first) && (CustomData_has_layer(&em->fdata, CD_MTFACE))) {
		EM_EndEditMesh(obedit->data, em);
		return 1;
	}

	EM_EndEditMesh(obedit->data, em);
	return 0;
}

int ED_operator_uvmap(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= NULL;

	if(obedit && obedit->type==OB_MESH)
		em= EM_GetEditMesh((Mesh *)obedit->data);

	if(em && (em->faces.first)) {
		EM_EndEditMesh(obedit->data, em);
		return 1;
	}

	EM_EndEditMesh(obedit->data, em);
	return 0;
}

int ED_operator_editsurfcurve(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && ELEM(obedit->type, OB_CURVE, OB_SURF))
		return NULL != ((Curve *)obedit->data)->editnurb;
	return 0;
}


int ED_operator_editcurve(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_CURVE)
		return NULL != ((Curve *)obedit->data)->editnurb;
	return 0;
}

int ED_operator_editsurf(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_SURF)
		return NULL != ((Curve *)obedit->data)->editnurb;
	return 0;
}

int ED_operator_editfont(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_FONT)
		return NULL != ((Curve *)obedit->data)->editfont;
	return 0;
}

/* *************************** action zone operator ************************** */

/* operator state vars used:  
	none

functions:

	apply() set actionzone event

	exit()	free customdata
	
callbacks:

	exec()	never used

	invoke() check if in zone  
		add customdata, put mouseco and area in it
		add modal handler

	modal()	accept modal events while doing it
		call apply() with gesture info, active window, nonactive window
		call exit() and remove handler when LMB confirm

*/

typedef struct sActionzoneData {
	ScrArea *sa1, *sa2;
	AZone *az;
	int x, y, gesture_dir, modifier;
} sActionzoneData;

/* used by other operators too */
static ScrArea *screen_areahascursor(bScreen *scr, int x, int y)
{
	ScrArea *sa= NULL;
	sa= scr->areabase.first;
	while(sa) {
		if(BLI_in_rcti(&sa->totrct, x, y)) break;
		sa= sa->next;
	}
	
	return sa;
}


AZone *is_in_area_actionzone(ScrArea *sa, int x, int y)
{
	AZone *az= NULL;
	int i= 0;
	
	for(az= sa->actionzones.first, i= 0; az; az= az->next, i++) {
		if(az->type == AZONE_TRI) {
			if(IsPointInTri2DInts(az->x1, az->y1, az->x2, az->y2, x, y)) 
				break;
		}
		if(az->type == AZONE_QUAD) {
			if(az->x1 < x && x < az->x2 && az->y1 < y && y < az->y2) 
				break;
		}
	}
	
	return az;
}

static int actionzone_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	AZone *az= is_in_area_actionzone(CTX_wm_area(C), event->x, event->y);
	sActionzoneData *sad;
	
	/* quick escape */
	if(az==NULL)
		return OPERATOR_PASS_THROUGH;
	
	/* ok we do the actionzone */
	sad= op->customdata= MEM_callocN(sizeof(sActionzoneData), "sActionzoneData");
	sad->sa1= CTX_wm_area(C);
	sad->az= az;
	sad->x= event->x; sad->y= event->y;
	
	/* add modal handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	return OPERATOR_RUNNING_MODAL;
}

static void actionzone_exit(bContext *C, wmOperator *op)
{
	if(op->customdata)
		MEM_freeN(op->customdata);
	op->customdata= NULL;
}

/* send EVT_ACTIONZONE event */
static void actionzone_apply(bContext *C, wmOperator *op)
{
	wmEvent event;
	wmWindow *win= CTX_wm_window(C);
	sActionzoneData *sad= op->customdata;
	sad->modifier= RNA_int_get(op->ptr, "modifier");
	
	event= *(win->eventstate);	/* XXX huh huh? make api call */
	event.type= EVT_ACTIONZONE;
	event.customdata= op->customdata;
	event.customdatafree= TRUE;
	op->customdata= NULL;
	
	wm_event_add(win, &event);
}

static int actionzone_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sActionzoneData *sad= op->customdata;
	int deltax, deltay;
	
	switch(event->type) {
		case MOUSEMOVE:
			/* calculate gesture direction */
			deltax= (event->x - sad->x);
			deltay= (event->y - sad->y);
			
			if(deltay > ABS(deltax))
				sad->gesture_dir= AZONE_N;
			else if(deltax > ABS(deltay))
				sad->gesture_dir= AZONE_E;
			else if(deltay < -ABS(deltax))
				sad->gesture_dir= AZONE_S;
			else
				sad->gesture_dir= AZONE_W;
			
			/* gesture is large enough? */
			if(ABS(deltax) > 12 || ABS(deltay) > 12) {
				
				/* second area, for join */
				sad->sa2= screen_areahascursor(CTX_wm_screen(C), event->x, event->y);
				/* apply sends event */
				actionzone_apply(C, op);
				actionzone_exit(C, op);
				
				return OPERATOR_FINISHED;
			}
				break;
		case ESCKEY:
		case LEFTMOUSE:
			actionzone_exit(C, op);
			return OPERATOR_CANCELLED;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

void SCREEN_OT_actionzone(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Handle area action zones";
	ot->idname= "SCREEN_OT_actionzone";
	
	ot->invoke= actionzone_invoke;
	ot->modal= actionzone_modal;
	
	ot->poll= ED_operator_areaactive;
	RNA_def_int(ot->srna, "modifier", 0, 0, 2, "modifier", "modifier state", 0, 2);
}

/* ************** swap area operator *********************************** */

/* operator state vars used:  
 					sa1		start area
					sa2		area to swap with

	functions:

	init()   set custom data for operator, based on actionzone event custom data

	cancel()	cancel the operator

	exit()	cleanup, send notifier

	callbacks:

	invoke() gets called on shift+lmb drag in actionzone
            call init(), add handler

	modal()  accept modal events while doing it

*/

typedef struct sAreaSwapData {
	ScrArea *sa1, *sa2;
} sAreaSwapData;

static int area_swap_init(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaSwapData *sd= NULL;
	sActionzoneData *sad= event->customdata;

	if(sad==NULL || sad->sa1==NULL)
					return 0;
	
	sd= MEM_callocN(sizeof(sAreaSwapData), "sAreaSwapData");
	sd->sa1= sad->sa1;
	sd->sa2= sad->sa2;
	op->customdata= sd;

	return 1;
}


static void area_swap_exit(bContext *C, wmOperator *op)
{
	if(op->customdata)
		MEM_freeN(op->customdata);
	op->customdata= NULL;
}

static int area_swap_cancel(bContext *C, wmOperator *op)
{
	area_swap_exit(C, op);
	return OPERATOR_CANCELLED;
}

static int area_swap_invoke(bContext *C, wmOperator *op, wmEvent *event)
{

	if(!area_swap_init(C, op, event))
		return OPERATOR_PASS_THROUGH;

	/* add modal handler */
	WM_cursor_modal(CTX_wm_window(C), BC_SWAPAREA_CURSOR);
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	return OPERATOR_RUNNING_MODAL;

}

static int area_swap_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sActionzoneData *sad= op->customdata;

	switch(event->type) {
		case MOUSEMOVE:
			/* second area, for join */
			sad->sa2= screen_areahascursor(CTX_wm_screen(C), event->x, event->y);
			break;
		case LEFTMOUSE: /* release LMB */
			if(event->val==0) {
				if(sad->sa1 == sad->sa2) {

					return area_swap_cancel(C, op);
				}
				ED_area_swapspace(C, sad->sa1, sad->sa2);

				area_swap_exit(C, op);

				WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);

				return OPERATOR_FINISHED;
			}
			break;

		case ESCKEY:
			return area_swap_cancel(C, op);
	}
	return OPERATOR_RUNNING_MODAL;
}

void SCREEN_OT_area_swap(wmOperatorType *ot)
{
	ot->name= "Swap areas";
	ot->idname= "SCREEN_OT_area_swap";

	ot->invoke= area_swap_invoke;
	ot->modal= area_swap_modal;
	ot->poll= ED_operator_areaactive;
}

/* *********** Duplicate area as new window operator ****************** */

/* operator callback */
static int area_dupli_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	wmWindow *newwin, *win;
	bScreen *newsc, *sc;
	ScrArea *sa;
	rcti rect;
	sActionzoneData *sad= event->customdata;

	if(sad==NULL)
		return OPERATOR_PASS_THROUGH;
	
	win= CTX_wm_window(C);
	sc= CTX_wm_screen(C);
	sa= sad->sa1;

	/*  poll() checks area context, but we don't accept full-area windows */
	if(sc->full != SCREENNORMAL) {
		actionzone_exit(C, op);
		return OPERATOR_CANCELLED;
	}
	
	/* adds window to WM */
	rect= sa->totrct;
	BLI_translate_rcti(&rect, win->posx, win->posy);
	newwin= WM_window_open(C, &rect);
	
	/* allocs new screen and adds to newly created window, using window size */
	newsc= screen_add(newwin, CTX_data_scene(C), sc->id.name+2);
	newwin->screen= newsc;
	
	/* copy area to new screen */
	area_copy_data((ScrArea *)newsc->areabase.first, sa, 0);
	
	/* screen, areas init */
	WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);

	actionzone_exit(C, op);
	
	return OPERATOR_FINISHED;
}

void SCREEN_OT_area_dupli(wmOperatorType *ot)
{
	ot->name= "Duplicate Area into New Window";
	ot->idname= "SCREEN_OT_area_dupli";
	
	ot->invoke= area_dupli_invoke;
	ot->poll= ED_operator_areaactive;
}


/* ************** move area edge operator *********************************** */

/* operator state vars used:  
           x, y   			mouse coord near edge
           delta            movement of edge

	functions:

	init()   set default property values, find edge based on mouse coords, test
            if the edge can be moved, select edges, calculate min and max movement

	apply()	apply delta on selection

	exit()	cleanup, send notifier

	cancel() cancel moving

	callbacks:

	exec()   execute without any user interaction, based on properties
            call init(), apply(), exit()

	invoke() gets called on mouse click near edge
            call init(), add handler

	modal()  accept modal events while doing it
			call apply() with delta motion
            call exit() and remove handler

*/

typedef struct sAreaMoveData {
	int bigger, smaller, origval;
	char dir;
} sAreaMoveData;

/* helper call to move area-edge, sets limits */
static void area_move_set_limits(bScreen *sc, int dir, int *bigger, int *smaller)
{
	ScrArea *sa;
	
	/* we check all areas and test for free space with MINSIZE */
	*bigger= *smaller= 100000;
	
	for(sa= sc->areabase.first; sa; sa= sa->next) {
		if(dir=='h') {
			int y1= sa->v2->vec.y - sa->v1->vec.y-AREAMINY;
			
			/* if top or down edge selected, test height */
			if(sa->v1->flag && sa->v4->flag)
				*bigger= MIN2(*bigger, y1);
			else if(sa->v2->flag && sa->v3->flag)
				*smaller= MIN2(*smaller, y1);
		}
		else {
			int x1= sa->v4->vec.x - sa->v1->vec.x-AREAMINX;
			
			/* if left or right edge selected, test width */
			if(sa->v1->flag && sa->v2->flag)
				*bigger= MIN2(*bigger, x1);
			else if(sa->v3->flag && sa->v4->flag)
				*smaller= MIN2(*smaller, x1);
		}
	}
}

/* validate selection inside screen, set variables OK */
/* return 0: init failed */
static int area_move_init (bContext *C, wmOperator *op)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrEdge *actedge;
	sAreaMoveData *md;
	int x, y;

	/* required properties */
	x= RNA_int_get(op->ptr, "x");
	y= RNA_int_get(op->ptr, "y");

	/* setup */
	actedge= screen_find_active_scredge(sc, x, y);
	if(actedge==NULL) return 0;

	md= MEM_callocN(sizeof(sAreaMoveData), "sAreaMoveData");
	op->customdata= md;

	md->dir= scredge_is_horizontal(actedge)?'h':'v';
	if(md->dir=='h') md->origval= actedge->v1->vec.y;
	else md->origval= actedge->v1->vec.x;
	
	select_connected_scredge(sc, actedge);
	/* now all vertices with 'flag==1' are the ones that can be moved. */

	area_move_set_limits(sc, md->dir, &md->bigger, &md->smaller);
	
	return 1;
}

/* moves selected screen edge amount of delta, used by split & move */
static void area_move_apply_do(bContext *C, int origval, int delta, int dir, int bigger, int smaller)
{
	wmWindow *win= CTX_wm_window(C);
	bScreen *sc= CTX_wm_screen(C);
	ScrVert *v1;
	
	delta= CLAMPIS(delta, -smaller, bigger);
	
	for (v1= sc->vertbase.first; v1; v1= v1->next) {
		if (v1->flag) {
			/* that way a nice AREAGRID  */
			if((dir=='v') && v1->vec.x>0 && v1->vec.x<win->sizex-1) {
				v1->vec.x= origval + delta;
				if(delta != bigger && delta != -smaller) v1->vec.x-= (v1->vec.x % AREAGRID);
			}
			if((dir=='h') && v1->vec.y>0 && v1->vec.y<win->sizey-1) {
				v1->vec.y= origval + delta;

				v1->vec.y+= AREAGRID-1;
				v1->vec.y-= (v1->vec.y % AREAGRID);
				
				/* prevent too small top header */
				if(v1->vec.y > win->sizey-AREAMINY)
					v1->vec.y= win->sizey-AREAMINY;
			}
		}
	}

	WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
}

static void area_move_apply(bContext *C, wmOperator *op)
{
	sAreaMoveData *md= op->customdata;
	int delta;
	
	delta= RNA_int_get(op->ptr, "delta");
	area_move_apply_do(C, md->origval, delta, md->dir, md->bigger, md->smaller);
}

static void area_move_exit(bContext *C, wmOperator *op)
{
	if(op->customdata)
		MEM_freeN(op->customdata);
	op->customdata= NULL;
	
	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scrverts(CTX_wm_screen(C));
	removedouble_scredges(CTX_wm_screen(C));
}

static int area_move_exec(bContext *C, wmOperator *op)
{
	if(!area_move_init(C, op))
		return OPERATOR_CANCELLED;
	
	area_move_apply(C, op);
	area_move_exit(C, op);
	
	return OPERATOR_FINISHED;
}

/* interaction callback */
static int area_move_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	RNA_int_set(op->ptr, "x", event->x);
	RNA_int_set(op->ptr, "y", event->y);

	if(!area_move_init(C, op)) 
		return OPERATOR_PASS_THROUGH;
	
	/* add temp handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	return OPERATOR_RUNNING_MODAL;
}

static int area_move_cancel(bContext *C, wmOperator *op)
{

	RNA_int_set(op->ptr, "delta", 0);
	area_move_apply(C, op);
	area_move_exit(C, op);

	return OPERATOR_CANCELLED;
}

/* modal callback for while moving edges */
static int area_move_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaMoveData *md;
	int delta, x, y;

	md= op->customdata;

	x= RNA_int_get(op->ptr, "x");
	y= RNA_int_get(op->ptr, "y");

	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			delta= (md->dir == 'v')? event->x - x: event->y - y;
			RNA_int_set(op->ptr, "delta", delta);

			area_move_apply(C, op);
			break;
			
		case LEFTMOUSE:
			if(event->val==0) {
				area_move_exit(C, op);
				return OPERATOR_FINISHED;
			}
			break;
			
		case ESCKEY:
			return area_move_cancel(C, op);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

void SCREEN_OT_area_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move area edges";
	ot->idname= "SCREEN_OT_area_move";

	ot->exec= area_move_exec;
	ot->invoke= area_move_invoke;
	ot->cancel= area_move_cancel;
	ot->modal= area_move_modal;

	ot->poll= ED_operator_screen_mainwinactive; /* when mouse is over area-edge */

	/* rna */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "delta", 0, INT_MIN, INT_MAX, "Delta", "", INT_MIN, INT_MAX);
}

/* ************** split area operator *********************************** */

/* 
operator state vars:  
	fac              spit point
	dir              direction 'v' or 'h'

operator customdata:
	area   			pointer to (active) area
	x, y			last used mouse pos
	(more, see below)

functions:

	init()   set default property values, find area based on context

	apply()	split area based on state vars

	exit()	cleanup, send notifier

	cancel() remove duplicated area

callbacks:

	exec()   execute without any user interaction, based on state vars
            call init(), apply(), exit()

	invoke() gets called on mouse click in action-widget
            call init(), add modal handler
			call apply() with initial motion

	modal()  accept modal events while doing it
            call move-areas code with delta motion
            call exit() or cancel() and remove handler

*/

#define SPLIT_STARTED	1
#define SPLIT_PROGRESS	2

typedef struct sAreaSplitData
{
	int x, y;	/* last used mouse position */
	
	int origval;			/* for move areas */
	int bigger, smaller;	/* constraints for moving new edge */
	int delta;				/* delta move edge */
	int origmin, origsize;	/* to calculate fac, for property storage */

	ScrEdge *nedge;			/* new edge */
	ScrArea *sarea;			/* start area */
	ScrArea *narea;			/* new area */
} sAreaSplitData;

/* generic init, no UI stuff here */
static int area_split_init(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	sAreaSplitData *sd;
	int dir;
	
	/* required context */
	if(sa==NULL) return 0;
	
	/* required properties */
	dir= RNA_enum_get(op->ptr, "direction");
	
	/* minimal size */
	if(dir=='v' && sa->winx < 2*AREAMINX) return 0;
	if(dir=='h' && sa->winy < 2*AREAMINY) return 0;
	   
	/* custom data */
	sd= (sAreaSplitData*)MEM_callocN(sizeof (sAreaSplitData), "op_area_split");
	op->customdata= sd;
	
	sd->sarea= sa;
	sd->origsize= dir=='v' ? sa->winx:sa->winy;
	sd->origmin = dir=='v' ? sa->totrct.xmin:sa->totrct.ymin;
	
	return 1;
}

/* with sa as center, sb is located at: 0=W, 1=N, 2=E, 3=S */
/* used with split operator */
static ScrEdge *area_findsharededge(bScreen *screen, ScrArea *sa, ScrArea *sb)
{
	ScrVert *sav1= sa->v1;
	ScrVert *sav2= sa->v2;
	ScrVert *sav3= sa->v3;
	ScrVert *sav4= sa->v4;
	ScrVert *sbv1= sb->v1;
	ScrVert *sbv2= sb->v2;
	ScrVert *sbv3= sb->v3;
	ScrVert *sbv4= sb->v4;
	
	if(sav1==sbv4 && sav2==sbv3) { /* sa to right of sb = W */
		return screen_findedge(screen, sav1, sav2);
	}
	else if(sav2==sbv1 && sav3==sbv4) { /* sa to bottom of sb = N */
		return screen_findedge(screen, sav2, sav3);
	}
	else if(sav3==sbv2 && sav4==sbv1) { /* sa to left of sb = E */
		return screen_findedge(screen, sav3, sav4);
	}
	else if(sav1==sbv2 && sav4==sbv3) { /* sa on top of sb = S*/
		return screen_findedge(screen, sav1, sav4);
	}

	return NULL;
}


/* do the split, return success */
static int area_split_apply(bContext *C, wmOperator *op)
{
	bScreen *sc= CTX_wm_screen(C);
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;
	float fac;
	int dir;
	
	fac= RNA_float_get(op->ptr, "factor");
	dir= RNA_enum_get(op->ptr, "direction");

	sd->narea= area_split(CTX_wm_window(C), sc, sd->sarea, dir, fac);
	
	if(sd->narea) {
		ScrVert *sv;
		
		sd->nedge= area_findsharededge(sc, sd->sarea, sd->narea);
	
		/* select newly created edge, prepare for moving edge */
		for(sv= sc->vertbase.first; sv; sv= sv->next)
			sv->flag = 0;
		
		sd->nedge->v1->flag= 1;
		sd->nedge->v2->flag= 1;

		if(dir=='h') sd->origval= sd->nedge->v1->vec.y;
		else sd->origval= sd->nedge->v1->vec.x;

		WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
		
		return 1;
	}		
	
	return 0;
}

static void area_split_exit(bContext *C, wmOperator *op)
{
	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}
	
	WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);

	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scrverts(CTX_wm_screen(C));
	removedouble_scredges(CTX_wm_screen(C));
}


/* UI callback, adds new handler */
static int area_split_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaSplitData *sd;
	
	if(event->type==EVT_ACTIONZONE) {
		sActionzoneData *sad= event->customdata;
		int dir;

		if(sad->modifier>0) {
			return OPERATOR_PASS_THROUGH;
		}
		
		/* verify *sad itself */
		if(sad==NULL || sad->sa1==NULL || sad->az==NULL)
			return OPERATOR_PASS_THROUGH;
		
		/* is this our *sad? if areas not equal it should be passed on */
		if(CTX_wm_area(C)!=sad->sa1 || sad->sa1!=sad->sa2)
			return OPERATOR_PASS_THROUGH;
		
		/* prepare operator state vars */
		if(sad->gesture_dir==AZONE_N || sad->gesture_dir==AZONE_S) {
			dir= 'h';
			RNA_float_set(op->ptr, "factor", ((float)(event->x - sad->sa1->v1->vec.x)) / (float)sad->sa1->winx);
		}
		else {
			dir= 'v';
			RNA_float_set(op->ptr, "factor", ((float)(event->y - sad->sa1->v1->vec.y)) / (float)sad->sa1->winy);
		}
		RNA_enum_set(op->ptr, "direction", dir);

		/* general init, also non-UI case, adds customdata, sets area and defaults */
		if(!area_split_init(C, op))
			return OPERATOR_PASS_THROUGH;
		
		sd= (sAreaSplitData *)op->customdata;
		
		sd->x= event->x;
		sd->y= event->y;
		
		/* do the split */
		if(area_split_apply(C, op)) {
			area_move_set_limits(CTX_wm_screen(C), dir, &sd->bigger, &sd->smaller);
			
			/* add temp handler for edge move or cancel */
			WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
			
			return OPERATOR_RUNNING_MODAL;
		}
		
	}
	else {
		/* nonmodal for now */
		return op->type->exec(C, op);
	}
	
	return OPERATOR_PASS_THROUGH;
}

/* function to be called outside UI context, or for redo */
static int area_split_exec(bContext *C, wmOperator *op)
{
	
	if(!area_split_init(C, op))
		return OPERATOR_CANCELLED;
	
	area_split_apply(C, op);
	area_split_exit(C, op);
	
	return OPERATOR_FINISHED;
}


static int area_split_cancel(bContext *C, wmOperator *op)
{
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;

	if (screen_area_join(C, CTX_wm_screen(C), sd->sarea, sd->narea)) {
		if (CTX_wm_area(C) == sd->narea) {
			CTX_wm_area_set(C, NULL);
			CTX_wm_region_set(C, NULL);
		}
		sd->narea = NULL;
	}
	area_split_exit(C, op);

	return OPERATOR_CANCELLED;
}

static int area_split_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;
	float fac;
	int dir;

	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			dir= RNA_enum_get(op->ptr, "direction");
			
			sd->delta= (dir == 'v')? event->x - sd->origval: event->y - sd->origval;
			area_move_apply_do(C, sd->origval, sd->delta, dir, sd->bigger, sd->smaller);
			
			fac= (dir == 'v') ? event->x-sd->origmin : event->y-sd->origmin;
			RNA_float_set(op->ptr, "factor", fac / (float)sd->origsize);
			
			WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
			break;
			
		case LEFTMOUSE:
			if(event->val==0) { /* mouse up */
				area_split_exit(C, op);
				return OPERATOR_FINISHED;
			}
			break;
		case RIGHTMOUSE: /* cancel operation */
		case ESCKEY:
			return area_split_cancel(C, op);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

static EnumPropertyItem prop_direction_items[] = {
	{'h', "HORIZONTAL", "Horizontal", ""},
	{'v', "VERTICAL", "Vertical", ""},
	{0, NULL, NULL, NULL}};

void SCREEN_OT_area_split(wmOperatorType *ot)
{
	ot->name = "Split area";
	ot->idname = "SCREEN_OT_area_split";
	
	ot->exec= area_split_exec;
	ot->invoke= area_split_invoke;
	ot->modal= area_split_modal;
	
	ot->poll= ED_operator_areaactive;
	ot->flag= OPTYPE_REGISTER;
	
	/* rna */
	RNA_def_enum(ot->srna, "direction", prop_direction_items, 'h', "Direction", "");
	RNA_def_float(ot->srna, "factor", 0.5f, 0.0, 1.0, "Factor", "", 0.0, 1.0);
}

/* ************** frame change operator ***************************** */


/* function to be called outside UI context, or for redo */
static int frame_offset_exec(bContext *C, wmOperator *op)
{
	int delta;

	delta = RNA_int_get(op->ptr, "delta");

	CTX_data_scene(C)->r.cfra += delta;

	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

void SCREEN_OT_frame_offset(wmOperatorType *ot)
{
	ot->name = "Frame Offset";
	ot->idname = "SCREEN_OT_frame_offset";

	ot->exec= frame_offset_exec;

	ot->poll= ED_operator_screenactive;
	ot->flag= 0;

	/* rna */
	RNA_def_int(ot->srna, "delta", 0, INT_MIN, INT_MAX, "Delta", "", INT_MIN, INT_MAX);
}

/* ************** switch screen operator ***************************** */


/* function to be called outside UI context, or for redo */
static int screen_set_exec(bContext *C, wmOperator *op)
{
	bScreen *screen= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	int tot= BLI_countlist(&CTX_data_main(C)->screen);
	int delta= RNA_int_get(op->ptr, "delta");
	
	/* this screen is 'fake', solve later XXX */
	if(sa && sa->full)
		return OPERATOR_CANCELLED;
	
	if(delta==1) {
		while(tot--) {
			screen= screen->id.next;
			if(screen==NULL) screen= CTX_data_main(C)->screen.first;
			if(screen->winid==0 && screen->full==0)
				break;
		}
	}
	else if(delta== -1) {
		while(tot--) {
			screen= screen->id.prev;
			if(screen==NULL) screen= CTX_data_main(C)->screen.last;
			if(screen->winid==0 && screen->full==0)
				break;
		}
	}
	else {
		screen= NULL;
	}
	
	if(screen) {
		ED_screen_set(C, screen);
		return OPERATOR_FINISHED;
	}
	return OPERATOR_CANCELLED;
}

void SCREEN_OT_screen_set(wmOperatorType *ot)
{
	ot->name = "Set Screen";
	ot->idname = "SCREEN_OT_screen_set";
	
	ot->exec= screen_set_exec;
	ot->poll= ED_operator_screenactive;
	
	/* rna */
	RNA_def_pointer_runtime(ot->srna, "screen", &RNA_Screen, "Screen", "");
	RNA_def_int(ot->srna, "delta", 0, INT_MIN, INT_MAX, "Delta", "", INT_MIN, INT_MAX);
}

/* ************** screen full-area operator ***************************** */


/* function to be called outside UI context, or for redo */
static int screen_full_area_exec(bContext *C, wmOperator *op)
{
	ed_screen_fullarea(C, CTX_wm_area(C));
	return OPERATOR_FINISHED;
}

void SCREEN_OT_screen_full_area(wmOperatorType *ot)
{
	ot->name = "Toggle Make Area Fullscreen";
	ot->idname = "SCREEN_OT_screen_full_area";
	
	ot->exec= screen_full_area_exec;
	ot->poll= ED_operator_areaactive;
	ot->flag= 0;

}



/* ************** join area operator ********************************************** */

/* operator state vars used:  
			x1, y1     mouse coord in first area, which will disappear
			x2, y2     mouse coord in 2nd area, which will become joined

functions:

   init()   find edge based on state vars 
			test if the edge divides two areas, 
			store active and nonactive area,
            
   apply()  do the actual join

   exit()	cleanup, send notifier

callbacks:

   exec()	calls init, apply, exit 
   
   invoke() sets mouse coords in x,y
            call init()
            add modal handler

   modal()	accept modal events while doing it
			call apply() with active window and nonactive window
            call exit() and remove handler when LMB confirm

*/

typedef struct sAreaJoinData
{
	ScrArea *sa1;	/* first area to be considered */
	ScrArea *sa2;	/* second area to be considered */
	ScrArea *scr;	/* designed for removal */

} sAreaJoinData;


/* validate selection inside screen, set variables OK */
/* return 0: init failed */
/* XXX todo: find edge based on (x,y) and set other area? */
static int area_join_init(bContext *C, wmOperator *op)
{
	ScrArea *sa1, *sa2;
	sAreaJoinData* jd= NULL;
	int x1, y1;
	int x2, y2;

	/* required properties, make negative to get return 0 if not set by caller */
	x1= RNA_int_get(op->ptr, "x1");
	y1= RNA_int_get(op->ptr, "y1");
	x2= RNA_int_get(op->ptr, "x2");
	y2= RNA_int_get(op->ptr, "y2");
	
	sa1 = screen_areahascursor(CTX_wm_screen(C), x1, y1);
	sa2 = screen_areahascursor(CTX_wm_screen(C), x2, y2);
	if(sa1==NULL || sa2==NULL || sa1==sa2)
		return 0;

	jd = (sAreaJoinData*)MEM_callocN(sizeof (sAreaJoinData), "op_area_join");
		
	jd->sa1 = sa1;
	jd->sa1->flag |= AREA_FLAG_DRAWJOINFROM;
	jd->sa2 = sa2;
	jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
	
	op->customdata= jd;
	
	return 1;
}

/* apply the join of the areas (space types) */
static int area_join_apply(bContext *C, wmOperator *op)
{
	sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
	if (!jd) return 0;

	if(!screen_area_join(C, CTX_wm_screen(C), jd->sa1, jd->sa2)){
		return 0;
	}
	if (CTX_wm_area(C) == jd->sa2) {
		CTX_wm_area_set(C, NULL);
		CTX_wm_region_set(C, NULL);
	}

	return 1;
}

/* finish operation */
static void area_join_exit(bContext *C, wmOperator *op)
{
	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}

	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scredges(CTX_wm_screen(C));
	removenotused_scredges(CTX_wm_screen(C));
	removenotused_scrverts(CTX_wm_screen(C));
}

static int area_join_exec(bContext *C, wmOperator *op)
{
	if(!area_join_init(C, op)) 
		return OPERATOR_CANCELLED;
	
	area_join_apply(C, op);
	area_join_exit(C, op);

	return OPERATOR_FINISHED;
}

/* interaction callback */
static int area_join_invoke(bContext *C, wmOperator *op, wmEvent *event)
{

	if(event->type==EVT_ACTIONZONE) {
		sActionzoneData *sad= event->customdata;

		if(sad->modifier>0) {
			return OPERATOR_PASS_THROUGH;
		}
		
		/* verify *sad itself */
		if(sad==NULL || sad->sa1==NULL || sad->sa2==NULL)
			return OPERATOR_PASS_THROUGH;
		
		/* is this our *sad? if areas equal it should be passed on */
		if(sad->sa1==sad->sa2)
			return OPERATOR_PASS_THROUGH;
		
		/* prepare operator state vars */
		RNA_int_set(op->ptr, "x1", sad->x);
		RNA_int_set(op->ptr, "y1", sad->y);
		RNA_int_set(op->ptr, "x2", event->x);
		RNA_int_set(op->ptr, "y2", event->y);

		if(!area_join_init(C, op)) 
			return OPERATOR_PASS_THROUGH;
	
		/* add temp handler */
		WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
		return OPERATOR_RUNNING_MODAL;
	}
	
	return OPERATOR_PASS_THROUGH;
}

static int area_join_cancel(bContext *C, wmOperator *op)
{
	sAreaJoinData *jd = (sAreaJoinData *)op->customdata;

	if (jd->sa1) {
		jd->sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
		jd->sa1->flag &= ~AREA_FLAG_DRAWJOINTO;
	}
	if (jd->sa2) {
		jd->sa2->flag &= ~AREA_FLAG_DRAWJOINFROM;
		jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
	}

	WM_event_add_notifier(C, NC_WINDOW, NULL);
	
	area_join_exit(C, op);

	return OPERATOR_CANCELLED;
}

/* modal callback while selecting area (space) that will be removed */
static int area_join_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	bScreen *sc= CTX_wm_screen(C);
	sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
	
	/* execute the events */
	switch(event->type) {
			
		case MOUSEMOVE: 
			{
				ScrArea *sa = screen_areahascursor(sc, event->x, event->y);
				int dir;
				
				if (sa) {					
					if (jd->sa1 != sa) {
						dir = area_getorientation(sc, jd->sa1, sa);
						if (dir >= 0) {
							if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
							jd->sa2 = sa;
							jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
						} 
						else {
							/* we are not bordering on the previously selected area 
							   we check if area has common border with the one marked for removal
							   in this case we can swap areas.
							*/
							dir = area_getorientation(sc, sa, jd->sa2);
							if (dir >= 0) {
								if (jd->sa1) jd->sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
								if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
								jd->sa1 = jd->sa2;
								jd->sa2 = sa;
								if (jd->sa1) jd->sa1->flag |= AREA_FLAG_DRAWJOINFROM;
								if (jd->sa2) jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
							} 
							else {
								if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
								jd->sa2 = NULL;
							}
						}
						WM_event_add_notifier(C, NC_WINDOW, NULL);
					} 
					else {
						/* we are back in the area previously selected for keeping 
						 * we swap the areas if possible to allow user to choose */
						if (jd->sa2 != NULL) {
							if (jd->sa1) jd->sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
							if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
							jd->sa1 = jd->sa2;
							jd->sa2 = sa;
							if (jd->sa1) jd->sa1->flag |= AREA_FLAG_DRAWJOINFROM;
							if (jd->sa2) jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
							dir = area_getorientation(sc, jd->sa1, jd->sa2);
							if (dir < 0) {
								printf("oops, didn't expect that!\n");
							}
						} 
						else {
							dir = area_getorientation(sc, jd->sa1, sa);
							if (dir >= 0) {
								if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
								jd->sa2 = sa;
								jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
							}
						}
						WM_event_add_notifier(C, NC_WINDOW, NULL);
					}
				}
			}
			break;
		case LEFTMOUSE:
			if(event->val==0) {
				area_join_apply(C, op);
				WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
				area_join_exit(C, op);
				return OPERATOR_FINISHED;
			}
			break;
			
		case ESCKEY:
			return area_join_cancel(C, op);
	}

	return OPERATOR_RUNNING_MODAL;
}

/* Operator for joining two areas (space types) */
void SCREEN_OT_area_join(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Join area";
	ot->idname= "SCREEN_OT_area_join";
	
	/* api callbacks */
	ot->exec= area_join_exec;
	ot->invoke= area_join_invoke;
	ot->modal= area_join_modal;

	ot->poll= ED_operator_areaactive;

	/* rna */
	RNA_def_int(ot->srna, "x1", -100, INT_MIN, INT_MAX, "X 1", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y1", -100, INT_MIN, INT_MAX, "Y 1", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "x2", -100, INT_MIN, INT_MAX, "X 2", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y2", -100, INT_MIN, INT_MAX, "Y 2", "", INT_MIN, INT_MAX);
}

/* ************** repeat last operator ***************************** */

static int repeat_last_exec(bContext *C, wmOperator *op)
{
	wmOperator *lastop= CTX_wm_manager(C)->operators.last;
	
	if(lastop)
		WM_operator_repeat(C, lastop);
	
	return OPERATOR_CANCELLED;
}

void SCREEN_OT_repeat_last(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Repeat Last";
	ot->idname= "SCREEN_OT_repeat_last";
	
	/* api callbacks */
	ot->exec= repeat_last_exec;
	
	ot->poll= ED_operator_screenactive;
	
}

static int repeat_history_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmOperator *lastop;
	uiMenuItem *head;
	int items, i;
	
	items= BLI_countlist(&wm->operators);
	if(items==0)
		return OPERATOR_CANCELLED;
	
	head= uiPupMenuBegin(op->type->name, 0);

	for (i=items-1, lastop= wm->operators.last; lastop; lastop= lastop->prev, i--)
		uiMenuItemIntO(head, lastop->type->name, 0, op->type->idname, "index", i);

	uiPupMenuEnd(C, head);
	
	return OPERATOR_CANCELLED;
}

static int repeat_history_exec(bContext *C, wmOperator *op)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	
	op= BLI_findlink(&wm->operators, RNA_int_get(op->ptr, "index"));
	if(op) {
		/* let's put it as last operator in list */
		BLI_remlink(&wm->operators, op);
		BLI_addtail(&wm->operators, op);
		
		WM_operator_repeat(C, op);
	}
					 
	return OPERATOR_FINISHED;
}

void SCREEN_OT_repeat_history(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Repeat History";
	ot->idname= "SCREEN_OT_repeat_history";
	
	/* api callbacks */
	ot->invoke= repeat_history_invoke;
	ot->exec= repeat_history_exec;
	
	ot->poll= ED_operator_screenactive;
	
	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/* ********************** redo operator ***************************** */

static void redo_last_cb(bContext *C, void *arg_op, void *arg2)
{
	wmOperator *lastop= arg_op;
	
	if(lastop) {
		ED_undo_pop(C);
		WM_operator_repeat(C, lastop);
	}
	
}

static uiBlock *ui_block_create_redo_last(bContext *C, ARegion *ar, void *arg_op)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmOperator *op= arg_op;
	PointerRNA ptr;
	uiBlock *block;
	int height;
	
	block= uiBeginBlock(C, ar, "redo_last_popup", UI_EMBOSS, UI_HELV);
	uiBlockClearFlag(block, UI_BLOCK_LOOP);
	uiBlockSetFlag(block, UI_BLOCK_KEEP_OPEN|UI_BLOCK_RET_1);
	uiBlockSetFunc(block, redo_last_cb, arg_op, NULL);

	if(!op->properties) {
		IDPropertyTemplate val = {0};
		op->properties= IDP_New(IDP_GROUP, val, "wmOperatorProperties");
	}

	RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);
	height= uiDefAutoButsRNA(C, block, &ptr);

	uiPopupBoundsBlock(block, 4.0f, 0, 0);
	uiEndBlock(C, block);

	return block;
}

static int redo_last_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmOperator *lastop;

	/* only for operators that are registered and did an undo push */
	for(lastop= wm->operators.last; lastop; lastop= lastop->prev)
		if((lastop->type->flag & OPTYPE_REGISTER) && (lastop->type->flag & OPTYPE_UNDO))
			break;
	
	if(!lastop)
		return OPERATOR_CANCELLED;

	uiPupBlock(C, ui_block_create_redo_last, lastop);

	return OPERATOR_CANCELLED;
}

void SCREEN_OT_redo_last(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Redo Last";
	ot->idname= "SCREEN_OT_redo_last";
	
	/* api callbacks */
	ot->invoke= redo_last_invoke;
	
	ot->poll= ED_operator_screenactive;
}

/* ************** region split operator ***************************** */

/* insert a region in the area region list */
static int region_split_exec(bContext *C, wmOperator *op)
{
	ARegion *ar= CTX_wm_region(C);
	
	if(ar->regiontype==RGN_TYPE_HEADER)
		BKE_report(op->reports, RPT_ERROR, "Cannot split header");
	else if(ar->alignment==RGN_ALIGN_QSPLIT)
		BKE_report(op->reports, RPT_ERROR, "Cannot split further");
	else {
		ScrArea *sa= CTX_wm_area(C);
		ARegion *newar= BKE_area_region_copy(sa->type, ar);
		int dir= RNA_enum_get(op->ptr, "type");
	
		BLI_insertlinkafter(&sa->regionbase, ar, newar);
		
		newar->alignment= ar->alignment;
		
		if(dir=='h')
			ar->alignment= RGN_ALIGN_HSPLIT;
		else
			ar->alignment= RGN_ALIGN_VSPLIT;
		
		WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
	}
	
	return OPERATOR_FINISHED;
}

void SCREEN_OT_region_split(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Split Region";
	ot->idname= "SCREEN_OT_region_split";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= region_split_exec;
	ot->poll= ED_operator_areaactive;
	
	RNA_def_enum(ot->srna, "type", prop_direction_items, 'h', "Direction", "");
}

/* ************** region four-split operator ***************************** */

/* insert a region in the area region list */
static int region_foursplit_exec(bContext *C, wmOperator *op)
{
	ARegion *ar= CTX_wm_region(C);
	
	/* some rules... */
	if(ar->regiontype!=RGN_TYPE_WINDOW)
		BKE_report(op->reports, RPT_ERROR, "Only window region can be 4-splitted");
	else if(ar->alignment==RGN_ALIGN_QSPLIT) {
		ScrArea *sa= CTX_wm_area(C);
		ARegion *arn;
		
		/* keep current region */
		ar->alignment= 0;
		
		if(sa->spacetype==SPACE_VIEW3D) {
			RegionView3D *rv3d= ar->regiondata;
			rv3d->viewlock= 0;
			rv3d->rflag &= ~RV3D_CLIPPING;
		}
		
		for(ar= sa->regionbase.first; ar; ar= arn) {
			arn= ar->next;
			if(ar->alignment==RGN_ALIGN_QSPLIT) {
				ED_region_exit(C, ar);
				BKE_area_region_free(sa->type, ar);
				BLI_remlink(&sa->regionbase, ar);
				MEM_freeN(ar);
			}
		}
		WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
	}
	else if(ar->next)
		BKE_report(op->reports, RPT_ERROR, "Only last region can be 4-splitted");
	else {
		ScrArea *sa= CTX_wm_area(C);
		ARegion *newar;
		int count;
		
		ar->alignment= RGN_ALIGN_QSPLIT;
		
		for(count=0; count<3; count++) {
			newar= BKE_area_region_copy(sa->type, ar);
			BLI_addtail(&sa->regionbase, newar);
		}
		
		/* lock views and set them */
		if(sa->spacetype==SPACE_VIEW3D) {
			RegionView3D *rv3d;
			
			rv3d= ar->regiondata;
			rv3d->viewlock= RV3D_LOCKED; rv3d->view= V3D_VIEW_FRONT; rv3d->persp= V3D_ORTHO;
			
			ar= ar->next;
			rv3d= ar->regiondata;
			rv3d->viewlock= RV3D_LOCKED; rv3d->view= V3D_VIEW_TOP; rv3d->persp= V3D_ORTHO;
			
			ar= ar->next;
			rv3d= ar->regiondata;
			rv3d->viewlock= RV3D_LOCKED; rv3d->view= V3D_VIEW_RIGHT; rv3d->persp= V3D_ORTHO;
			
			ar= ar->next;
			rv3d= ar->regiondata;
			rv3d->view= V3D_VIEW_CAMERA; rv3d->persp= V3D_CAMOB;
		}
		
		WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
	}
	
	
	return OPERATOR_FINISHED;
}

void SCREEN_OT_region_foursplit(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Split Region in 4 Parts";
	ot->idname= "SCREEN_OT_region_foursplit";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= region_foursplit_exec;
	ot->poll= ED_operator_areaactive;
	ot->flag= OPTYPE_REGISTER;
}



/* ************** region flip operator ***************************** */

/* flip a region alignment */
static int region_flip_exec(bContext *C, wmOperator *op)
{
	ARegion *ar= CTX_wm_region(C);

	if(ar->alignment==RGN_ALIGN_TOP)
		ar->alignment= RGN_ALIGN_BOTTOM;
	else if(ar->alignment==RGN_ALIGN_BOTTOM)
		ar->alignment= RGN_ALIGN_TOP;
	else if(ar->alignment==RGN_ALIGN_LEFT)
		ar->alignment= RGN_ALIGN_RIGHT;
	else if(ar->alignment==RGN_ALIGN_RIGHT)
		ar->alignment= RGN_ALIGN_LEFT;
	
	WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
	printf("executed region flip\n");
	
	return OPERATOR_FINISHED;
}

static void testfunc(bContext *C, void *argv, int arg)
{
	printf("arg %d\n", arg);
}

static void newlevel1(bContext *C, uiMenuItem *head, void *arg)
{
	uiMenuFunc(head, testfunc, NULL);
	
	uiMenuItemVal(head, "First", ICON_PROP_ON, 1);
	uiMenuItemVal(head, "Second", ICON_PROP_CON, 2);
	uiMenuItemVal(head, "Third", ICON_SMOOTHCURVE, 3);
	uiMenuItemVal(head, "Fourth", ICON_SHARPCURVE, 4);	
}

static int testing123(bContext *C, wmOperator *op, wmEvent *event)
{
	uiMenuItem *head= uiPupMenuBegin("Hello world", 0);
	
	uiMenuContext(head, WM_OP_EXEC_DEFAULT);
	uiMenuItemO(head, ICON_PROP_ON, "SCREEN_OT_region_flip");
	uiMenuItemO(head, ICON_PROP_CON, "SCREEN_OT_screen_full_area");
	uiMenuItemO(head, ICON_SMOOTHCURVE, "SCREEN_OT_region_foursplit");
	uiMenuLevel(head, "Submenu", newlevel1);
	
	uiPupMenuEnd(C, head);
	
	/* this operator is only for a menu, not used further */
	return OPERATOR_CANCELLED;
}

void SCREEN_OT_region_flip(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Flip Region";
	ot->idname= "SCREEN_OT_region_flip";
	
	/* api callbacks */
	ot->invoke= testing123; // XXX WM_operator_confirm;
	ot->exec= region_flip_exec;
	
	ot->poll= ED_operator_areaactive;
	ot->flag= OPTYPE_REGISTER;
	
	RNA_def_int(ot->srna, "test", 0, INT_MIN, INT_MAX, "test", "", INT_MIN, INT_MAX);

}

/* ****************** anim player, typically with timer ***************** */

static int screen_animation_play(bContext *C, wmOperator *op, wmEvent *event)
{
	bScreen *screen= CTX_wm_screen(C);
	
	if(screen->animtimer==event->customdata) {
		Scene *scene= CTX_data_scene(C);
		
		if(scene->audio.flag & AUDIO_SYNC) {
			wmTimer *wt= screen->animtimer;
			int step = floor(wt->duration * FPS);
			scene->r.cfra += step;
			wt->duration -= ((float)step)/FPS;
		}
		else
			scene->r.cfra++;
		
		if (scene->r.psfra) {
			if(scene->r.cfra > scene->r.pefra)
				scene->r.cfra= scene->r.psfra;
		}
		else {
			if(scene->r.cfra > scene->r.efra)
				scene->r.cfra= scene->r.sfra;
		}

		WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
		
		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

void SCREEN_OT_animation_play(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Animation player";
	ot->idname= "SCREEN_OT_animation_play";
	
	/* api callbacks */
	ot->invoke= screen_animation_play;
	
	ot->poll= ED_operator_screenactive;
	
}

/* ************** border select operator (template) ***************************** */

/* operator state vars used: (added by default WM callbacks)   
	xmin, ymin     
	xmax, ymax     

	customdata: the wmGesture pointer

callbacks:

	exec()	has to be filled in by user

	invoke() default WM function
			 adds modal handler

	modal()	default WM function 
			accept modal events while doing it, calls exec(), handles ESC and border drawing
	
	poll()	has to be filled in by user for context
*/
#if 0
static int border_select_do(bContext *C, wmOperator *op)
{
	int event_type= RNA_int_get(op->ptr, "event_type");
	
	if(event_type==LEFTMOUSE)
		printf("border select do select\n");
	else if(event_type==RIGHTMOUSE)
		printf("border select deselect\n");
	else 
		printf("border select do something\n");
	
	return 1;
}

void SCREEN_OT_border_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border select";
	ot->idname= "SCREEN_OT_border_select";
	
	/* api callbacks */
	ot->exec= border_select_do;
	ot->invoke= WM_border_select_invoke;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_areaactive;
	
	/* rna */
	RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);

}
#endif

/* ****************************** render invoking ***************** */

/* set callbacks, exported to sequence render too. 
Only call in foreground (UI) renders. */

/* returns biggest area that is not uv/image editor. Note that it uses buttons */
/* window as the last possible alternative.									   */
static ScrArea *biggest_non_image_area(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa, *big= NULL;
	int size, maxsize= 0, bwmaxsize= 0;
	short foundwin= 0;
	
	for(sa= sc->areabase.first; sa; sa= sa->next) {
		if(sa->winx > 10 && sa->winy > 10) {
			size= sa->winx*sa->winy;
			if(sa->spacetype == SPACE_BUTS) {
				if(foundwin == 0 && size > bwmaxsize) {
					bwmaxsize= size;
					big= sa;	
				}
			}
			else if(sa->spacetype != SPACE_IMAGE && size > maxsize) {
				maxsize= size;
				big= sa;
				foundwin= 1;
			}
		}
	}
	
	return big;
}

static ScrArea *biggest_area(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa, *big= NULL;
	int size, maxsize= 0;
	
	for(sa= sc->areabase.first; sa; sa= sa->next) {
		size= sa->winx*sa->winy;
		if(size > maxsize) {
			maxsize= size;
			big= sa;
		}
	}
	return big;
}


static ScrArea *find_area_showing_r_result(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa;
	SpaceImage *sima;
	
	/* find an imagewindow showing render result */
	for(sa=sc->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_IMAGE) {
			sima= sa->spacedata.first;
			if(sima->image && sima->image->type==IMA_TYPE_R_RESULT)
				break;
		}
	}
	return sa;
}

static void screen_set_image_output(bContext *C)
{
	ScrArea *sa;
	SpaceImage *sima;
	
	sa= find_area_showing_r_result(C);
	
	if(sa==NULL) {
		/* find largest open non-image area */
		sa= biggest_non_image_area(C);
		if(sa) {
			ED_area_newspace(C, sa, SPACE_IMAGE);
			sima= sa->spacedata.first;
			
			/* makes ESC go back to prev space */
			sima->flag |= SI_PREVSPACE;
		}
		else {
			/* use any area of decent size */
			sa= biggest_area(C);
			if(sa->spacetype!=SPACE_IMAGE) {
				// XXX newspace(sa, SPACE_IMAGE);
				sima= sa->spacedata.first;
				
				/* makes ESC go back to prev space */
				sima->flag |= SI_PREVSPACE;
			}
		}
	}
	
	sima= sa->spacedata.first;
	
	/* get the correct image, and scale it */
	sima->image= BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
	
	if(G.displaymode==2) { // XXX
		if(sa->full==0) {
			sima->flag |= SI_FULLWINDOW;
			
			ed_screen_fullarea(C, sa);
		}
	}
	
}

/* executes blocking render */
static int screen_render_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Render *re= RE_GetRender(scene->id.name);
	
	if(re==NULL) {
		re= RE_NewRender(scene->id.name);
	}
	RE_test_break_cb(re, NULL, (int (*)(void *)) blender_test_break);
	
	if(RNA_boolean_get(op->ptr, "anim"))
		RE_BlenderAnim(re, scene, scene->r.sfra, scene->r.efra, scene->frame_step);
	else
		RE_BlenderFrame(re, scene, scene->r.cfra);
	
	// no redraw needed, we leave state as we entered it
	ED_update_for_newframe(C, 1);
	
	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_RESULT, scene);

	return OPERATOR_FINISHED;
}

typedef struct RenderJob {
	Scene *scene;
	Render *re;
	wmWindow *win;
	int anim;
	Image *image;
	ImageUser iuser;
	short *stop;
	short *do_update;
} RenderJob;

static void render_freejob(void *rjv)
{
	RenderJob *rj= rjv;
	
	MEM_freeN(rj);
}

/* called inside thread! */
static void image_rect_update(void *rjv, RenderResult *rr, volatile rcti *renrect)
{
	RenderJob *rj= rjv;
	ImBuf *ibuf;
	float x1, y1, *rectf= NULL;
	int ymin, ymax, xmin, xmax;
	int rymin, rxmin;
	char *rectc;
	
	ibuf= BKE_image_get_ibuf(rj->image, &rj->iuser);
	if(ibuf==NULL) return;

	/* if renrect argument, we only refresh scanlines */
	if(renrect) {
		/* if ymax==recty, rendering of layer is ready, we should not draw, other things happen... */
		if(rr->renlay==NULL || renrect->ymax>=rr->recty)
			return;
		
		/* xmin here is first subrect x coord, xmax defines subrect width */
		xmin = renrect->xmin + rr->crop;
		xmax = renrect->xmax - xmin - rr->crop;
		if (xmax<2) return;
		
		ymin= renrect->ymin + rr->crop;
		ymax= renrect->ymax - ymin - rr->crop;
		if(ymax<2)
			return;
		renrect->ymin= renrect->ymax;
		
	}
	else {
		xmin = ymin = rr->crop;
		xmax = rr->rectx - 2*rr->crop;
		ymax = rr->recty - 2*rr->crop;
	}
	
	/* xmin ymin is in tile coords. transform to ibuf */
	rxmin= rr->tilerect.xmin + xmin;
	if(rxmin >= ibuf->x) return;
	rymin= rr->tilerect.ymin + ymin;
	if(rymin >= ibuf->y) return;
	
	if(rxmin + xmax > ibuf->x)
		xmax= ibuf->x - rxmin;
	if(rymin + ymax > ibuf->y)
		ymax= ibuf->y - rymin;
	
	if(xmax < 1 || ymax < 1) return;
	
	/* find current float rect for display, first case is after composit... still weak */
	if(rr->rectf)
		rectf= rr->rectf;
	else {
		if(rr->rect32)
			return;
		else {
			if(rr->renlay==NULL || rr->renlay->rectf==NULL) return;
			rectf= rr->renlay->rectf;
		}
	}
	if(rectf==NULL) return;
	
	rectf+= 4*(rr->rectx*ymin + xmin);
	rectc= (char *)(ibuf->rect + ibuf->x*rymin + rxmin);

	for(y1= 0; y1<ymax; y1++) {
		float *rf= rectf;
		char *rc= rectc;
		
		/* XXX temp. because crop offset */
		if( rectc >= (char *)(ibuf->rect)) {
			for(x1= 0; x1<xmax; x1++, rf += 4, rc+=4) {
				rc[0]= FTOCHAR(rf[0]);
				rc[1]= FTOCHAR(rf[1]);
				rc[2]= FTOCHAR(rf[2]);
				rc[3]= FTOCHAR(rf[3]);
			}
		}
		rectf += 4*rr->rectx;
		rectc += 4*ibuf->x;
	}
	
	/* make jobs timer to send notifier */
	*(rj->do_update)= 1;
}

static void render_startjob(void *rjv, short *stop, short *do_update)
{
	RenderJob *rj= rjv;
	
	rj->stop= stop;
	rj->do_update= do_update;
	
	if(rj->anim)
		RE_BlenderAnim(rj->re, rj->scene, rj->scene->r.sfra, rj->scene->r.efra, rj->scene->frame_step);
	else
		RE_BlenderFrame(rj->re, rj->scene, rj->scene->r.cfra);
}

/* called by render, check job 'stop' value or the global */
static int render_breakjob(void *rjv)
{
	RenderJob *rj= rjv;
	
	if(G.afbreek)
		return 1;
	if(rj->stop && *(rj->stop))
		return 1;
	return 0;
}

/* catch esc */
static int screen_render_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	/* no running blender, remove handler and pass through */
	if(0==WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C)))
	   return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;
	
	/* running render */
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_RUNNING_MODAL;
			break;
	}
	return OPERATOR_PASS_THROUGH;
}

/* using context, starts job */
static int screen_render_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/* new render clears all callbacks */
	Scene *scene= CTX_data_scene(C);
	Render *re;
	wmJob *steve;
	RenderJob *rj;
	Image *ima;
	
	/* only one job at a time */
	if(WM_jobs_test(CTX_wm_manager(C), scene))
		return OPERATOR_CANCELLED;
	
	/* handle UI stuff */
	WM_cursor_wait(1);

	/* flush multires changes (for sculpt) */
	multires_force_update(CTX_data_active_object(C));
	
	// get editmode results
	// store spare
	// get view3d layer, local layer, make this nice api call to render
	// store spare
	
	/* ensure at least 1 area shows result */
	screen_set_image_output(C);

	/* job custom data */
	rj= MEM_callocN(sizeof(RenderJob), "render job");
	rj->scene= scene;
	rj->win= CTX_wm_window(C);
	rj->anim= RNA_boolean_get(op->ptr, "anim");
	rj->iuser.scene= scene;
	rj->iuser.ok= 1;
	
	/* setup job */
	steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene);
	WM_jobs_customdata(steve, rj, render_freejob);
	WM_jobs_timer(steve, 0.2, NC_SCENE|ND_RENDER_RESULT, 0);
	WM_jobs_callbacks(steve, render_startjob, NULL, NULL);
	
	/* get a render result image, and make sure it is empty */
	ima= BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
	BKE_image_signal(ima, NULL, IMA_SIGNAL_FREE);
	rj->image= ima;
	
	/* setup new render */
	re= RE_NewRender(scene->id.name);
	RE_test_break_cb(re, rj, render_breakjob);
	RE_display_draw_cb(re, rj, image_rect_update);
	rj->re= re;
	G.afbreek= 0;
	
	//	BKE_report in render!
	//	RE_error_cb(re, error_cb);

	WM_jobs_start(steve);
	
	G.afbreek= 0;
	
	WM_cursor_wait(0);
	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_RESULT, scene);

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	return OPERATOR_RUNNING_MODAL;
}


/* contextual render, using current scene, view3d? */
void SCREEN_OT_render(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Render";
	ot->idname= "SCREEN_OT_render";
	
	/* api callbacks */
	ot->invoke= screen_render_invoke;
	ot->modal= screen_render_modal;
	ot->exec= screen_render_exec;
	
	ot->poll= ED_operator_screenactive;
	
	RNA_def_int(ot->srna, "layers", 0, 0, INT_MAX, "Layers", "", 0, INT_MAX);
	RNA_def_boolean(ot->srna, "anim", 0, "Animation", "");
}

/* *********************** cancel render viewer *************** */

static int render_view_cancel_exec(bContext *C, wmOperator *unused)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceImage *sima= sa->spacedata.first;
	
	if(sima->flag & SI_PREVSPACE) {
		sima->flag &= ~SI_PREVSPACE;
		
		if(sima->flag & SI_FULLWINDOW) {
			sima->flag &= ~SI_FULLWINDOW;
			ED_screen_full_prevspace(C);
		}
		else
			ED_area_prevspace(C);
	}
	else if(sima->flag & SI_FULLWINDOW) {
		sima->flag &= ~SI_FULLWINDOW;
		ed_screen_fullarea(C, sa);
	}		
	
	return OPERATOR_FINISHED;
}

void SCREEN_OT_render_view_cancel(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Cancel Render View";
	ot->idname= "SCREEN_OT_render_view_cancel";
	
	/* api callbacks */
	ot->exec= render_view_cancel_exec;
	ot->poll= ED_operator_image_active;
}


/* ****************  Assigning operatortypes to global list, adding handlers **************** */

/* called in spacetypes.c */
void ED_operatortypes_screen(void)
{
	/* generic UI stuff */
	WM_operatortype_append(SCREEN_OT_actionzone);
	WM_operatortype_append(SCREEN_OT_repeat_last);
	WM_operatortype_append(SCREEN_OT_repeat_history);
	WM_operatortype_append(SCREEN_OT_redo_last);
	
	/* screen tools */
	WM_operatortype_append(SCREEN_OT_area_move);
	WM_operatortype_append(SCREEN_OT_area_split);
	WM_operatortype_append(SCREEN_OT_area_join);
	WM_operatortype_append(SCREEN_OT_area_dupli);
	WM_operatortype_append(SCREEN_OT_area_swap);
	WM_operatortype_append(SCREEN_OT_region_split);
	WM_operatortype_append(SCREEN_OT_region_foursplit);
	WM_operatortype_append(SCREEN_OT_region_flip);
	WM_operatortype_append(SCREEN_OT_screen_set);
	WM_operatortype_append(SCREEN_OT_screen_full_area);
	WM_operatortype_append(SCREEN_OT_screenshot);
	WM_operatortype_append(SCREEN_OT_screencast);
	
	/*frame changes*/
	WM_operatortype_append(SCREEN_OT_frame_offset);
	WM_operatortype_append(SCREEN_OT_animation_play);
	
	/* render */
	WM_operatortype_append(SCREEN_OT_render);
	WM_operatortype_append(SCREEN_OT_render_view_cancel);
	
	/* tools shared by more space types */
	WM_operatortype_append(ED_OT_undo);
	WM_operatortype_append(ED_OT_redo);	
	
}

/* called in spacetypes.c */
void ED_keymap_screen(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "Screen", 0, 0);
	
	/* standard timers */
	WM_keymap_add_item(keymap, "SCREEN_OT_animation_play", TIMER0, KM_ANY, KM_ANY, 0);
	
	RNA_int_set(WM_keymap_add_item(keymap, "SCREEN_OT_actionzone", LEFTMOUSE, KM_PRESS, 0, 0)->ptr, "modifier", 0);
	RNA_int_set(WM_keymap_add_item(keymap, "SCREEN_OT_actionzone", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "modifier", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "SCREEN_OT_actionzone", LEFTMOUSE, KM_PRESS, KM_ALT, 0)->ptr, "modifier", 2);
	
	/* screen tools */
	WM_keymap_verify_item(keymap, "SCREEN_OT_area_move", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "SCREEN_OT_area_split", EVT_ACTIONZONE, 0, 0, 0);
	WM_keymap_verify_item(keymap, "SCREEN_OT_area_join", EVT_ACTIONZONE, 0, 0, 0);
	WM_keymap_verify_item(keymap, "SCREEN_OT_area_dupli", EVT_ACTIONZONE, 0, KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "SCREEN_OT_area_swap", EVT_ACTIONZONE, 0, KM_ALT, 0);
	RNA_int_set(WM_keymap_add_item(keymap, "SCREEN_OT_screen_set", RIGHTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "SCREEN_OT_screen_set", LEFTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "delta", -1);
	WM_keymap_add_item(keymap, "SCREEN_OT_screen_full_area", UPARROWKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "SCREEN_OT_screen_full_area", DOWNARROWKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "SCREEN_OT_screen_full_area", SPACEKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "SCREEN_OT_screenshot", F3KEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "SCREEN_OT_screencast", F3KEY, KM_PRESS, KM_ALT, 0);

	 /* tests */
	WM_keymap_add_item(keymap, "SCREEN_OT_region_split", SKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_add_item(keymap, "SCREEN_OT_region_foursplit", SKEY, KM_PRESS, KM_CTRL|KM_ALT|KM_SHIFT, 0);
	
	WM_keymap_verify_item(keymap, "SCREEN_OT_repeat_history", F3KEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "SCREEN_OT_repeat_last", F4KEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SCREEN_OT_region_flip", F5KEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "SCREEN_OT_redo_last", F6KEY, KM_PRESS, 0, 0);

	/* files */
	WM_keymap_add_item(keymap, "FILE_OT_exec", RETKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "FILE_OT_cancel", ESCKEY, KM_PRESS, 0, 0);
	
	/* undo */
	WM_keymap_add_item(keymap, "ED_OT_undo", ZKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "ED_OT_undo", ZKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "ED_OT_redo", ZKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	WM_keymap_add_item(keymap, "ED_OT_redo", ZKEY, KM_PRESS, KM_SHIFT|KM_OSKEY, 0);
						  
	/* render */
	WM_keymap_add_item(keymap, "SCREEN_OT_render", F12KEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SCREEN_OT_render_view_cancel", ESCKEY, KM_PRESS, 0, 0);
	
	/* frame offsets */
	keymap= WM_keymap_listbase(wm, "Frames", 0, 0);
	RNA_int_set(WM_keymap_add_item(keymap, "SCREEN_OT_frame_offset", UPARROWKEY, KM_PRESS, 0, 0)->ptr, "delta", 10);
	RNA_int_set(WM_keymap_add_item(keymap, "SCREEN_OT_frame_offset", DOWNARROWKEY, KM_PRESS, 0, 0)->ptr, "delta", -10);
	RNA_int_set(WM_keymap_add_item(keymap, "SCREEN_OT_frame_offset", LEFTARROWKEY, KM_PRESS, 0, 0)->ptr, "delta", -1);
	RNA_int_set(WM_keymap_add_item(keymap, "SCREEN_OT_frame_offset", RIGHTARROWKEY, KM_PRESS, 0, 0)->ptr, "delta", 1);
	
}

