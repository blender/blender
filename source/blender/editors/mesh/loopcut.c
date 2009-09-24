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
 * Contributor(s): Joseph Eagar, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <float.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h" /*for WM_operator_pystring */
#include "BLI_editVert.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
#include "BKE_mesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h" /* for paint cursor */

#include "IMB_imbuf_types.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_space_api.h"
#include "ED_view3d.h"
#include "ED_mesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "mesh_intern.h"

/* ringsel operator */

/* struct for properties used while drawing */
typedef struct tringselOpData {
	ARegion *ar;		/* region that ringsel was activated in */
	void *draw_handle;	/* for drawing preview loop */
	
	float (*edges)[2][3];
	int totedge;

	ViewContext vc;

	Object *ob;
	EditMesh *em;
	EditEdge *eed;

	int extend;
	int do_cut;
} tringselOpData;

/* modal loop selection drawing callback */
static void ringsel_draw(const bContext *C, ARegion *ar, void *arg)
{
	int i;
	tringselOpData *lcd = arg;
	
	glDisable(GL_DEPTH_TEST);

	glPushMatrix();
	glMultMatrixf(lcd->ob->obmat);

	glColor3ub(255, 0, 255);
	glBegin(GL_LINES);
	for (i=0; i<lcd->totedge; i++) {
		glVertex3fv(lcd->edges[i][0]);
		glVertex3fv(lcd->edges[i][1]);
	}
	glEnd();

	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

static void edgering_sel(tringselOpData *lcd, int previewlines, int select)
{
	EditMesh *em = lcd->em;
	EditEdge *startedge = lcd->eed;
	EditEdge *eed;
	EditFace *efa;
	EditVert *v[2][2];
	float (*edges)[2][3] = NULL;
	V_DYNDECLARE(edges);
	float co[2][3];
	int looking=1, i, tot=0;
	
	if (!startedge)
		return;

	if (lcd->edges) {
		MEM_freeN(lcd->edges);
		lcd->edges = NULL;
		lcd->totedge = 0;
	}

	if (!lcd->extend) {
		EM_clear_flag_all(lcd->em, SELECT);
	}

	/* in eed->f1 we put the valence (amount of faces in edge) */
	/* in eed->f2 we put tagged flag as correct loop */
	/* in efa->f1 we put tagged flag as correct to select */

	for(eed= em->edges.first; eed; eed= eed->next) {
		eed->f1= 0;
		eed->f2= 0;
	}

	for(efa= em->faces.first; efa; efa= efa->next) {
		efa->f1= 0;
		if(efa->h==0) {
			efa->e1->f1++;
			efa->e2->f1++;
			efa->e3->f1++;
			if(efa->e4) efa->e4->f1++;
		}
	}
	
	// tag startedge OK
	startedge->f2= 1;
	
	while(looking) {
		looking= 0;
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->e4 && efa->f1==0 && efa->h == 0) {	// not done quad
				if(efa->e1->f1<=2 && efa->e2->f1<=2 && efa->e3->f1<=2 && efa->e4->f1<=2) { // valence ok

					// if edge tagged, select opposing edge and mark face ok
					if(efa->e1->f2) {
						efa->e3->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
					else if(efa->e2->f2) {
						efa->e4->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
					if(efa->e3->f2) {
						efa->e1->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
					if(efa->e4->f2) {
						efa->e2->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
				}
			}
		}
	}
	
	if(previewlines > 0 && !select){
			for(efa= em->faces.first; efa; efa= efa->next) {
				if(efa->v4 == NULL) {  continue; }
				if(efa->h == 0){
					if(efa->e1->f2 == 1){
						if(efa->e1->h == 1 || efa->e3->h == 1 )
							continue;
						
						v[0][0] = efa->v1;
						v[0][1] = efa->v2;
						v[1][0] = efa->v4;
						v[1][1] = efa->v3;
					} else if(efa->e2->f2 == 1){
						if(efa->e2->h == 1 || efa->e4->h == 1)
							continue;
						v[0][0] = efa->v2;
						v[0][1] = efa->v3;
						v[1][0] = efa->v1;
						v[1][1] = efa->v4;					
					} else { continue; }
										  
					for(i=1;i<=previewlines;i++){
						co[0][0] = (v[0][1]->co[0] - v[0][0]->co[0])*(i/((float)previewlines+1))+v[0][0]->co[0];
						co[0][1] = (v[0][1]->co[1] - v[0][0]->co[1])*(i/((float)previewlines+1))+v[0][0]->co[1];
						co[0][2] = (v[0][1]->co[2] - v[0][0]->co[2])*(i/((float)previewlines+1))+v[0][0]->co[2];

						co[1][0] = (v[1][1]->co[0] - v[1][0]->co[0])*(i/((float)previewlines+1))+v[1][0]->co[0];
						co[1][1] = (v[1][1]->co[1] - v[1][0]->co[1])*(i/((float)previewlines+1))+v[1][0]->co[1];
						co[1][2] = (v[1][1]->co[2] - v[1][0]->co[2])*(i/((float)previewlines+1))+v[1][0]->co[2];					
						
						V_GROW(edges);
						VECCOPY(edges[tot][0], co[0]);
						VECCOPY(edges[tot][1], co[1]);
						tot++;
					}
				}
			}
	} else {
		select = (startedge->f & SELECT) == 0;

		/* select the edges */
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->f2) EM_select_edge(eed, select);
		}
	}

	lcd->edges = edges;
	lcd->totedge = tot;
}

static void ringsel_find_edge(tringselOpData *lcd, const bContext *C, ARegion *ar)
{
	if (lcd->eed)
		edgering_sel(lcd, 1, 0);
}

static void ringsel_finish(bContext *C, wmOperator *op)
{
	tringselOpData *lcd= op->customdata;

	if (lcd->eed) {
		edgering_sel(lcd, 0, 1);
		if (lcd->do_cut) {
			EditMesh *em = BKE_mesh_get_editmesh(lcd->ob->data);
			esubdivideflag(lcd->ob, em, SELECT, 0.0f, 
			               0.0f, 0, 1, SUBDIV_SELECT_LOOPCUT);
		}
	}
}

/* called when modal loop selection is done... */
static void ringsel_exit (bContext *C, wmOperator *op)
{
	tringselOpData *lcd= op->customdata;

	/* deactivate the extra drawing stuff in 3D-View */
	ED_region_draw_cb_exit(lcd->ar->type, lcd->draw_handle);
	
	if (lcd->edges)
		MEM_freeN(lcd->edges);

	ED_region_tag_redraw(lcd->ar);

	/* free the custom data */
	MEM_freeN(lcd);
	op->customdata= NULL;
}

/* called when modal loop selection gets set up... */
static int ringsel_init (bContext *C, wmOperator *op, int do_cut)
{
	tringselOpData *lcd;
	
	/* alloc new customdata */
	lcd= op->customdata= MEM_callocN(sizeof(tringselOpData), "ringsel Modal Op Data");
	
	/* assign the drawing handle for drawing preview line... */
	lcd->ar= CTX_wm_region(C);
	lcd->draw_handle= ED_region_draw_cb_activate(lcd->ar->type, ringsel_draw, lcd, REGION_DRAW_POST);
	lcd->ob = CTX_data_edit_object(C);
	lcd->em= BKE_mesh_get_editmesh((Mesh *)lcd->ob->data);
	lcd->extend = do_cut ? 0 : RNA_boolean_get(op->ptr, "extend");
	lcd->do_cut = do_cut;
	em_setup_viewcontext(C, &lcd->vc);

	ED_region_tag_redraw(lcd->ar);

	return 1;
}

static int ringsel_cancel (bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	ringsel_exit(C, op);
	return OPERATOR_CANCELLED;
}

static int ringsel_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	tringselOpData *lcd;
	EditEdge *edge;
	int dist = 75;

	view3d_operator_needs_opengl(C);

	if (!ringsel_init(C, op, 0))
		return OPERATOR_CANCELLED;
	
	/* add a modal handler for this operator - handles loop selection */
	WM_event_add_modal_handler(C, op);

	lcd = op->customdata;
	lcd->vc.mval[0] = evt->mval[0];
	lcd->vc.mval[1] = evt->mval[1];
	
	edge = findnearestedge(&lcd->vc, &dist);
	if (edge != lcd->eed) {
		lcd->eed = edge;
		ringsel_find_edge(lcd, C, lcd->ar);
	}

	return OPERATOR_RUNNING_MODAL;
}


static int ringcut_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	tringselOpData *lcd;
	EditEdge *edge;
	int dist = 75;

	view3d_operator_needs_opengl(C);

	if (!ringsel_init(C, op, 1))
		return OPERATOR_CANCELLED;
	
	/* add a modal handler for this operator - handles loop selection */
	WM_event_add_modal_handler(C, op);

	lcd = op->customdata;
	lcd->vc.mval[0] = evt->mval[0];
	lcd->vc.mval[1] = evt->mval[1];
	
	edge = findnearestedge(&lcd->vc, &dist);
	if (edge != lcd->eed) {
		lcd->eed = edge;
		ringsel_find_edge(lcd, C, lcd->ar);
	}

	return OPERATOR_RUNNING_MODAL;
}

static int ringsel_modal (bContext *C, wmOperator *op, wmEvent *event)
{
	tringselOpData *lcd= op->customdata;

	view3d_operator_needs_opengl(C);

	switch (event->type) {
		case RIGHTMOUSE:
		case LEFTMOUSE: /* confirm */ // XXX hardcoded
			if (event->val == KM_RELEASE) {
				/* finish */
				ED_region_tag_redraw(lcd->ar);

				ringsel_finish(C, op);
				ringsel_exit(C, op);
				
				return OPERATOR_FINISHED;
			}

			ED_region_tag_redraw(lcd->ar);
			break;
		case MOUSEMOVE: { /* mouse moved somewhere to select another loop */
			int dist = 75;
			EditEdge *edge;

			lcd->vc.mval[0] = event->mval[0];
			lcd->vc.mval[1] = event->mval[1];
			edge = findnearestedge(&lcd->vc, &dist);

			if (edge != lcd->eed) {
				lcd->eed = edge;
				ringsel_find_edge(lcd, C, lcd->ar);
			}

			ED_region_tag_redraw(lcd->ar);
			break;
		}
	}
	
	/* keep going until the user confirms */
	return OPERATOR_RUNNING_MODAL;
}

void MESH_OT_edgering_select (wmOperatorType *ot)
{
	/* description */
	ot->name= "Edge Ring Select";
	ot->idname= "MESH_OT_edgering_select";
	ot->description= "Select an edge ring";
	
	/* callbacks */
	ot->invoke= ringsel_invoke;
	ot->modal= ringsel_modal;
	ot->cancel= ringsel_cancel;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
}

void MESH_OT_loopcut (wmOperatorType *ot)
{
	/* description */
	ot->name= "Loop Cut";
	ot->idname= "MESH_OT_loopcut";
	ot->description= "Add a new loop between existing loops.";
	
	/* callbacks */
	ot->invoke= ringcut_invoke;
	ot->modal= ringsel_modal;
	ot->cancel= ringsel_cancel;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}
