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
 * Contributor(s): Francisco De La Cruz
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* Takes heavily from editmesh_loopcut.c */

#include <float.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_mesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"


#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_mesh.h"
#include "ED_space_api.h"

#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "mesh_intern.h"

#define VTX_SLIDE_SLIDE_SENS_F 15.0f
#define VTX_SLIDE_SNAP_THRSH    0.3f

/* Cusom VertexSlide Operator data */
typedef struct VertexSlideOp {
	/* Starting Vertex */
	BMVert *start_vtx;
	BMEdge *sel_edge;

	ViewContext *view_context;
	ARegion *active_region;

	/* Draw callback handle */
	void *draw_handle;

	/* Active Object */
	Object *obj;

	/* Are we in slide mode */
	int slide_mode;
	int snap_n_weld;
	int snap_to_end_vtx;
	int snap_to_mid;

	float distance;
	float interp[3];

	/* Edge Frame Count */
	int disk_edges;

	/* Edges */
	BMEdge** edge_frame;

	/* Slide Frame Endpoints */
	float (*vtx_frame)[3];

	/* Mouse Click 2d pos */
	int m_co[2];

} VertexSlideOp;

static void vtx_slide_draw(const bContext *C, ARegion *ar, void *arg);
static int edbm_vert_slide_exec(bContext *C, wmOperator *op);
static void vtx_slide_exit(const bContext *C, wmOperator *op);
static void vtx_slide_set_frame(VertexSlideOp *vso);

static int vtx_slide_init(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMEditSelection *ese = em->bm->selected.first;

	/* Custom data */
	VertexSlideOp *vso;

	const char *header_str = "Vertex Slide: Hover over an edge and left-click to select slide edge. "
	                         "Left-Shift: Midpoint Snap, Left-Alt: Snap, Left-Ctrl: Snap&Weld";

	if (!obedit) {
		BKE_report(op->reports, RPT_ERROR, "Vertex Slide Error: Not object in context");
		return FALSE;
	}

	EDBM_selectmode_flush(em);
	ese = em->bm->selected.first;

	/* Is there a starting vertex  ? */
	if (ese == NULL || ese->htype != BM_VERT) {
		BKE_report(op->reports, RPT_ERROR_INVALID_INPUT, "Vertex Slide Error: Select a (single) vertex");
		return FALSE;
	}

	vso = MEM_callocN(sizeof(VertexSlideOp), "Vertex Slide Operator");
	vso->view_context = MEM_callocN(sizeof(ViewContext), "Vertex Slide View Context");

	op->customdata = vso;

	/* Set the start vertex */
	vso->start_vtx = (BMVert *)ese->ele;

	vso->sel_edge = NULL;

	/* Edges */
	vso->edge_frame = NULL;

	vso->vtx_frame = NULL;

	vso->disk_edges = 0;

	vso->slide_mode = FALSE;

	vso->snap_n_weld = FALSE;

	vso->snap_to_end_vtx = FALSE;

	vso->snap_to_mid = FALSE;

	vso->distance = 0.0f;

	/* Add handler for the vertex sliding */
	WM_event_add_modal_handler(C, op);

	/* Notify the viewport */
	view3d_operator_needs_opengl(C);

	/* Set the drawing region */
	vso->active_region = CTX_wm_region(C);

	/* Set the draw callback */
	vso->draw_handle = ED_region_draw_cb_activate(vso->active_region->type, vtx_slide_draw, vso, REGION_DRAW_POST_VIEW);

	ED_area_headerprint(CTX_wm_area(C), header_str);
	
	em_setup_viewcontext(C, vso->view_context);

	/* Set the object */
	vso->obj = obedit;

	/* Init frame */
	vtx_slide_set_frame(vso);

	/* Tag for redraw */
	ED_region_tag_redraw(vso->active_region);

	return TRUE;
}

static void vtx_slide_confirm(bContext *C, wmOperator *op)
{
	VertexSlideOp *vso = op->customdata;
	BMEditMesh *em = BMEdit_FromObject(vso->obj);
	BMesh* bm = em->bm;

	/* Select new edge */
	BM_edge_select_set(bm, vso->sel_edge, TRUE);

	/* Invoke operator */
	edbm_vert_slide_exec(C, op);

	if(vso->snap_n_weld) {
		BMVert* other = BM_edge_other_vert(vso->sel_edge, vso->start_vtx);
		BM_vert_select_set(bm, other, TRUE);
	
		EDBM_op_callf(em, op, "pointmerge verts=%hv mergeco=%v", BM_ELEM_SELECT, other->co);
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	} else {
		/* Store edit selection of the active vertex, allows other
		 *  ops to run without reselecting */
		EDBM_editselection_store(em, &vso->start_vtx->head);
	}

	EDBM_selectmode_flush(em);
	
	/* NC_GEOM | ND_DATA & Retess */
	EDBM_update_generic(C, em, TRUE);
	
	ED_region_tag_redraw(vso->active_region);
}

static void vtx_slide_exit(const bContext *C, wmOperator *op)
{
	/* Fetch custom data */
	VertexSlideOp *vso = op->customdata;

	/* Clean-up the custom data */
	ED_region_draw_cb_exit(vso->active_region->type, vso->draw_handle);

	/* Free Custom Data
	 *
	 */
	MEM_freeN(vso->view_context);

	vso->view_context = NULL;

	if (vso->edge_frame) {
		MEM_freeN(vso->edge_frame);
	}

	if(vso->vtx_frame) {
		MEM_freeN(vso->vtx_frame);
	}

	vso->edge_frame = NULL;

	vso->vtx_frame = NULL;

	vso->slide_mode = FALSE;

	MEM_freeN(vso);
	vso = NULL;

	/* Clear the header */
	ED_area_headerprint(CTX_wm_area(C), NULL);
}

static void vtx_slide_draw(const bContext *C, ARegion *UNUSED(ar), void *arg)
{
	VertexSlideOp *vso = arg;

	/* Have an edge to draw */
	if (vso && vso->sel_edge) {
		/* Get 3d view */
		View3D *view3d = CTX_wm_view3d(C);
		int outline_w = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 1;
		int i = 0;
		if (view3d && view3d->zbuf)
			glDisable(GL_DEPTH_TEST);

		glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT | GL_POINT_BIT);

		glPushMatrix();
		glMultMatrixf(vso->obj->obmat);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		/* Draw selected edge
		 * Add color offset and reduce alpha */
		UI_ThemeColorShadeAlpha(TH_EDGE_SELECT, 50, -50);

		glLineWidth(outline_w);

		glBegin(GL_LINES);
		bglVertex3fv(vso->sel_edge->v1->co);
		bglVertex3fv(vso->sel_edge->v2->co);
		glEnd();

		if (vso->slide_mode && vso->disk_edges > 0) {
			/* Draw intermediate edge frame */
			UI_ThemeColorShadeAlpha(TH_EDGE_SELECT, 50, -50);

			for (i = 0; i < vso->disk_edges; i++) {
				glBegin(GL_LINES);
				glVertex3fv(vso->vtx_frame[i]);
				glVertex3fv(vso->interp);
				glEnd();
			}
		}

		if (vso->slide_mode) {
			/* Draw interpolated vertex */
			int pt_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 2;
			UI_ThemeColorShadeAlpha(TH_FACE_DOT, -90, -50);

			glPointSize(pt_size);

			bglBegin(GL_POINTS);
			bglVertex3fv(vso->interp);
			bglEnd();
		}

		glDisable(GL_BLEND);
		glPopMatrix();
		glPopAttrib();

		if (view3d && view3d->zbuf)
			glEnable(GL_DEPTH_TEST);
	}
}

static BMEdge* vtx_slide_nrst_in_frame(VertexSlideOp *vso, const float mval[2])
{
	BMEdge* cl_edge = NULL;
	if (vso->disk_edges > 0) {
		int i = 0;
		BMEdge* edge = NULL;
		
		float v1_proj[3], v2_proj[3];
		float dist = 0;
		float min_dist = FLT_MAX;
		for (i = 0; i < vso->disk_edges; i++) {
			edge = vso->edge_frame[i];
			project_float_noclip(vso->active_region, edge->v1->co, v1_proj);
			project_float_noclip(vso->active_region, edge->v2->co, v2_proj);
			dist = dist_to_line_segment_v2(mval, v1_proj, v2_proj);
			if (dist < min_dist) {
				min_dist = dist;
				cl_edge = edge;
			}
		}
	}
	return cl_edge;
}

static void vtx_slide_find_edge(VertexSlideOp *vso, wmEvent *event)
{
	/* Nearest edge */
	BMEdge *nst_edge = NULL;

	/* Temp Vtx */
	BMVert *start_vtx = vso->start_vtx;

	float mval_float[] = { (float)event->mval[0], (float)event->mval[1]};

	/* Set mouse coords */
	vso->view_context->mval[0] = event->mval[0];
	vso->view_context->mval[1] = event->mval[1];

	/* Find nearest edge */
	nst_edge = vtx_slide_nrst_in_frame(vso, mval_float);

	if (nst_edge) {
		/* Find a connected edge */
		if (nst_edge->v1 == start_vtx || nst_edge->v2 == start_vtx) {
			/* Save mouse coords */
			vso->m_co[0] = event->mval[0];
			vso->m_co[1] = event->mval[1];

			/* Set edge */
			vso->sel_edge = nst_edge;
		}
	}
}

/* Updates the status of the operator - Invoked on mouse movement */
static void vtx_slide_update(VertexSlideOp *vso, wmEvent *event)
{
	BMEdge *edge;
	float edge_other_proj[3];
	float start_vtx_proj[3];
	BMVert *other;
	/* Find nearest edge */
	edge = vso->sel_edge;

	if (edge) {
		float interp[3];

		/* Calculate interpolation value for preview */
		float t_val;

		float mval_float[] = { (float)event->mval[0], (float)event->mval[1]};
		float closest_2d[2];

		other = BM_edge_other_vert(edge, vso->start_vtx);

		/* Project points onto screen and do interpolation in 2D */
		project_float_noclip(vso->active_region, vso->start_vtx->co, start_vtx_proj);

		project_float_noclip(vso->active_region, other->co, edge_other_proj);

		closest_to_line_v2(closest_2d, mval_float, start_vtx_proj, edge_other_proj);

		t_val = line_point_factor_v2(closest_2d, start_vtx_proj, edge_other_proj);

		/* Snap to mid */
		if (vso->snap_to_mid) {
			t_val = 0.5f;
		}

		/* Interpolate preview vertex 3D */
		interp_v3_v3v3(interp, vso->start_vtx->co, other->co, t_val);
		copy_v3_v3(vso->interp, interp);

		vso->distance = t_val;

		/* If snapping */
		if (vso->snap_to_end_vtx) {
			int start_at_v1 = edge->v1 == vso->start_vtx;
			float v1_d = len_v3v3(vso->interp, edge->v1->co);
			float v2_d = len_v3v3(vso->interp, edge->v2->co);

			if (v1_d > v2_d && v2_d < VTX_SLIDE_SNAP_THRSH) {
				copy_v3_v3(vso->interp, edge->v2->co);

				if (start_at_v1)
					vso->distance = 1.0f;
				else
					vso->distance = 0.0f;
			}
			if (v2_d > v1_d && v1_d < VTX_SLIDE_SNAP_THRSH) {
				copy_v3_v3(vso->interp, edge->v1->co);
				if (start_at_v1)
					vso->distance = 0.0f;
				else
					vso->distance = 1.0f;
			}
		}
	}
}

/* Sets the outline frame */
static void vtx_slide_set_frame(VertexSlideOp *vso)
{
	BMEdge *edge;
	float (*vtx_frame)[3] = NULL;
	BMEdge** edge_frame = NULL;
	BLI_array_declare(vtx_frame);
	BLI_array_declare(edge_frame);
	BMIter iter;
	BMEditMesh *em = BMEdit_FromObject(vso->obj);
	BMesh *bm = em->bm;
	BMVert *sel_vtx = vso->start_vtx;
	int idx = 0;

	vso->disk_edges = 0;

	if (vso->edge_frame) {
		MEM_freeN(vso->edge_frame);
		vso->edge_frame = NULL;
	}

	if (vso->vtx_frame) {
		MEM_freeN(vso->vtx_frame);
		vso->vtx_frame = NULL;
	}

	/* Iterate over edges of vertex and copy them */
	BM_ITER_INDEX(edge, &iter, bm, BM_EDGES_OF_VERT, sel_vtx, idx)
	{
		BLI_array_growone(vtx_frame);

		if (sel_vtx == edge->v1)
			copy_v3_v3(vtx_frame[idx], edge->v2->co);
		else
			copy_v3_v3(vtx_frame[idx], edge->v1->co);

		BLI_array_append(edge_frame, edge);
		vso->disk_edges++;
	}

	vso->edge_frame = edge_frame;
	vso->vtx_frame = vtx_frame;

	/* Set the interp at starting vtx */
	copy_v3_v3(vso->interp, sel_vtx->co);
}

static int edbm_vert_slide_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	VertexSlideOp *vso = op->customdata;

	/* Notify the viewport */
	view3d_operator_needs_opengl(C);

	switch (event->type) {
		case LEFTSHIFTKEY:
		{
			switch (event->val) {
				case KM_PRESS:
					vso->snap_to_mid = TRUE;
					break;
				case KM_RELEASE:
					vso->snap_to_mid = FALSE;
					break;
			}

			break;
		}
		case LEFTCTRLKEY:
		{
			switch (event->val) {
				case KM_PRESS:
					vso->snap_n_weld = TRUE;
					vso->snap_to_end_vtx = TRUE;
					break;
				case KM_RELEASE:
					vso->snap_n_weld = FALSE;
					vso->snap_to_end_vtx = FALSE;
					break;
			}

			break;
		}
		case LEFTALTKEY:
		{
			switch (event->val) {
				case KM_PRESS:
					vso->snap_to_end_vtx = TRUE;
					break;
				case KM_RELEASE:
					vso->snap_to_end_vtx = FALSE;
					break;
			}

			break;
		}
		case RIGHTMOUSE:
		{
			/* Enforce redraw */
			ED_region_tag_redraw(vso->active_region);

			/* Clean-up */
			vtx_slide_exit(C, op);

			return OPERATOR_CANCELLED;
		}
		case LEFTMOUSE:
		{
			if (event->val == KM_PRESS) {
				/* Update mouse coords */
				copy_v2_v2_int(vso->m_co, event->mval);

				if (vso->slide_mode) {
					vtx_slide_confirm(C, op);
					/* Clean-up */
					vtx_slide_exit(C, op);
					return OPERATOR_FINISHED;
				}
				else if (vso->sel_edge) {
					vso->slide_mode = TRUE;
				}
			}

			ED_region_tag_redraw(vso->active_region);
			break;

		}
		case MOUSEMOVE:
		{
			if (!vso->slide_mode) {
				vtx_slide_find_edge(vso, event);
			}
			else {
				vtx_slide_update(vso, event);
			}

			ED_region_tag_redraw(vso->active_region);
			break;
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

static int edbm_vert_slide_cancel(bContext *C, wmOperator *op)
{
	/* Exit the modal */
	vtx_slide_exit(C, op);

	return OPERATOR_CANCELLED;
}

static int edbm_vert_slide_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	/* Initialize the operator */
	if (vtx_slide_init(C, op))
		return OPERATOR_RUNNING_MODAL;
	else
		return OPERATOR_CANCELLED;
}

/* Vertex Slide */
static int edbm_vert_slide_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMesh *bm = em->bm;
	BMVert *start_vert;
	BMOperator bmop;
	BMEditSelection *ese = em->bm->selected.first;

	float distance_t = 0.0f;

	/* Invoked modally? */
	if (op->type->modal == edbm_vert_slide_modal && op->customdata) {
		VertexSlideOp *vso = op->customdata;
		if (bm->totedgesel > 1) {
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			BM_edge_select_set(bm, vso->sel_edge, TRUE);
			EDBM_editselection_store(em, &vso->sel_edge->head);
			ese = em->bm->selected.first;
		}
		distance_t = vso->distance;
		RNA_float_set(op->ptr, "distance_t", distance_t);
	}
	else {
		/* Get Properties */
		distance_t = RNA_float_get(op->ptr, "distance_t");
	}

	/* Is there a starting vertex  ? */
	if ((ese == NULL) || (ese->htype != BM_VERT && ese->htype != BM_EDGE)) {
		BKE_report(op->reports, RPT_ERROR_INVALID_INPUT, "Vertex Slide Error: Select a (single) vertex");
		return OPERATOR_CANCELLED;
	}

	start_vert = (BMVert *)ese->ele;

	/* Prepare operator */
	if (!EDBM_op_init(em, &bmop, op, "vertslide vert=%e edge=%hfev distance_t=%f", start_vert, BM_ELEM_SELECT, distance_t))  {
		return OPERATOR_CANCELLED;
	}
	/* Execute operator */
	BMO_op_exec(bm, &bmop);

	/* Select the edge */
	BMO_slot_buffer_hflag_enable(bm, &bmop, "vertout", BM_ALL, BM_ELEM_SELECT, TRUE);

	/* Flush the select buffers */
	EDBM_selectmode_flush(em);

	if (!EDBM_op_finish(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	/* Update Geometry */
	EDBM_update_generic(C, em, TRUE);

	return OPERATOR_FINISHED;
}

void MESH_OT_vert_slide(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Vertex Slide";
	ot->idname = "MESH_OT_vert_slide";
	ot->description = "Vertex slide";

	/* api callback */
	ot->invoke = edbm_vert_slide_invoke;
	ot->modal = edbm_vert_slide_modal;
	ot->cancel = edbm_vert_slide_cancel;
	ot->poll = ED_operator_editmesh_region_view3d;

	/* ot->exec = edbm_vert_slide_exec;
	 * ot->poll = ED_operator_editmesh; */

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* Properties for vertex slide */
	prop = RNA_def_float(ot->srna, "distance_t", 0.0f, -FLT_MAX, FLT_MAX, "Distance", "Distance", -5.0f, 5.0f);
	RNA_def_property_ui_range(prop, -5.0f, 5.0f, 0.1, 4);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
